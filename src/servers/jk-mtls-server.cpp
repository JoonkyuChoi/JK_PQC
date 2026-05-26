/*----------------------------------------------------------------------------+-
jk-mtls-server.cpp
-+----------------------------------------------------------------------------+-
Description : PQC(mTLS) TCP 인증 서버 콘솔 프로그램
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/27] 최초 작성
  [2026/05/14] --kem 옵션 파싱, KEM 그룹 시작 로그, SSL_CTX_set1_groups_list 오류 메시지
  [2026/05/14] 다중 클라이언트: accept 루프로 연속 처리
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
#include <openssl/x509.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "pqc_utils.h"

// Winsock 2.2 초기화 (0=성공, 그 외 WSAStartup 오류 코드)
static int sfInitWinsock()
{
  WSADATA l_tWsaData = { 0 };
  return WSAStartup(MAKEWORD(2, 2), &l_tWsaData);
}
/*----------------------------------------------------------------------------+-
[8443] PQC(mTLS) TCP 인증 서버
-+----------------------------------------------------------------------------+-
- 목적
  현재 OpenSSL 3.5.4 버전에서, OQS Provider를 활용한 PQC(mTLS) TCP 인증 서버 구현 및 테스트
- 구현
  단순 TCP 서버로, 클라이언트와 SSL 핸드쉐이크 후 간단한 메시지 전송(연속 accept)
-+----------------------------------------------------------------------------*/
int main(int a_iArgc, char** a_ppszArgv)
{
  const char*         l_cpszServerPort = "8443";
  std::string         l_strKemOpt;
  std::vector<char*>  l_posArgs;
  for (int l_i = 1; l_i < a_iArgc; ++l_i)
  {
    if (strcmp(a_ppszArgv[l_i], "--kem") == 0)
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
  const std::string l_strDefaultKem = "X25519MLKEM768:SecP256r1MLKEM768:X25519";
  const std::string l_strKemGroups  = (l_strKemOpt.size() > 0) ? l_strKemOpt : l_strDefaultKem;

  const char* l_cpszCaPath      = (static_cast<int>(l_posArgs.size()) > 0) ? l_posArgs[0] : "certs/ca/ca-cert.pem";
  const char* l_cpszCertPath    = (static_cast<int>(l_posArgs.size()) > 1) ? l_posArgs[1] : "certs/server/server-cert.pem";
  const char* l_cpszKeyPath     = (static_cast<int>(l_posArgs.size()) > 2) ? l_posArgs[2] : "certs/server/server-key.pem";

  SSL_CTX*    l_pSslCtx     = NULL;
  SOCKET      l_hListenSock = INVALID_SOCKET;
  sockaddr_in l_tAddr       = { 0 };
  int         l_iResult     = 1;
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
  l_pSslCtx = SSL_CTX_new(TLS_server_method());
  if (l_pSslCtx == NULL)
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  // -----------------
  // 클라이언트 인증서 검증을 위해, CA 인증서 로드
  // -----------------
  SSL_CTX_set_verify(l_pSslCtx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
  if (SSL_CTX_load_verify_locations(l_pSslCtx, l_cpszCaPath, NULL) != 1)
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  // -----------------
  // 서버 인증서 및 키 설정
  // -----------------
  if (SSL_CTX_use_certificate_file(l_pSslCtx, l_cpszCertPath, SSL_FILETYPE_PEM) != 1)
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  if (SSL_CTX_use_PrivateKey_file(l_pSslCtx, l_cpszKeyPath, SSL_FILETYPE_PEM) != 1)
  {
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  printf("[INFO] KEM Groups  : %s\n", l_strKemGroups.c_str());
  // -----------------
  // Hybrid KEM 그룹 설정 (클라이언트와 동일한 그룹 사용)
  // -----------------
  if (SSL_CTX_set1_groups_list(l_pSslCtx, l_strKemGroups.c_str()) != 1)
  {
    printf("[ERR_] SSL_CTX_set1_groups_list 실패: %s\n", l_strKemGroups.c_str());
    ERR_print_errors_fp(stderr);
    goto FINALIZE;
  }
  // -------------------------------------
  // Winsock 초기화 및 서버 소켓 설정
  // -------------------------------------
  if (sfInitWinsock() != 0)
  {
    printf("[ERR_] WSAStartup 실패\n");
    goto FINALIZE;
  }
  // 서버 소켓 생성
  l_hListenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (l_hListenSock == INVALID_SOCKET)
  {
    printf("[ERR_] socket 생성 실패\n");
    goto FINALIZE;
  }
  // 바인딩을 위해, 주소 구조체 설정
  l_tAddr.sin_family      = AF_INET;
  l_tAddr.sin_port        = htons(static_cast<u_short>(atoi(l_cpszServerPort)));
  l_tAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  // -------------------------------------
  // 서버 바인딩 및 대기
  // -------------------------------------
  if (bind(l_hListenSock, reinterpret_cast<sockaddr*>(&l_tAddr), sizeof(l_tAddr)) == SOCKET_ERROR)
  {
    printf("[ERR_] bind 실패\n");
    goto FINALIZE;
  }
  if (listen(l_hListenSock, SOMAXCONN) == SOCKET_ERROR)
  {
    printf("[ERR_] listen 실패\n");
    goto FINALIZE;
  }

  printf("[INFO] 서버 대기 중: 0.0.0.0:%s (Ctrl+C 로 종료)\n", l_cpszServerPort);
  // -------------------------------------
  // 연속 접속 처리: accept 루프
  // -------------------------------------
  for (;;)
  {
    SOCKET l_hClientSock = accept(l_hListenSock, NULL, NULL);
    if (l_hClientSock == INVALID_SOCKET)
    {
      printf("[ERR_] accept 실패, 다음 대기로 계속합니다.\n");
      continue;
    }
    SSL* l_pSsl = SSL_new(l_pSslCtx);
    if ((l_pSsl == NULL) || (SSL_set_fd(l_pSsl, static_cast<int>(l_hClientSock)) != 1))
    {
      ERR_print_errors_fp(stderr);
      closesocket(l_hClientSock);
      continue;
    }
    if (SSL_accept(l_pSsl) != 1)
    {
      ERR_print_errors_fp(stderr);
      SSL_free(l_pSsl);
      closesocket(l_hClientSock);
      continue;
    }
    gfPrintInfo4SSL(l_pSsl);
    const char* l_cpszResponse = "Hello from PQC (TCP) Server";
    SSL_write(l_pSsl, l_cpszResponse, static_cast<int>(strlen(l_cpszResponse)));
    l_iResult = 0;
    SSL_shutdown(l_pSsl);
    SSL_free(l_pSsl);
    closesocket(l_hClientSock);
  }
FINALIZE:
  if (l_hListenSock != INVALID_SOCKET)
  {
    closesocket(l_hListenSock);
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
// -----------------------------------------------------------------------------
