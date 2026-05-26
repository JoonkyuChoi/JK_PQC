/*----------------------------------------------------------------------------+-
https.h
-+----------------------------------------------------------------------------+-
Description : "mTLS + PQC + HTTPS" 통신을 위한, 클래스들 정의
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
  [2026/05/14] GetKemGroups() 추가
  [2026/05/15] 이중 인증서(ML-DSA / ECDSA) 경로·ClientHello 콜백 지원
-+----------------------------------------------------------------------------*/
#pragma once

#include <string>
#include <atomic>
#include <thread>

#include <openssl/provider.h>
#include <openssl/ssl.h>

#include "httplib.h"  // cpp-httplib (0.40.0)
#include "pqc_utils.h"
#include "shm_stats.h"
/*----------------------------------------------------------------------------+-
"mTLS + PQC + HTTPS" 통신의 기반 클래스
-+----------------------------------------------------------------------------+-
- 목적
  OpenSSL 라이브러리 초기화 및 정리를 담당하는, HTTPS 통신의 기반 클래스
- 주의 사항
  > 반드시 SSL_CTX_new() 함수를 사용하여, oqs-provider를 로드해야만 한다.
-+----------------------------------------------------------------------------*/
class CHttpsBase
{
  // -------------------------------------
public:
  CHttpsBase();           // 생성자 - 멤버 기본값 초기화
  virtual ~CHttpsBase();  // 소멸자 - SSL_CTX·Winsock 정리

  static int smfClientHelloCb(SSL* a_pSsl, int* a_piAlert, void* a_pvArg);
    // ClientHello signature_algorithms(ext 13)에 따라 ML-DSA/ECDSA 서버 인증서 선택

  bool Init(OSSL_LIB_CTX* a_ptLibCtx, const std::string& a_rstrKEM="",
            bool a_bDualCert=true, bool a_bmTLS=true);
    // OQS Provider 로드, SSL_CTX 생성, KEM·mTLS·이중인증서 옵션 적용
  const std::string& GetKemGroups() const; // SSL_CTX에 적용된 KEM 그룹 문자열
  void SetCertPaths(const std::string& a_rstrCaPath,
                    const std::string& a_rstrCertPath,
                    const std::string& a_rstrKeyPath);
    // ML-DSA(또는 기본) CA·엔드포인트 인증서·개인키 경로 설정
  void SetEcdsaCertPaths(const std::string& a_rstrCaPath,
                         const std::string& a_rstrCertPath,
                         const std::string& a_rstrKeyPath);
    // ECDSA CA·인증서·키 경로 설정 및 이중 인증서 모드 활성화
  SSL_CTX* GetContext();              // 내부 SSL_CTX* 반환 (소유권 없음)
  void PrintInfo4SSL(SSL* a_pSsl);    // gfPrintInfo4SSL 래퍼 - 연결 정보 출력
  // -------------------------------------
protected:
  virtual const SSL_METHOD* createMethod() = 0; // 파생 클래스 TLS 메서드 (client/server)
  bool applyCommonTlsOptions();               // KEM·알고리즘 등 공통 SSL_CTX 옵션
  bool loadCertificates(bool a_bRequirePeerCert); // PEM 로드 및 mTLS 검증 정책
  // -------------------------------------
private:
  int  _winsock_Init();   // Winsock 2.2 초기화 (0=성공)
  void _winsock_Close();  // WSACleanup 호출

  SSL_CTX* m_pSslCtx;         // OpenSSL SSL_CTX (oqs-provider 적용)
  bool     m_bWinsockInited;  // Winsock 초기화 완료 여부
protected:
  std::string m_strKEM;            // Init()에 적용된 Hybrid KEM 그룹 목록
  std::string m_strCaPath;         // CA PEM 경로
  std::string m_strCertPath;       // 서버/클라이언트 인증서 PEM 경로
  std::string m_strKeyPath;        // 개인키 PEM 경로
  std::string m_strEcdsaCaPath;    // ECDSA용 CA PEM 경로 (이중 인증서)
  std::string m_strEcdsaCertPath;  // ECDSA 서버 인증서 PEM 경로
  std::string m_strEcdsaKeyPath;   // ECDSA 서버 개인키 PEM 경로
  bool        m_bDualCert;         // ClientHello 기반 ML-DSA/ECDSA 이중 인증서 모드
  bool        m_bmTLS;             // 상호 TLS(클라이언트 인증서 필수) 여부
  // -------------------------------------
};

/*----------------------------------------------------------------------------+-
"mTLS + PQC + HTTPS" 통신의 서버 클래스 (cpp-httplib SSLServer)
-+----------------------------------------------------------------------------*/
class CHttpsServer : public CHttpsBase
{
  // -------------------------------------
public:
  CHttpsServer();           // 생성자
  virtual ~CHttpsServer();  // 소멸자 - 통계 스레드·SHM 정리

  bool Run(int a_iPort);    // 지정 포트 HTTPS 리스닝 및 이벤트 루프 (블로킹)
  // -------------------------------------
protected:
  virtual const SSL_METHOD* createMethod() override; // TLS_server_method()
  // -------------------------------------
private:
  void _updateStatsLoop();  // 1초 주기 uptime·SHM 통계 갱신 스레드 본체

  CShmStatsProducer m_oShmStatsProducer;  // Global\\JK_PQC_STATS 프로듀서
  std::atomic<bool> m_bRunStatsLoop;      // _updateStatsLoop 실행 플래그
  std::thread       m_oStatsThread;       // 통계 갱신 백그라운드 스레드
};

/*----------------------------------------------------------------------------+-
"mTLS + PQC + HTTPS" 통신의 클라이언트 클래스 (cpp-httplib SSLClient)
-+----------------------------------------------------------------------------*/
class CHttpsClient : public CHttpsBase
{
  // -------------------------------------
public:
  CHttpsClient();           // 생성자
  virtual ~CHttpsClient();  // 소멸자

  bool Get(const std::string& a_rstrHost, int a_iPort,
           std::string& a_rstrResponseBody);
    // GET / (루트 경로) HTTPS 요청
  bool Get(const std::string& a_rstrHost, int a_iPort,
           const std::string& a_rstrPath, std::string& a_rstrResponseBody);
    // GET 지정 경로 HTTPS 요청
  bool Post(const std::string& a_rstrHost, int a_iPort,
            const std::string& a_rstrPath, const std::string& a_rstrBody,
            const std::string& a_rstrContentType, std::string& a_rstrResponseBody);
    // POST 지정 경로·Content-Type HTTPS 요청
  // -------------------------------------
protected:
  virtual const SSL_METHOD* createMethod() override; // TLS_client_method()
  // -------------------------------------
private:
  bool m_bSslInfoPrinted; // 세션 검증 시 Cipher/KEM/Subject 1회 출력 여부
};
// -----------------------------------------------------------------------------
