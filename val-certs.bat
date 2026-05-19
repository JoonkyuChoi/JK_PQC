@ECHO OFF
REM ============================================================
REM val-certs.bat
REM JK-PQC : CA / Server / Client 인증서 검증 (ML-DSA-65)
REM ============================================================
REM CA 인증서 정보 확인 (ML-DSA-65 서명 알고리즘인지)
openssl x509 -in certs\ca\ca-cert.pem -noout -text | findstr /i "Signature Subject Issuer Basic"

REM 서버 인증서 검증 (CA로 서명됐는지)
openssl verify -CAfile certs\ca\ca-cert.pem certs\server\server-cert.pem

REM 클라이언트 인증서 검증 (CA로 서명됐는지)
openssl verify -CAfile certs\ca\ca-cert.pem certs\client\client-cert.pem
REM --------------------------------------
