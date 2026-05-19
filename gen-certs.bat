@ECHO OFF
REM ============================================================
REM gen-certs.bat
REM JK-PQC : 전체 인증서 일괄 생성
REM   [ML-DSA-65] CA / Server / Client
REM   [ECDSA P-256] CA / Server / Browser Client
REM ============================================================

REM ------------------------------------------------------------
REM 0. 디렉터리 준비
REM ------------------------------------------------------------
IF NOT EXIST certs              MKDIR certs
IF NOT EXIST certs\ca           MKDIR certs\ca
IF NOT EXIST certs\server       MKDIR certs\server
IF NOT EXIST certs\client       MKDIR certs\client
IF NOT EXIST certs\ecdsa        MKDIR certs\ecdsa
IF NOT EXIST certs\ecdsa\ca     MKDIR certs\ecdsa\ca
IF NOT EXIST certs\ecdsa\server MKDIR certs\ecdsa\server
IF NOT EXIST certs\ecdsa\browser MKDIR certs\ecdsa\browser

REM ------------------------------------------------------------
REM 1. Extension 파일 생성
REM    (배치 파일과 같은 폴더에 생성 → 실행 후 자동 삭제)
REM ------------------------------------------------------------

REM -- server_ext.cnf --
(
  ECHO basicConstraints=CA:FALSE
  ECHO keyUsage=critical,digitalSignature
  ECHO extendedKeyUsage=serverAuth
  ECHO subjectAltName=DNS:localhost,IP:127.0.0.1
) > server_ext.cnf

REM -- client_ext.cnf --
(
  ECHO basicConstraints=CA:FALSE
  ECHO keyUsage=critical,digitalSignature
  ECHO extendedKeyUsage=clientAuth
) > client_ext.cnf

REM ============================================================
REM [ML-DSA-65] 인증서 생성
REM ============================================================

REM ------------------------------------------------------------
REM 2. [ML-DSA] CA 키 + 자체 서명 루트 인증서 생성
REM ------------------------------------------------------------
ECHO [1/5] [ML-DSA] CA 키 및 루트 인증서 생성...
openssl req -x509 -new -newkey ml-dsa-65 ^
  -keyout certs\ca\ca-key.pem ^
  -out    certs\ca\ca-cert.pem ^
  -days 3650 -nodes ^
  -subj   "/CN=JK-PQC-CA-MLDSA65" ^
  -addext "basicConstraints=critical,CA:TRUE" ^
  -addext "keyUsage=critical,keyCertSign,cRLSign"
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 3. [ML-DSA] 서버 키 + CSR 생성
REM ------------------------------------------------------------
ECHO [2/5] [ML-DSA] 서버 키 및 CSR 생성...
openssl req -new -newkey ml-dsa-65 ^
  -keyout certs\server\server-key.pem ^
  -out    certs\server\server.csr ^
  -nodes ^
  -subj "/CN=localhost"
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 4. [ML-DSA] CA로 서버 인증서 서명 (SAN + EKU 포함)
REM ------------------------------------------------------------
ECHO [3/5] [ML-DSA] CA로 서버 인증서 서명...
openssl x509 -req ^
  -in    certs\server\server.csr ^
  -CA    certs\ca\ca-cert.pem ^
  -CAkey certs\ca\ca-key.pem ^
  -CAcreateserial ^
  -out   certs\server\server-cert.pem ^
  -days 365 ^
  -extfile server_ext.cnf
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 5. [ML-DSA] 클라이언트 키 + CSR 생성
REM ------------------------------------------------------------
ECHO [4/5] [ML-DSA] 클라이언트 키 및 CSR 생성...
openssl req -new -newkey ml-dsa-65 ^
  -keyout certs\client\client-key.pem ^
  -out    certs\client\client.csr ^
  -nodes ^
  -subj "/CN=JK-PQC-MLDSA65-Client"
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 6. [ML-DSA] CA로 클라이언트 인증서 서명 (EKU=clientAuth 포함)
REM ------------------------------------------------------------
ECHO [5/5] [ML-DSA] CA로 클라이언트 인증서 서명...
openssl x509 -req ^
  -in    certs\client\client.csr ^
  -CA    certs\ca\ca-cert.pem ^
  -CAkey certs\ca\ca-key.pem ^
  -CAcreateserial ^
  -out   certs\client\client-cert.pem ^
  -days 365 ^
  -extfile client_ext.cnf
IF ERRORLEVEL 1 GOTO :ERROR

REM -- [ML-DSA] .p12 변환 (향후 브라우저 PQC 지원 대비)
openssl pkcs12 -export ^
  -in    certs\client\client-cert.pem ^
  -inkey certs\client\client-key.pem ^
  -out   certs\client\client.p12 ^
  -name  "JK-PQC-MLDSA65-Client" ^
  -passout pass:
IF ERRORLEVEL 1 GOTO :ERROR

REM ============================================================
REM [ECDSA P-256] 인증서 생성
REM ============================================================

REM ------------------------------------------------------------
REM 7. [ECDSA] CA 키 + 자체 서명 루트 인증서 생성
REM    브라우저 호환용 (Chrome 등 ML-DSA 미지원 클라이언트)
REM ------------------------------------------------------------
ECHO [6/9] [ECDSA] CA 키 및 루트 인증서 생성...
openssl req -x509 -new -newkey ec -pkeyopt ec_paramgen_curve:P-256 ^
  -keyout certs\ecdsa\ca\ca-key.pem ^
  -out    certs\ecdsa\ca\ca-cert.pem ^
  -days 3650 -nodes ^
  -subj   "/CN=JK-PQC-CA-ECDSA" ^
  -addext "basicConstraints=critical,CA:TRUE" ^
  -addext "keyUsage=critical,keyCertSign,cRLSign"
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 8. [ECDSA] 서버 키 + CSR 생성
REM ------------------------------------------------------------
ECHO [7/9] [ECDSA] 서버 키 및 CSR 생성...
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:P-256 ^
  -keyout certs\ecdsa\server\server-key.pem ^
  -out    certs\ecdsa\server\server.csr ^
  -nodes ^
  -subj "/CN=localhost"
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 9. [ECDSA] CA로 서버 인증서 서명 (SAN + EKU 포함)
REM ------------------------------------------------------------
ECHO [8/9] [ECDSA] CA로 서버 인증서 서명...
openssl x509 -req ^
  -in    certs\ecdsa\server\server.csr ^
  -CA    certs\ecdsa\ca\ca-cert.pem ^
  -CAkey certs\ecdsa\ca\ca-key.pem ^
  -CAcreateserial ^
  -out certs\ecdsa\server\server-cert.pem ^
  -days 365 ^
  -extfile server_ext.cnf
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 10. [ECDSA] 브라우저용 클라이언트 키 + CSR 생성
REM     ECDSA CA로 서명 → 브라우저 mTLS 인증서
REM ------------------------------------------------------------
ECHO [9/9] [ECDSA] 브라우저용 클라이언트 인증서 생성...
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:P-256 ^
  -keyout certs\ecdsa\browser\client-key.pem ^
  -out    certs\ecdsa\browser\client.csr ^
  -nodes ^
  -subj "/CN=JK-PQC-ECDSA-Browser"
IF ERRORLEVEL 1 GOTO :ERROR

openssl x509 -req ^
  -in    certs\ecdsa\browser\client.csr ^
  -CA    certs\ecdsa\ca\ca-cert.pem ^
  -CAkey certs\ecdsa\ca\ca-key.pem ^
  -CAcreateserial ^
  -out certs\ecdsa\browser\client-cert.pem ^
  -days 365 ^
  -extfile client_ext.cnf
IF ERRORLEVEL 1 GOTO :ERROR

REM -- .p12 변환 (브라우저 Windows 설치용)
REM    비밀번호를 입력하지 않으면 Enter = 비밀번호 없음
openssl pkcs12 -export ^
  -in    certs\ecdsa\browser\client-cert.pem ^
  -inkey certs\ecdsa\browser\client-key.pem ^
  -out   certs\ecdsa\browser\client.p12 ^
  -name  "JK-PQC-ECDSA-Browser" ^
  -passout pass:
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 11. 임시 Extension 파일 정리
REM ------------------------------------------------------------
DEL server_ext.cnf client_ext.cnf

ECHO.
ECHO ============================================================
ECHO  인증서 생성 완료
ECHO  [ML-DSA-65]
ECHO    CA     : certs\ca\ca-cert.pem
ECHO    Server : certs\server\server-cert.pem
ECHO    Client : certs\client\client-cert.pem
ECHO             certs\client\client.p12  (향후 브라우저 PQC 지원 대비)
ECHO  [ECDSA P-256]
ECHO    CA     : certs\ecdsa\ca\ca-cert.pem
ECHO    Server : certs\ecdsa\server\server-cert.pem
ECHO    Browser: certs\ecdsa\browser\client-cert.pem
ECHO             certs\ecdsa\browser\client.p12  (Windows 설치용)
ECHO ============================================================
GOTO :EOF

REM ------------------------------------------------------------
:ERROR
ECHO.
ECHO [ERROR] 인증서 생성 중 오류가 발생했습니다.
ECHO 위 오류 메시지를 확인하세요.
IF EXIST server_ext.cnf DEL server_ext.cnf
IF EXIST client_ext.cnf DEL client_ext.cnf
EXIT /B 1
