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

// OQS-Provider(oqsprovider)를 OpenSSL 라이브러리 컨텍스트에 동적 로드
// a_pCtx: 대상 OSSL_LIB_CTX (NULL이면 프로세스 기본 컨텍스트)
// 반환값: provider 로드·활성화 성공 여부
bool gfLoadProvider4OQS(OSSL_LIB_CTX* a_pCtx);

// TLS 핸드셰이크 완료 후 협상 Cipher·KEM 그룹·피어 인증서 Subject를 stdout에 출력
// a_pSsl: 정보를 조회할 SSL 세션 포인터 (NULL이면 no-op)
void gfPrintInfo4SSL(SSL* a_pSsl);
// -----------------------------------------------------------------------------
