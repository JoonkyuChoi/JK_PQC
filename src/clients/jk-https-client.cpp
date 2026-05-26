/*----------------------------------------------------------------------------+-
jk-https-client.cpp
-+----------------------------------------------------------------------------+-
Description : PQC(mTLS) HTTPS 인증 클라이언트 콘솔 프로그램
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
  [2026/05/14] --kem 옵션 파싱 및 KEM 그룹 시작 로그 추가
  [2026/05/26] -h <ip>, -p <port> 콘솔 옵션 추가
  [2026/05/26] --help 콘솔 옵션 및 Usage 출력 추가
-+----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
// [JKC:20260428-1034] OPENSSL_Applink 문제 해결 (DLL 방식 OpenSSL 사용시에만 필요)
// Notes: 반드시 다른 OpenSSL 헤더보다 우선하여 include
#include <openssl/applink.c>
#include <json.hpp>   // nlohmann::json

#include "https.h"
/*----------------------------------------------------------------------------+-
PQC(mTLS) HTTPS 인증 클라이언트
-+----------------------------------------------------------------------------+-
- 목적
  현재 OpenSSL 3.5.4 버전에서, OQS Provider를 활용한 PQC(mTLS) HTTPS 인증 클라이언트 구현
- 구현
  
-+----------------------------------------------------------------------------*/

// CLI 사용법·옵션 설명 출력
static void sfPrintUsage(const char* a_pszProg)
{
  printf("\n");
  printf("  Usage: %s [options] [<ca> <cert> <key>]\n", a_pszProg);
  printf("\n");
  printf("    -h <ip>         접속 호스트 IP (기본값: 127.0.0.1 = IPv4전용)\n");
  printf("    -p <port>       접속 호스트 포트 (기본값: 18080)\n");
  printf("    --kem <groups>  사용할 KEM 그룹 목록 (예: X25519MLKEM768:SecP256r1MLKEM768:X25519)\n");
  printf("    --help, -?      사용법 출력\n");
  printf("\n");
  printf("  위치 인자 (모두 선택):\n");
  printf("    <ca>            CA 인증서 파일 (기본값: certs/ca/ca-cert.pem)\n");
  printf("    <cert>          클라이언트 인증서 파일 (기본값: certs/client/client-cert.pem)\n");
  printf("    <key>           클라이언트 개인키 파일 (기본값: certs/client/client-key.pem)\n");
  printf("\n");
  printf("  하위 호환: -h/-p 미지정 시 첫 두 위치 인자가 host/port 로 사용됩니다.\n");
  printf("\n");
}

int main(int a_iArgc, char** a_ppszArgv)
{
  std::string        l_strKemOpt;
  std::string        l_strHostOpt;        // -h <ip>  (빈 문자열 = 미지정)
  int                l_iPortOpt    = 0;   // -p <port> (0 = 미지정)
  std::vector<char*> l_posArgs;
  for (int l_i = 1; l_i < a_iArgc; ++l_i)
  {
    if ((strcmp(a_ppszArgv[l_i], "--help") == 0) || (strcmp(a_ppszArgv[l_i], "-?") == 0))
    {
      sfPrintUsage(a_ppszArgv[0]);
      return 0;
    }
    else if (strcmp(a_ppszArgv[l_i], "--kem") == 0)
    {
      if ((l_i + 1) >= a_iArgc)
      {
        printf("[ERR_] --kem 옵션에 값이 필요합니다.\n");
        return 1;
      }
      l_strKemOpt = a_ppszArgv[++l_i];
    }
    else if (strcmp(a_ppszArgv[l_i], "-h") == 0)
    {
      if ((l_i + 1) >= a_iArgc)
      {
        printf("[ERR_] -h 옵션에 값이 필요합니다.\n");
        return 1;
      }
      l_strHostOpt = a_ppszArgv[++l_i];
    }
    else if (strcmp(a_ppszArgv[l_i], "-p") == 0)
    {
      if ((l_i + 1) >= a_iArgc)
      {
        printf("[ERR_] -p 옵션에 값이 필요합니다.\n");
        return 1;
      }
      l_iPortOpt = atoi(a_ppszArgv[++l_i]);
    }
    else
    {
      l_posArgs.push_back(a_ppszArgv[l_i]);
    }
  }
  // 우선순위: -h/-p 옵션 > 위치 인자 > 기본값
  const std::string l_strServerHost = !l_strHostOpt.empty()
                                      ? l_strHostOpt
                                      : ((static_cast<int>(l_posArgs.size()) > 0) ? std::string(l_posArgs[0]) : std::string("127.0.0.1"));
  const int         l_iServerPort   = (l_iPortOpt > 0)
                                      ? l_iPortOpt
                                      : ((static_cast<int>(l_posArgs.size()) > 1) ? atoi(l_posArgs[1]) : 18080);
  const std::string l_strCaPath     = (static_cast<int>(l_posArgs.size()) > 2) ? l_posArgs[2] : "certs/ca/ca-cert.pem";
  const std::string l_strCertPath   = (static_cast<int>(l_posArgs.size()) > 3) ? l_posArgs[3] : "certs/client/client-cert.pem";
  const std::string l_strKeyPath    = (static_cast<int>(l_posArgs.size()) > 4) ? l_posArgs[4] : "certs/client/client-key.pem";

  CHttpsClient l_oClient;
  if (!l_oClient.Init(NULL, l_strKemOpt))
  {
    return 1;
  }
  printf("[INFO] KEM Groups  : %s\n", l_oClient.GetKemGroups().c_str());
  printf("[INFO] Target      : %s:%d\n", l_strServerHost.c_str(), l_iServerPort);
  l_oClient.SetCertPaths(l_strCaPath, l_strCertPath, l_strKeyPath);

  std::string l_strResponseBody;
  if (!l_oClient.Get(l_strServerHost, l_iServerPort, l_strResponseBody))
  {
    return 1;
  }
  printf("Server Response: %s\n", l_strResponseBody.c_str());

  if (!l_oClient.Get(l_strServerHost, l_iServerPort, "/api/status", l_strResponseBody))
  {
    return 1;
  }
  printf("[INFO] API /api/status 응답: %s\n", l_strResponseBody.c_str());

  if (!l_oClient.Get(l_strServerHost, l_iServerPort, "/api/session", l_strResponseBody))
  {
    return 1;
  }
  printf("[INFO] API /api/session 응답: %s\n", l_strResponseBody.c_str());

  if (!l_oClient.Get(l_strServerHost, l_iServerPort, "/api/client/info", l_strResponseBody))
  {
    return 1;
  }
  printf("[INFO] API /api/client/info 응답: %s\n", l_strResponseBody.c_str());

  nlohmann::json  l_oJson;
                  l_oJson["message"] = "hello PQC";
  std::string     l_strJson = l_oJson.dump();
  if (!l_oClient.Post(l_strServerHost, l_iServerPort, "/api/echo", l_strJson, "application/json", l_strResponseBody))
  {
    return 1;
  }
  printf("[INFO] API /api/echo 응답: %s\n", l_strResponseBody.c_str());
  return 0;
}
// -----------------------------------------------------------------------------
