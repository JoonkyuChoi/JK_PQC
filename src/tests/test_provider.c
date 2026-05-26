/*----------------------------------------------------------------------------+-
test_provider.c
-+----------------------------------------------------------------------------+-
Description	: OpenSSL Provider (oqsprovider) 로드 테스트
Copyright		: 2026~ by Joonkyu.Choi, All rights reserved.

변경이력:
  [2026/04/27] 최초 작성
-+----------------------------------------------------------------------------*/
#include <openssl/provider.h>
#include <openssl/err.h>
#include <stdio.h>
/*----------------------------------------------------------------------------+-
OpenSSL용 [oqsprovider] 로드 테스트
-+----------------------------------------------------------------------------+-
- 직접 컴파일
  > cd D:\E\Study\AI\AutoAgents\projects\JK-PQC\src\tests
  > cl test_provider.c /I"D:\openssl\3.5.4\include" /link D:\openssl\3.5.4\lib\libcrypto.lib Crypt32.lib Ws2_32.lib
  > SET OPENSSL_MODULES "D:\openssl\3.5.4\lib\ossl-modules"
  > test_provider.exe
-+----------------------------------------------------------------------------*/
int main()
{
  // cnf 없이 직접 로드
  OSSL_PROVIDER* oqs = OSSL_PROVIDER_load(NULL, "oqsprovider");
  if (!oqs)
  {
    printf("[ERR_] OSSL_PROVIDER_load failed:\n");
    ERR_print_errors_fp(stderr);
    return 1;
  }
  printf("[INFO] oqsprovider loaded OK\n");
  OSSL_PROVIDER_unload(oqs);

  return 0;
}
// -----------------------------------------------------------------------------
