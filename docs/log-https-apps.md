# `jk-https-*` 어플 로그

## 목차
- [1. jk-https-server](#1-jk-https-server)
  * [1-0. [2026.05.17] 사용법](#1-0-20260517-사용법)
  * [1-1. [2026.05.17] "Dual Cert" + mTLS](#1-1-20260517-dual-cert--mtls)
  * [1-2. [2026.05.17] "Dual Cert" + TLS](#1-2-20260517-dual-cert--tls)
  * [1-3. [2026.05.17] "Single Cert" + mTLS](#1-3-20260517-single-cert--mtls)
  * [1-4. [2026.05.17] "Single Cert" + TLS](#1-4-20260517-single-cert--tls)
- [2. [2026.05.17:jk-https-client]](#2-20260517jk-https-client)
- [3. [2026.05.17:jk-https-dashboard]](#3-20260517jk-https-dashboard)

---

## 1. jk-https-server
- `이중 인증서`를 사용하는 `1-1. "Dual Cert" + mTLS`와 `1-2. "Dual Cert" + TLS`는 현재의 브라우저에서도 접속할 수 있습니다.  
  다만, `1-1. mTLS`의 경우, 반드시 클라이언트 시스템에 `certs\ecdsa\browser\client.p12` 인증서 패키지를 등록시켜, 인증에 사용해야 합니다.  
  `1-2. TLS`는 일반적인 TLS로 동작하기 때문에, `PQC`가 적용되지 않은 평범한 TLS로 동작합니다.
- `단일 인증서`는 PQC (MLDSA) 인증서만 사용하기에, 현재의 브라우저는 인증할 수 없습니다.  
  특별히 개발한 `jk-https-client`와 `OpenSSL 클라이언트`로만, 정상적인 인증이 가능합니다.

### 1-0. [2026.05.17] 사용법
```bash
D:\E\Study\AI\AutoAgents\projects\JK-PQC>bin\msvc\Debug\jk-https-server --help
Usage: jk-https-server [OPTIONS]
  --kem <groups>  사용할 KEM 그룹 목록 (예: X25519MLKEM768:SecP256r1MLKEM768:X25519)
  --single-cert   ML-DSA 인증서만 수용 (이중 인증서 비활성화)
  --no-mtls       일반 TLS 동작 (클라이언트 인증서 불필요)
  --help          도움말 출력

```

### 1-1. [2026.05.17] "Dual Cert" + mTLS
- `이중 인증서` 활성화
- mTLS 동작 (클라이언트 인증서 필요)
```bash
D:\E\Study\AI\AutoAgents\projects\JK-PQC>bin\msvc\Debug\jk-https-server
Dual Cert    : ML-DSA + ECDSA
mTLS         : enabled
KEM Groups   : X25519MLKEM768:SecP256r1MLKEM768:X25519
서버 대기 중 : 0.0.0.0:18080
[INFO:127.0.0.1:58418:29A0] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58418] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : X25519MLKEM768
Peer Subject : /CN=JK-PQC-MLDSA65-Client

[INFO:127.0.0.1:58419:29A0] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58419] GET /api/status
[INFO:127.0.0.1:58420:6750] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58420] GET /api/session
[INFO:127.0.0.1:58421:E120] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58421] GET /api/client/info
[INFO:127.0.0.1:58422:E120] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58422] POST /api/echo
[INFO:127.0.0.1:58423:E120] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58423] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : SecP256r1MLKEM768
Peer Subject : /CN=JK-PQC-MLDSA65-Client

[INFO:127.0.0.1:58424:6750] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58424] GET /api/status
[INFO:127.0.0.1:58425:80C0] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58425] GET /api/session
[INFO:127.0.0.1:58426:6750] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58426] GET /api/client/info
[INFO:127.0.0.1:58427:1120] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58427] POST /api/echo
[INFO:127.0.0.1:58429:1120] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58429] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : x25519
Peer Subject : /CN=JK-PQC-MLDSA65-Client

[INFO:127.0.0.1:58430:1120] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58430] GET /api/status
[INFO:127.0.0.1:58431:B8F0] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58431] GET /api/session
[INFO:127.0.0.1:58432:4100] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58432] GET /api/client/info
[INFO:127.0.0.1:58433:4100] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:58433] POST /api/echo
[INFO:127.0.0.1:26981:9320] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
[INFO:127.0.0.1:01737:4100] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
[INFO:127.0.0.1:14896:4100] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
[INFO:127.0.0.1:47191:4100] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47191] GET /
Cipher Suite : TLS_AES_128_GCM_SHA256
KEM Group    : X25519MLKEM768
Peer Subject : /CN=JK-PQC-ECDSA-Browser

[INFO:127.0.0.1:60732:8BF0] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
[INFO:127.0.0.1:21733:9AB0] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)

```

### 1-2. [2026.05.17] "Dual Cert" + TLS
과거 보안이 약한, 일반 TLS 처럼 동작합니다.
- `이중 인증서` 활성화
- 일반 TLS 동작 (클라이언트 인증서 불필요)
```bash
D:\E\Study\AI\AutoAgents\projects\JK-PQC>bin\msvc\Debug\jk-https-server --no-mtls
Dual Cert    : ML-DSA + ECDSA
mTLS         : disabled (TLS only)
KEM Groups   : X25519MLKEM768:SecP256r1MLKEM768:X25519
서버 대기 중 : 0.0.0.0:18080
[INFO:127.0.0.1:21747:1EE0] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21747] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : X25519MLKEM768
Peer Subject : N/A

[INFO:127.0.0.1:21748:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21748] GET /api/status
[INFO:127.0.0.1:21749:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21749] GET /api/session
[INFO:127.0.0.1:21750:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21750] GET /api/client/info
[INFO:127.0.0.1:21751:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21751] POST /api/echo
[INFO:127.0.0.1:21752:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21752] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : SecP256r1MLKEM768
Peer Subject : N/A

[INFO:127.0.0.1:21753:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21753] GET /api/status
[INFO:127.0.0.1:21754:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21754] GET /api/session
[INFO:127.0.0.1:21755:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21755] GET /api/client/info
[INFO:127.0.0.1:21756:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21756] POST /api/echo
[INFO:127.0.0.1:21757:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21757] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : x25519
Peer Subject : N/A

[INFO:127.0.0.1:21758:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21758] GET /api/status
[INFO:127.0.0.1:21759:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21759] GET /api/session
[INFO:127.0.0.1:21760:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21760] GET /api/client/info
[INFO:127.0.0.1:21761:E760] Dual Cert : ML-DSA-65
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:21761] POST /api/echo
[INFO:127.0.0.1:15862:E760] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
[INFO:127.0.0.1:59071:E760] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
[INFO:127.0.0.1:19167:5F00] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
[INFO:127.0.0.1:20577:E760] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
[INFO:127.0.0.1:38771:E760] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:38771] GET /
Cipher Suite : TLS_AES_128_GCM_SHA256
KEM Group    : X25519MLKEM768
Peer Subject : N/A

[REQ_:127.0.0.1:38771] GET /favicon.ico
[INFO:127.0.0.1:08993:E760] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)
----------------------------------------
[ACPT] TLSv1.3 Handshake done

```

### 1-3. [2026.05.17] "Single Cert" + mTLS
- MLDSA (PQC) 인증서만 수용 (이중 인증서 비활성화, 브라우저 접속 불허)
- mTLS 동작 (클라이언트 인증서 필요)
```bash
D:\E\Study\AI\AutoAgents\projects\JK-PQC>bin\msvc\Debug\jk-https-server --single-cert
Dual Cert    : ML-DSA only
mTLS         : enabled
KEM Groups   : X25519MLKEM768:SecP256r1MLKEM768:X25519
서버 대기 중 : 0.0.0.0:18080
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09003] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : X25519MLKEM768
Peer Subject : /CN=JK-PQC-MLDSA65-Client

----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09004] GET /api/status
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09005] GET /api/session
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09006] GET /api/client/info
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09007] POST /api/echo
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09009] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : SecP256r1MLKEM768
Peer Subject : /CN=JK-PQC-MLDSA65-Client

----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09010] GET /api/status
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09011] GET /api/session
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09012] GET /api/client/info
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09013] POST /api/echo
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09014] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : x25519
Peer Subject : /CN=JK-PQC-MLDSA65-Client

----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09015] GET /api/status
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09016] GET /api/session
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09017] GET /api/client/info
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:09018] POST /api/echo

```

### 1-4. [2026.05.17] "Single Cert" + TLS
- MLDSA (PQC) 인증서만 수용 (이중 인증서 비활성화, 브라우저 접속 불허)
- 일반 TLS 동작 (클라이언트 인증서 불필요)
```bash
D:\E\Study\AI\AutoAgents\projects\JK-PQC>bin\msvc\Debug\jk-https-server --single-cert --no-mtls
Dual Cert    : ML-DSA only
mTLS         : disabled (TLS only)
KEM Groups   : X25519MLKEM768:SecP256r1MLKEM768:X25519
서버 대기 중 : 0.0.0.0:18080
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47152] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : X25519MLKEM768
Peer Subject : N/A

----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47153] GET /api/status
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47154] GET /api/session
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47155] GET /api/client/info
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47156] POST /api/echo
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47157] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : SecP256r1MLKEM768
Peer Subject : N/A

----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47158] GET /api/status
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47159] GET /api/session
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47160] GET /api/client/info
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47161] POST /api/echo
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47162] GET /
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : x25519
Peer Subject : N/A

----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47163] GET /api/status
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47164] GET /api/session
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47165] GET /api/client/info
----------------------------------------
[ACPT] TLSv1.3 Handshake done
[REQ_:127.0.0.1:47166] POST /api/echo

```

---

## 2. [2026.05.17:jk-https-client]
```bash
D:\E\Study\AI\AutoAgents\projects\JK-PQC>bin\msvc\Debug\jk-https-client
KEM Groups  : X25519MLKEM768:SecP256r1MLKEM768:X25519
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : X25519MLKEM768
Peer Subject : /CN=localhost

Server Response: Hello from PQC (HTTPS) Server
[API] /api/status 응답: {"cipher":"TLS_AES_256_GCM_SHA384","kem":"X25519MLKEM768:SecP256r1MLKEM768:X25519","server":"JK-PQC HTTPS Server","signature":"ML-DSA-65","status":"ok"}
[API] /api/session 응답: {"cipher_suite":"TLS_AES_256_GCM_SHA384","kem_group":"X25519MLKEM768","tls_version":"TLSv1.3"}
[API] /api/client/info 응답: {"not_after":"2027-05-16","not_before":"2026-05-16","subject":"/CN=JK-PQC-MLDSA65-Client"}
[API] /api/echo 응답: {"message":"hello PQC"}

D:\E\Study\AI\AutoAgents\projects\JK-PQC>bin\msvc\Debug\jk-https-client --kem SecP256r1MLKEM768
KEM Groups  : SecP256r1MLKEM768
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : SecP256r1MLKEM768
Peer Subject : /CN=localhost

Server Response: Hello from PQC (HTTPS) Server
[API] /api/status 응답: {"cipher":"TLS_AES_256_GCM_SHA384","kem":"X25519MLKEM768:SecP256r1MLKEM768:X25519","server":"JK-PQC HTTPS Server","signature":"ML-DSA-65","status":"ok"}
[API] /api/session 응답: {"cipher_suite":"TLS_AES_256_GCM_SHA384","kem_group":"SecP256r1MLKEM768","tls_version":"TLSv1.3"}
[API] /api/client/info 응답: {"not_after":"2027-05-16","not_before":"2026-05-16","subject":"/CN=JK-PQC-MLDSA65-Client"}
[API] /api/echo 응답: {"message":"hello PQC"}

D:\E\Study\AI\AutoAgents\projects\JK-PQC>bin\msvc\Debug\jk-https-client --kem X25519
KEM Groups  : X25519
Cipher Suite : TLS_AES_256_GCM_SHA384
KEM Group    : x25519
Peer Subject : /CN=localhost

Server Response: Hello from PQC (HTTPS) Server
[API] /api/status 응답: {"cipher":"TLS_AES_256_GCM_SHA384","kem":"X25519MLKEM768:SecP256r1MLKEM768:X25519","server":"JK-PQC HTTPS Server","signature":"ML-DSA-65","status":"ok"}
[API] /api/session 응답: {"cipher_suite":"TLS_AES_256_GCM_SHA384","kem_group":"x25519","tls_version":"TLSv1.3"}
[API] /api/client/info 응답: {"not_after":"2027-05-16","not_before":"2026-05-16","subject":"/CN=JK-PQC-MLDSA65-Client"}
[API] /api/echo 응답: {"message":"hello PQC"}

```

---

## 3. [2026.05.17:jk-https-dashboard]
```bash
D:\E\Study\AI\AutoAgents\projects\JK-PQC>bin\msvc\Debug\jk-https-dashboard
대시보드 서버 대기 중: https://0.0.0.0:18081

```
