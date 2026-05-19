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
  CHttpsBase();
  virtual ~CHttpsBase();

  static int smfClientHelloCb(SSL* a_pSsl, int* a_piAlert, void* a_pvArg); // ClientHello에서 ML-DSA 지원 여부에 따라 서버 인증서를 선택한다.

  bool      Init(OSSL_LIB_CTX* a_ptLibCtx, const std::string& a_rstrKEM="", bool a_bDualCert=true, bool a_bmTLS=true); // OQS Provider 로드 및 SSL_CTX 생성을 수행한다.
  const std::string& GetKemGroups() const; // 적용된 KEM 그룹 문자열을 리턴한다.
  void      SetCertPaths(const std::string& a_rstrCaPath, const std::string& a_rstrCertPath, const std::string& a_rstrKeyPath); // 인증서 경로들을 설정한다.
  void      SetEcdsaCertPaths(const std::string& a_rstrCaPath, const std::string& a_rstrCertPath, const std::string& a_rstrKeyPath); // ECDSA 인증서 경로를 설정하고 이중 인증서 모드를 켠다.
  SSL_CTX*  GetContext();                   // SSL_CTX 포인터를 리턴한다.
  void      PrintInfo4SSL(SSL* a_pSsl);     // SSL 연결 정보를 출력한다.
  // -------------------------------------
protected:
  virtual const SSL_METHOD* createMethod() = 0;   // 파생 클래스별 TLS 메소드를 리턴한다.
  bool applyCommonTlsOptions();                   // 공통 TLS 옵션을 적용한다.
  bool loadCertificates(bool a_bRequirePeerCert); // 인증서/키/검증 정책을 적용한다.
  // -------------------------------------
private:
  int       _winsock_Init();    // Winsock 초기화를 수행하고, 결과 코드를 리턴한다. (0=성공, 기타=실패)
  void      _winsock_Close();   // Winsock 정리를 수행한다.
  SSL_CTX*    m_pSslCtx;        // SSL_CTX 포인터
  bool        m_bWinsockInited; // Winsock 초기화 성공 여부
protected:
  std::string m_strKEM;         // KEM 그룹 목록
  std::string m_strCaPath;      // CA 인증서 경로
  std::string m_strCertPath;    // 엔드포인트 인증서 경로
  std::string m_strKeyPath;     // 엔드포인트 개인키 경로
  std::string m_strEcdsaCaPath;    // ECDSA CA 인증서 경로
  std::string m_strEcdsaCertPath;  // ECDSA 서버 인증서 경로
  std::string m_strEcdsaKeyPath;   // ECDSA 서버 개인키 경로
  bool        m_bDualCert,         // 이중 인증서 활성화 여부 (기본값: false)
              m_bmTLS;             // mTLS 활성화 여부 (기본값: true)
  // -------------------------------------
};

/*----------------------------------------------------------------------------+-
"mTLS + PQC + HTTPS" 통신의 서버 클래스
-+----------------------------------------------------------------------------+-
- 목적
  
- 주의 사항
  > 
-+----------------------------------------------------------------------------*/
class CHttpsServer : public CHttpsBase
{
  // -------------------------------------
public:
  CHttpsServer();
  virtual ~CHttpsServer();
  bool Run(int a_iPort); // HTTPS 서버를 구동한다.
  // -------------------------------------
protected:
  virtual const SSL_METHOD* createMethod() override;
  void _updateStatsLoop(); // 공유 메모리 통계 업데이트 루프

private:
  CShmStatsProducer m_oShmStatsProducer; // 공유 메모리 통계 프로듀서
  std::atomic<bool> m_bRunStatsLoop;     // 업데이트 루프 실행 여부
  std::thread m_oStatsThread;            // 통계 업데이트 스레드
};

/*----------------------------------------------------------------------------+-
"mTLS + PQC + HTTPS" 통신의 클라이언트 클래스
-+----------------------------------------------------------------------------+-
- 목적

- 주의 사항
  >
-+----------------------------------------------------------------------------*/
class CHttpsClient : public CHttpsBase
{
  // -------------------------------------
public:
  CHttpsClient();
  virtual ~CHttpsClient();
  bool Get(const std::string& a_rstrHost, int a_iPort, std::string& a_rstrResponseBody); // HTTPS GET 요청을 수행한다. (경로: /)
  bool Get(const std::string& a_rstrHost, int a_iPort, const std::string& a_rstrPath, std::string& a_rstrResponseBody); // 지정 경로로 HTTPS GET 요청을 수행한다.
  bool Post(const std::string& a_rstrHost, int a_iPort, const std::string& a_rstrPath, const std::string& a_rstrBody, const std::string& a_rstrContentType, std::string& a_rstrResponseBody); // 지정 경로로 HTTPS POST 요청을 수행한다.
  // -------------------------------------
protected:
  virtual const SSL_METHOD* createMethod() override;
  // -------------------------------------
private:
  bool m_bSslInfoPrinted; // SSL 정보 1회 출력 여부
};
// -----------------------------------------------------------------------------
