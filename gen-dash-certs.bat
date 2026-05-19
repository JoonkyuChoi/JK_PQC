@ECHO OFF
REM ============================================================
REM gen-dash-certs.bat
REM JK-PQC : Dashboard 전용 CA / Server 인증서 일괄 생성 (ECDSA P-256)
REM ============================================================

REM ------------------------------------------------------------
REM 0. 디렉터리 준비
REM ------------------------------------------------------------
IF NOT EXIST certs           MKDIR certs
IF NOT EXIST certs\dashboard MKDIR certs\dashboard

REM ------------------------------------------------------------
REM 1. Extension 파일 생성
REM ------------------------------------------------------------
(
  ECHO basicConstraints=CA:FALSE
  ECHO keyUsage=critical,digitalSignature
  ECHO extendedKeyUsage=serverAuth
  ECHO subjectAltName=DNS:localhost,IP:127.0.0.1
) > dashboard_ext.cnf

REM ------------------------------------------------------------
REM 2. 대시보드 전용 CA 키 + 자체 서명 루트 인증서 (ECDSA P-256)
REM ------------------------------------------------------------
ECHO [1/3] 대시보드 CA 키 및 루트 인증서 생성...
openssl req -x509 -new -newkey ec -pkeyopt ec_paramgen_curve:P-256 ^
  -keyout certs\dashboard\dashboard-ca-key.pem ^
  -out   certs\dashboard\dashboard-ca-cert.pem ^
  -days 3650 -nodes ^
  -subj "/CN=JK-PQC-Dashboard-CA" ^
  -addext "basicConstraints=critical,CA:TRUE" ^
  -addext "keyUsage=critical,keyCertSign,cRLSign"
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 3. 대시보드 서버 키 + CSR 생성
REM ------------------------------------------------------------
ECHO [2/3] 대시보드 서버 키 및 CSR 생성...
openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:P-256 ^
  -keyout certs\dashboard\dashboard-key.pem ^
  -out   certs\dashboard\dashboard.csr ^
  -nodes ^
  -subj "/CN=localhost"
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 4. 대시보드 CA로 서버 인증서 서명 (SAN + EKU 포함)
REM ------------------------------------------------------------
ECHO [3/3] 대시보드 CA로 서버 인증서 서명...
openssl x509 -req ^
  -in  certs\dashboard\dashboard.csr ^
  -CA    certs\dashboard\dashboard-ca-cert.pem ^
  -CAkey certs\dashboard\dashboard-ca-key.pem ^
  -CAcreateserial ^
  -out certs\dashboard\dashboard-cert.pem ^
  -days 365 ^
  -extfile dashboard_ext.cnf
IF ERRORLEVEL 1 GOTO :ERROR

REM ------------------------------------------------------------
REM 5. 임시 Extension 파일 정리
REM ------------------------------------------------------------
DEL dashboard_ext.cnf

ECHO.
ECHO ============================================================
ECHO  인증서 생성 완료
ECHO  Dashboard CA     : certs\dashboard\dashboard-ca-cert.pem
ECHO  Dashboard Server : certs\dashboard\dashboard-cert.pem
ECHO ============================================================
GOTO :EOF

REM ------------------------------------------------------------
:ERROR
ECHO.
ECHO [ERROR] 인증서 생성 중 오류가 발생했습니다.
ECHO 위 오류 메시지를 확인하세요.
IF EXIST dashboard_ext.cnf DEL dashboard_ext.cnf
EXIT /B 1
