/*----------------------------------------------------------------------------+-
jk-https-server.cpp
-+----------------------------------------------------------------------------+-
Description : PQC(mTLS) HTTPS 인증 서버 콘솔 프로그램
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
  [2026/05/14] --kem 옵션 파싱 및 KEM 그룹 시작 로그 추가
  [2026/05/15] 이중 인증서용 SetEcdsaCertPaths 호출 추가
-+----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
// [JKC:20260428-1034] OPENSSL_Applink 문제 해결 (DLL 방식 OpenSSL 사용시에만 필요)
// Notes: 반드시 다른 OpenSSL 헤더보다 우선하여 include
#include <openssl/applink.c>

#include "https.h"
/*----------------------------------------------------------------------------+-
[18080] PQC(mTLS) HTTPS 인증 서버
-+----------------------------------------------------------------------------+-
- 목적
  현재 OpenSSL 3.5.4 버전에서, OQS Provider를 활용한 PQC(mTLS) HTTPS 인증 서버 구현
- 기능
  
-+----------------------------------------------------------------------------*/
int main(int a_iArgc, char** a_ppszArgv)
{
  std::string         l_strKemOpt;
  std::vector<char*>  l_posArgs;
  bool                l_bDualCert = true;   // `이중 인증서` 사용 여부 (기본값: true="ML-DSA + ECDSA")
  bool                l_bmTLS     = true;   // `mTLS` 사용 여부 (기본값: true=mTLS, false=TLS)
  // -------------------------------------
  // 콘솔 옵션 파싱
  // -------------------------------------
  for (int l_i = 1; l_i < a_iArgc; ++l_i)
  {
    if (::strcmp(a_ppszArgv[l_i], "--help") == 0)
    {
      printf("Usage: jk-https-server [OPTIONS]\n");
      printf("  --kem <groups>  사용할 KEM 그룹 목록 (예: X25519MLKEM768:SecP256r1MLKEM768:X25519)\n");
      printf("  --single-cert   ML-DSA 인증서만 수용 (이중 인증서 비활성화)\n");
      printf("  --no-mtls       일반 TLS 동작 (클라이언트 인증서 불필요)\n");
      printf("  --help          도움말 출력\n");
      return 0;
    }
    else if (::strcmp(a_ppszArgv[l_i], "--single-cert") == 0)
      l_bDualCert = false;
    else if (::strcmp(a_ppszArgv[l_i], "--no-mtls") == 0)
      l_bmTLS     = false;
    else if (strcmp(a_ppszArgv[l_i], "--kem") == 0)
    {
      if ((l_i + 1) >= a_iArgc)
      {
        printf("[ERR_] --kem 옵션에 값이 필요합니다.\n");
        return 1;
      }
      l_strKemOpt = a_ppszArgv[++l_i];
    }
    else
    {
      l_posArgs.push_back(a_ppszArgv[l_i]);
    }
  }
  const int         l_iServerPort = (static_cast<int>(l_posArgs.size()) > 0) ? atoi(l_posArgs[0]) : 18080;
  const std::string l_strCaPath   = (static_cast<int>(l_posArgs.size()) > 1) ? l_posArgs[1] : "certs/ca/ca-cert.pem";
  const std::string l_strCertPath = (static_cast<int>(l_posArgs.size()) > 2) ? l_posArgs[2] : "certs/server/server-cert.pem";
  const std::string l_strKeyPath  = (static_cast<int>(l_posArgs.size()) > 3) ? l_posArgs[3] : "certs/server/server-key.pem";

  CHttpsServer l_oServer;
  l_oServer.SetCertPaths(l_strCaPath, l_strCertPath, l_strKeyPath);
  // -------------------------------------
  // [JKC:20260516-1205] 외부(콘솔옵션) `mTLS` 사용 여부 적용
  // [JKC:20260516-0740] 외부(콘솔옵션) `이중 인증서` 사용 여부를 최우선 적용
  // -------------------------------------
  if (l_bDualCert)
  {
    l_oServer.SetEcdsaCertPaths(
      "certs/ecdsa/ca/ca-cert.pem",
      "certs/ecdsa/server/server-cert.pem",
      "certs/ecdsa/server/server-key.pem");
  }
  if (!l_oServer.Init(NULL, l_strKemOpt, l_bDualCert, l_bmTLS))
  {
    return 1;
  }
  printf("[INFO] Dual Cert    : %s\n", l_bDualCert ? "ML-DSA + ECDSA"  : "ML-DSA only");
  printf("[INFO] mTLS         : %s\n", l_bmTLS     ? "enabled"         : "disabled (TLS only)");
  printf("[INFO] KEM Groups   : %s\n", l_oServer.GetKemGroups().c_str());
  // -------------------------------------
  // 서버 실행
  // -------------------------------------
  if (!l_oServer.Run(l_iServerPort))
  {
    return 1;
  }
  // -------------------------------------
  return 0;
}
// -----------------------------------------------------------------------------
