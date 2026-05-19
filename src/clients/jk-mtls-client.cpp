/*----------------------------------------------------------------------------+-
jk-mtls-client.cpp
-+----------------------------------------------------------------------------+-
Description : PQC(mTLS) TCP 인증 클라이언트 콘솔 프로그램
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/27] 최초 작성
  [2026/05/14] --kem 옵션 파싱, KEM 그룹 시작 로그, SSL_CTX_set1_groups_list 오류 메시지
-+----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
// [JKC:20260428-1034] OPENSSL_Applink 문제 해결 (DLL 방식 OpenSSL 사용시에만 필요)
// Notes: 반드시 다른 OpenSSL 헤더보다 우선하여 include
#include <openssl/applink.c>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "pqc_utils.h"

static int sfInitWinsock()
{
  WSADATA l_tWsaData = { 0 };
  return WSAStartup(MAKEWORD(2, 2), &l_tWsaData);
}
/*----------------------------------------------------------------------------+-
PQC(mTLS) TCP 인증 클라이언트
-+----------------------------------------------------------------------------+-
- 목적
  현재 OpenSSL 3.5.4 버전에서, OQS Provider를 활용한 PQC(mTLS) TCP 인증 클라이언트 구현 및 테스트
- 구현
  로컬 8443 포트로 서버 접속, SSL 핸드쉐이크 후 서버로부터 메시지 수신
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
  const std::string l_strDefaultKem = "X25519MLKEM768:SecP256r1MLKEM768:X25519";
  const std::string l_strKemGroups  = (l_strKemOpt.size() > 0) ? l_strKemOpt : l_strDefaultKem;

  const char* l_cpszServerIp    = (static_cast<int>(l_posArgs.size()) > 0) ? l_posArgs[0] : "127.0.0.1";
  const char* l_cpszServerPort  = (static_cast<int>(l_posArgs.size()) > 1) ? l_posArgs[1] : "8443";
  const char* l_cpszCaPath      = (static_cast<int>(l_posArgs.size()) > 2) ? l_posArgs[2] : "certs/ca/ca-cert.pem";
  const char* l_cpszCertPath    = (static_cast<int>(l_posArgs.size()) > 3) ? l_posArgs[3] : "certs/client/client-cert.pem";
  const char* l_cpszKeyPath     = (static_cast<int>(l_posArgs.size()) > 4) ? l_posArgs[4] : "certs/client/client-key.pem";

  SSL_CTX*    l_pSslCtx = NULL;
  SSL*        l_pSsl    = NULL;
  SOCKET      l_hSock   = INVALID_SOCKET;
  sockaddr_in l_tAddr   = { 0 };
  int       l_iResult   = 1;
  // -------------------------------------
  // OpenSSL 초기화 및 SSL_CTX 설정
  // -------------------------------------
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
  // -------------------------------------
  // OQS Provider 로드
  // -------------------------------------
  if (!gfLoadProvider4OQS(NULL))
  {
    goto FINALIZE;
  }
  // -------------------------------------
  // SSL_CTX 생성 및 설정
  // -------------------------------------
  l_pSslCtx = SSL_CTX_new(TLS_client_method());
  if (l_pSslCtx == NULL)
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  // -----------------
  // 서버 인증서 검증을 위해, CA 인증서 로드
  // -----------------
  SSL_CTX_set_verify(l_pSslCtx, SSL_VERIFY_PEER, NULL);
  if (SSL_CTX_load_verify_locations(l_pSslCtx, l_cpszCaPath, NULL) != 1)
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  // -----------------
  // 클라이언트 인증서 설정
  // -----------------
  if (SSL_CTX_use_certificate_file(l_pSslCtx, l_cpszCertPath, SSL_FILETYPE_PEM) != 1)
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  // -----------------
  // 클라이언트 키 설정
  // -----------------
  if (SSL_CTX_use_PrivateKey_file(l_pSslCtx, l_cpszKeyPath, SSL_FILETYPE_PEM) != 1)
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  printf("KEM Groups  : %s\n", l_strKemGroups.c_str());
  // -----------------
  // Hybrid KEM 그룹 설정 (서버와 동일한 그룹 사용)
  // -----------------
  if (SSL_CTX_set1_groups_list(l_pSslCtx, l_strKemGroups.c_str()) != 1)
  {
    fprintf(stderr, "[ERR_] SSL_CTX_set1_groups_list 실패: %s\n", l_strKemGroups.c_str());
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  // -------------------------------------
  // 서버 접속 및 SSL 핸드쉐이크
  // -------------------------------------
  if (sfInitWinsock() != 0)
  {
    fprintf(stderr, "[ERR_] WSAStartup 실패\n");
    goto FINALIZE;
  }

  l_hSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (l_hSock == INVALID_SOCKET)
  {
    fprintf(stderr, "[ERR_] socket 생성 실패\n");
    goto FINALIZE;
  }

  l_tAddr.sin_family = AF_INET;
  l_tAddr.sin_port = htons(static_cast<u_short>(atoi(l_cpszServerPort)));
  if (inet_pton(AF_INET, l_cpszServerIp, &l_tAddr.sin_addr) != 1)
  {
    fprintf(stderr, "[ERR_] 서버 IP 파싱 실패\n");
    goto FINALIZE;
  }
  // -----------------
  // 서버 접속
  // -----------------
  if (connect(l_hSock, reinterpret_cast<sockaddr*>(&l_tAddr), sizeof(l_tAddr)) == SOCKET_ERROR)
  {
    fprintf(stderr, "[ERR_] connect 실패\n");
    goto FINALIZE;
  }
  // -----------------
  // SSL 핸드쉐이크
  // -----------------
  l_pSsl = SSL_new(l_pSslCtx);
  if ((l_pSsl == NULL) || (SSL_set_fd(l_pSsl, static_cast<int>(l_hSock)) != 1))
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  if (SSL_connect(l_pSsl) != 1)
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  // SSL 연결 정보 출력
  gfPrintInfo4SSL(l_pSsl);
  // -----------------
  // 응답 수신
  // -----------------
  char l_cRecvBuffer[1024] = { 0 };
  const int l_iRecvLen = SSL_read(l_pSsl, l_cRecvBuffer, static_cast<int>(sizeof(l_cRecvBuffer) - 1));
  if (l_iRecvLen > 0)
  {
    l_cRecvBuffer[l_iRecvLen] = '\0';
    printf("Server Response: %s\n", l_cRecvBuffer);
  }
  else
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }

  l_iResult = 0;
  // -------------------------------------
  // 접속 종료
  // -------------------------------------
FINALIZE:
  if (l_pSsl != NULL)
  {
    SSL_shutdown(l_pSsl);
    SSL_free(l_pSsl);
  }
  if (l_hSock != INVALID_SOCKET)
  {
    closesocket(l_hSock);
  }
  if (l_pSslCtx != NULL)
  {
    SSL_CTX_free(l_pSslCtx);
  }
  WSACleanup();
  EVP_cleanup();
  // -------------------------------------
  return l_iResult;
}
