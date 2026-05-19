/*----------------------------------------------------------------------------+-
pqc_utils.h
-+----------------------------------------------------------------------------+-
Description : PQC/OpenSSL 공통 유틸 함수 선언
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/27] 최초 작성
  [2026/05/14] gfPrintInfo4SSL 설명 보강(협상 그룹)
-+----------------------------------------------------------------------------*/
#pragma once

#include <openssl/provider.h>
#include <openssl/ssl.h>

bool gfLoadProvider4OQS(OSSL_LIB_CTX* a_pCtx); // 해당 OpenSSL 컨텍스트에 OQS-Provider를 로드하고, 성공 여부를 리턴한다. (a_pCtx가 NULL이면 기본 라이브러리 컨텍스트에 로드합니다.)
void gfPrintInfo4SSL(SSL* a_pSsl); // Cipher Suite 후 KEM(우선 SSL_get0_group_name, 폴백 SSL_get_negotiated_group+OBJ), Peer Subject 출력
