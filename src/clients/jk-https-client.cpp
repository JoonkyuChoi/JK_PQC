/*----------------------------------------------------------------------------+-
jk-https-client.cpp
-+----------------------------------------------------------------------------+-
Description : PQC(mTLS) HTTPS 인증 클라이언트 콘솔 프로그램
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
  [2026/05/14] --kem 옵션 파싱 및 KEM 그룹 시작 로그 추가
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
int main(int a_iArgc, char** a_ppszArgv)
{
  std::string        l_strKemOpt;
  std::vector<char*> l_posArgs;
  for (int l_i = 1; l_i < a_iArgc; ++l_i)
  {
    if (strcmp(a_ppszArgv[l_i], "--kem") == 0)
    {
      if ((l_i + 1) >= a_iArgc)
      {
        fprintf(stderr, "[ERR_] --kem 옵션에 값이 필요합니다.\n");
        return 1;
      }
      l_strKemOpt = a_ppszArgv[++l_i];
    }
    else
    {
      l_posArgs.push_back(a_ppszArgv[l_i]);
    }
  }
  const std::string l_strServerHost = (static_cast<int>(l_posArgs.size()) > 0) ? l_posArgs[0] : "127.0.0.1";
  const int         l_iServerPort   = (static_cast<int>(l_posArgs.size()) > 1) ? atoi(l_posArgs[1]) : 18080;
  const std::string l_strCaPath     = (static_cast<int>(l_posArgs.size()) > 2) ? l_posArgs[2] : "certs/ca/ca-cert.pem";
  const std::string l_strCertPath   = (static_cast<int>(l_posArgs.size()) > 3) ? l_posArgs[3] : "certs/client/client-cert.pem";
  const std::string l_strKeyPath    = (static_cast<int>(l_posArgs.size()) > 4) ? l_posArgs[4] : "certs/client/client-key.pem";

  CHttpsClient l_oClient;
  if (!l_oClient.Init(NULL, l_strKemOpt))
  {
    return 1;
  }
  printf("KEM Groups  : %s\n", l_oClient.GetKemGroups().c_str());
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
  printf("[API] /api/status 응답: %s\n", l_strResponseBody.c_str());

  if (!l_oClient.Get(l_strServerHost, l_iServerPort, "/api/session", l_strResponseBody))
  {
    return 1;
  }
  printf("[API] /api/session 응답: %s\n", l_strResponseBody.c_str());

  if (!l_oClient.Get(l_strServerHost, l_iServerPort, "/api/client/info", l_strResponseBody))
  {
    return 1;
  }
  printf("[API] /api/client/info 응답: %s\n", l_strResponseBody.c_str());

  nlohmann::json  l_oJson;
                  l_oJson["message"] = "hello PQC";
  std::string     l_strJson = l_oJson.dump();
  if (!l_oClient.Post(l_strServerHost, l_iServerPort, "/api/echo", l_strJson, "application/json", l_strResponseBody))
  {
    return 1;
  }
  printf("[API] /api/echo 응답: %s\n", l_strResponseBody.c_str());
  return 0;
}
// -----------------------------------------------------------------------------
