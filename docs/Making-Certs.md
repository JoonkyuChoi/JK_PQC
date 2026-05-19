# `이중 인증서` 체계 구축

## 목차
- [용어 정의](#용어-정의)
- [OpenSSL 지원 사양](#openssl-지원-사양)
  * [OpenSSL의 제공자 확인](#openssl의-제공자-확인)
  * [OpenSSL이 제공하는 서명 알고리즘들 확인](#openssl이-제공하는-서명-알고리즘들-확인)
  * [OpenSSL이 제공하는 키 교환/캡슐화 알고리즘들 확인](#openssl이-제공하는-키-교환캡슐화-알고리즘들-확인)
- [인증서 체계 구축](#인증서-체계-구축)
  * [mTLS를 위한, `클라이언트 인증서` 등록하기](#mtls를-위한-클라이언트-인증서-등록하기)
  * [mTLS를 위한, `클라이언트 인증서` 등록 확인](#mtls를-위한-클라이언트-인증서-등록-확인)
  * [mTLS에 대한, `클라이언트 인증서` 해제하기](#mtls에-대한-클라이언트-인증서-해제하기)

---

## 용어 정의
```
DSA   Digital Signature Algorithm     전자 서명 알고리즘
                                      전자서명이나 인증서 제작에 사용됩니다.
                                      이 알고리즘으로 상대방과 공통된 비밀키를 만드는 '키 교환'을 수행하기는 매우 어렵고 비효율적입니다.
KEM   Key-Encapsulation Mechanism     키 교화/캡슐화 메카니즘
                                      데이터를 암호화하여 전달하거나, 세션 키를 안전하게 공유할 때 사용합니다.
                                      접속자와 서버 간에 데이터를 암호화할 비밀키를 안전하게 교환합니다.
                                      이 메커니즘 자체로는 '디지털 서명' 기능을 제공하지 않습니다.
```

---

## OpenSSL 지원 사양

### OpenSSL의 제공자 확인
```bash
$ openssl list -providers -provider oqsprovider
Providers:
  default
    name: OpenSSL Default Provider
    version: 3.5.4
    status: active
  oqsprovider
    name: OpenSSL OQS Provider
    version: 0.9.0
    status: active
```

### OpenSSL이 제공하는 서명 알고리즘들 확인
```bash
$ openssl list -signature-algorithms
  { 1.2.840.113549.1.1.1, 2.5.8.1.1, RSA, rsaEncryption } @ default
  { 1.2.840.10040.4.1, 1.3.14.3.2.12, DSA, DSA-old, dsaEncryption, dsaEncryption-old } @ default
  { 1.2.840.10040.4.3, 1.3.14.3.2.27, DSA-SHA, DSA-SHA-1, DSA-SHA1, DSA-SHA1-old, dsaWithSHA, dsaWithSHA1, dsaWithSHA1-old } @ default
  { 1.3.101.112, ED25519 } @ default
  { 1.3.101.113, ED448 } @ default
  { 1.2.156.10197.1.301, SM2 } @ default
  { 2.16.840.1.101.3.4.3.1, DSA-SHA2-224, DSA-SHA224, dsa_with_SHA224 } @ default
  { 2.16.840.1.101.3.4.3.2, DSA-SHA2-256, DSA-SHA256, dsa_with_SHA256 } @ default
  { 1.2.840.1.101.3.4.3.3, DSA-SHA2-384, DSA-SHA384, dsa_with_SHA384, id-dsa-with-sha384 } @ default
  { 1.2.840.1.101.3.4.3.4, DSA-SHA2-512, DSA-SHA512, dsa_with_SHA512, id-dsa-with-sha512 } @ default
  { 2.16.840.1.101.3.4.3.5, DSA-SHA3-224, dsa_with_SHA3-224, id-dsa-with-sha3-224 } @ default
  { 2.16.840.1.101.3.4.3.6, DSA-SHA3-256, dsa_with_SHA3-256, id-dsa-with-sha3-256 } @ default
  { 2.16.840.1.101.3.4.3.7, DSA-SHA3-384, dsa_with_SHA3-384, id-dsa-with-sha3-384 } @ default
  { 2.16.840.1.101.3.4.3.8, DSA-SHA3-512, dsa_with_SHA3-512, id-dsa-with-sha3-512 } @ default
  { 1.3.36.3.3.1.2, ripemd160WithRSA, RSA-RIPEMD160 } @ default
  { 1.2.840.113549.1.1.5, RSA-SHA-1, RSA-SHA1, sha1WithRSAEncryption } @ default
  { 1.2.840.113549.1.1.14, RSA-SHA2-224, RSA-SHA224, sha224WithRSAEncryption } @ default
  { 1.2.840.113549.1.1.11, RSA-SHA2-256, RSA-SHA256, sha256WithRSAEncryption } @ default
  { 1.2.840.113549.1.1.12, RSA-SHA2-384, RSA-SHA384, sha384WithRSAEncryption } @ default
  { 1.2.840.113549.1.1.13, RSA-SHA2-512, RSA-SHA512, sha512WithRSAEncryption } @ default
  { 1.2.840.113549.1.1.15, RSA-SHA2-512/224, RSA-SHA512-224, sha512-224WithRSAEncryption } @ default
  { 1.2.840.113549.1.1.16, RSA-SHA2-512/256, RSA-SHA512-256, sha512-256WithRSAEncryption } @ default
  { 2.16.840.1.101.3.4.3.13, id-rsassa-pkcs1-v1_5-with-sha3-224, RSA-SHA3-224 } @ default
  { 2.16.840.1.101.3.4.3.14, id-rsassa-pkcs1-v1_5-with-sha3-256, RSA-SHA3-256 } @ default
  { 2.16.840.1.101.3.4.3.15, id-rsassa-pkcs1-v1_5-with-sha3-384, RSA-SHA3-384 } @ default
  { 2.16.840.1.101.3.4.3.16, id-rsassa-pkcs1-v1_5-with-sha3-512, RSA-SHA3-512 } @ default
  { 1.2.156.10197.1.504, RSA-SM3, sm3WithRSAEncryption } @ default
  ED25519ph @ default
  ED25519ctx @ default
  ED448ph @ default
  ECDSA @ default
  { 1.2.840.10045.4.1, ECDSA-SHA-1, ECDSA-SHA1, ecdsa-with-SHA1 } @ default
  { 1.2.840.10045.4.3.1, ECDSA-SHA2-224, ECDSA-SHA224, ecdsa-with-SHA224 } @ default
  { 1.2.840.10045.4.3.2, ECDSA-SHA2-256, ECDSA-SHA256, ecdsa-with-SHA256 } @ default
  { 1.2.840.10045.4.3.3, ECDSA-SHA2-384, ECDSA-SHA384, ecdsa-with-SHA384 } @ default
  { 1.2.840.10045.4.3.4, ECDSA-SHA2-512, ECDSA-SHA512, ecdsa-with-SHA512 } @ default
  { 2.16.840.1.101.3.4.3.9, ECDSA-SHA3-224, ecdsa_with_SHA3-224, id-ecdsa-with-sha3-224 } @ default
  { 2.16.840.1.101.3.4.3.10, ECDSA-SHA3-256, ecdsa_with_SHA3-256, id-ecdsa-with-sha3-256 } @ default
  { 2.16.840.1.101.3.4.3.11, ECDSA-SHA3-384, ecdsa_with_SHA3-384, id-ecdsa-with-sha3-384 } @ default
  { 2.16.840.1.101.3.4.3.12, ECDSA-SHA3-512, ecdsa_with_SHA3-512, id-ecdsa-with-sha3-512 } @ default
  { 2.16.840.1.101.3.4.3.17, id-ml-dsa-44, ML-DSA-44, MLDSA44 } @ default
  { 2.16.840.1.101.3.4.3.18, id-ml-dsa-65, ML-DSA-65, MLDSA65 } @ default
  { 2.16.840.1.101.3.4.3.19, id-ml-dsa-87, ML-DSA-87, MLDSA87 } @ default
  HMAC @ default
  SIPHASH @ default
  POLY1305 @ default
  CMAC @ default
  { 2.16.840.1.101.3.4.3.20, id-slh-dsa-sha2-128s, SLH-DSA-SHA2-128s } @ default
  { 2.16.840.1.101.3.4.3.21, id-slh-dsa-sha2-128f, SLH-DSA-SHA2-128f } @ default
  { 2.16.840.1.101.3.4.3.22, id-slh-dsa-sha2-192s, SLH-DSA-SHA2-192s } @ default
  { 2.16.840.1.101.3.4.3.23, id-slh-dsa-sha2-192f, SLH-DSA-SHA2-192f } @ default
  { 2.16.840.1.101.3.4.3.24, id-slh-dsa-sha2-256s, SLH-DSA-SHA2-256s } @ default
  { 2.16.840.1.101.3.4.3.25, id-slh-dsa-sha2-256f, SLH-DSA-SHA2-256f } @ default
  { 2.16.840.1.101.3.4.3.26, id-slh-dsa-shake-128s, SLH-DSA-SHAKE-128s } @ default
  { 2.16.840.1.101.3.4.3.27, id-slh-dsa-shake-128f, SLH-DSA-SHAKE-128f } @ default
  { 2.16.840.1.101.3.4.3.28, id-slh-dsa-shake-192s, SLH-DSA-SHAKE-192s } @ default
  { 2.16.840.1.101.3.4.3.29, id-slh-dsa-shake-192f, SLH-DSA-SHAKE-192f } @ default
  { 2.16.840.1.101.3.4.3.30, id-slh-dsa-shake-256s, SLH-DSA-SHAKE-256s } @ default
  { 2.16.840.1.101.3.4.3.31, id-slh-dsa-shake-256f, SLH-DSA-SHAKE-256f } @ default
```

### OpenSSL이 제공하는 키 교환/캡슐화 알고리즘들 확인
```bash
$ openssl list -kem-algorithms
  { 1.2.840.113549.1.1.1, 2.5.8.1.1, RSA, rsaEncryption } @ default
  { 1.2.840.10045.2.1, EC, id-ecPublicKey } @ default
  { 1.3.101.110, X25519 } @ default
  { 1.3.101.111, X448 } @ default
  { 2.16.840.1.101.3.4.4.1, id-alg-ml-kem-512, ML-KEM-512, MLKEM512 } @ default
  { 2.16.840.1.101.3.4.4.2, id-alg-ml-kem-768, ML-KEM-768, MLKEM768 } @ default
  { 2.16.840.1.101.3.4.4.3, id-alg-ml-kem-1024, ML-KEM-1024, MLKEM1024 } @ default
  X25519MLKEM768 @ default
  X448MLKEM1024 @ default
  SecP256r1MLKEM768 @ default
  SecP384r1MLKEM1024 @ default
```

---

## 인증서 체계 구축
- [gen-certs.bat](../gen-certs.bat) 파일을 실행하면, 개발에 필요한 인증서 파일들이 `certs/` 폴더에 구축됩니다.
- [val-certs.bat](../val-certs.bat) 파일을 실행하여, 인증서 검증을 확인할 수 있습니다.
- [gen-dash-certs.bat](../gen-dash-certs.bat) 파일을 실행하면, 대시보드(모니터링) 서버에 필요한 인증서 파일들이 `certs/dashboard/` 폴더에 구축됩니다.

> 중요:  
  현재(2026.05) 시점에, `Hybrid 인증서`는 비표준 상태입니다.  
  비표준으로 인하여 `Hybrid 인증서` 구축은 제외되었으며, 대신에 `이중 인증서` 개념을 적용하여, 브라우저의 인증도 수용할 수 있도록 하였습니다.  
  `PQC를 지원하지 않는 고전 브라우저`의 경우, `TLS-Handshake`단계에서 PQC 지원여부를 판단하여, `고전 ECDSA 인증서`로 서명(신원확인)하도록 처리하였습니다.  
  본 프로젝트는 `PQC 인증서 + mTLS + Hybrid 키교환` 기능을 적용시켜, 현재의 표준 PQC 통신을 구현하였습니다.

### mTLS를 위한, `클라이언트 인증서` 등록하기
mTLS는 내부망 C/S 간에, 상호 인증을 적용한 것으로, 서버에 접속하는 클라이언트도 반드시 `서버와 동일한 CA에서 발급한 인증서`를 제출하여, 서버로 부터 검증을 받아야 합니다.  
mTLS 서버에 접속하려는 브라우저의 경우, `서버와 동일한 CA에서 발급한 인증서`를 로컬 시스템에 등록시켜 놓고, (브라우저에서) 최초 접속 시, 수동으로 선택해 주어야 합니다.
```bash
# [MLDSA] 지원 브라우저용 (향후 브라우저 PQC 지원 대비)
$ certutil -importpfx -user certs\client\client.p12
# [ECDSA] 구형 브라우저용
$ certutil -importpfx -user certs\ecdsa\browser\client.p12
```

### mTLS를 위한, `클라이언트 인증서` 등록 확인
```bash
$ certutil -store -user My | findstr "JK-PQC"
```

### mTLS에 대한, `클라이언트 인증서` 해제하기
```bash
# [MLDSA] 지원 브라우저용 (향후 브라우저 PQC 지원 대비)
$ certutil -delstore -user My "JK-PQC-MLDSA65-Client"
# [ECDSA] 구형 브라우저용
$ certutil -delstore -user My "JK-PQC-ECDSA-Browser"
```
