/*----------------------------------------------------------------------------+-
pqc_utils.cpp
-+----------------------------------------------------------------------------+-
Description : PQC/OpenSSL 공통 유틸 함수 구현
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/27] 최초 작성
  [2026/05/14] 협상 KEM 출력: SSL_get0_group_name 우선, 없으면 SSL_get_negotiated_group+OBJ
-+----------------------------------------------------------------------------*/
#include "pqc_utils.h"

#include <stdio.h>

#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/x509.h>

// 해당 OpenSSL 컨텍스트에 OQS-Provider를 로드하고, 성공 여부를 리턴한다. (a_pCtx가 NULL이면 기본 라이브러리 컨텍스트에 로드합니다.)
bool gfLoadProvider4OQS(OSSL_LIB_CTX* a_pCtx)
{
  OSSL_PROVIDER* l_pProvider = OSSL_PROVIDER_load(a_pCtx, "oqsprovider");
  if (l_pProvider == NULL)
  {
    printf("[ERR_] oqsprovider 로드 실패\n");
    ERR_print_errors_fp(stderr);
    return false;
  }
  return true;
}

// 해당 SSL 객체에서 현재 사용 중인 Cipher Suite, KEM Group, Peer Certificate Subject 정보를 출력한다.
void gfPrintInfo4SSL(SSL* a_pSsl)
{
  const SSL_CIPHER* l_pCipher = SSL_get_current_cipher(a_pSsl);
  const char* l_cpszCipherName = (l_pCipher != NULL) ? SSL_CIPHER_get_name(l_pCipher) : "N/A";
  printf("[INFO] Cipher Suite : %s\n", l_cpszCipherName);

  // 하이브리드(X25519MLKEM768 등)는 협상 NID가 OBJ에 등록되지 않아 nid2sn이 NULL인 경우가 많다.
  // OpenSSL이 세션에 보관한 TLS 그룹 문자열을 우선 사용하고, 없을 때만 NID→이름으로 폴백한다.
  const char* l_cpszTlsGroupName = SSL_get0_group_name(a_pSsl);
  if ((l_cpszTlsGroupName != NULL) && (l_cpszTlsGroupName[0] != '\0'))
  {
    printf("[INFO] KEM Group    : %s\n", l_cpszTlsGroupName);
  }
  else
  {
    const int l_iNid = SSL_get_negotiated_group(a_pSsl);
    if (l_iNid != NID_undef)
    {
      const char* l_cpszGroupName = OBJ_nid2sn(l_iNid);
      if (l_cpszGroupName == NULL)
      {
        l_cpszGroupName = OBJ_nid2ln(l_iNid);
      }
      printf("[INFO] KEM Group    : %s\n", l_cpszGroupName ? l_cpszGroupName : "unknown");
    }
    else
    {
      printf("[INFO] KEM Group    : (협상 없음)\n");
    }
  }

  X509* l_pPeerCert = SSL_get_peer_certificate(a_pSsl);
  if (l_pPeerCert != NULL)
  {
    X509_NAME* l_pSubject = X509_get_subject_name(l_pPeerCert);
    if (l_pSubject != NULL)
    {
      char l_cBuffer[512] = { 0 };
      X509_NAME_oneline(l_pSubject, l_cBuffer, static_cast<int>(sizeof(l_cBuffer)));
      printf("[INFO] Peer Subject : %s\n", l_cBuffer);
    }
    X509_free(l_pPeerCert);
  }
  else
  {
    printf("[INFO] Peer Subject : N/A\n");
  }
//printf("\n");
}
// -----------------------------------------------------------------------------
