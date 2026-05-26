/*----------------------------------------------------------------------------+-
pqchttps.h
-+----------------------------------------------------------------------------+-
Description : CSocketPool + llhttp + OpenSSL Hybrid mTLS HTTP 서버 모듈
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/05/24] 최초 작성
  [2026/05/24] shm_stats 연동, 대시보드 API 라우트(CPqcHttpService)
-+----------------------------------------------------------------------------*/
#pragma once

#include "SocketPool.h"
#include "shm_stats.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <openssl/ssl.h>
#include <openssl/x509.h>

// =============================================================================
// 상수 / 버퍼 크기
// =============================================================================
#define D_PQCHTTPS_VERSION        "1.0.0"   // 모듈/API 버전 문자열
#define D_PQCHTTPS_DEFAULT_KEM    "X25519MLKEM768:SecP256r1MLKEM768:X25519"
                                          // Init() 기본 Hybrid KEM 그룹 목록
#define D_PQCHTTPS_RECV_BUF       65536     // TLS·HTTP 수신 버퍼 (64KB)
#define D_PQCHTTPS_RESP_BUF       65536     // HTTP 응답 조립 버퍼 (64KB)

// =============================================================================
// T_HTTP_REQ : llhttp 파싱 결과 HTTP 요청 컨텍스트
// =============================================================================
struct T_HTTP_REQ
{
  std::string               m_strMethod;   // HTTP 메서드 (GET, POST, …)
  std::string               m_strUrl;      // 요청 경로+쿼리 (/api/status 등)
  std::map<std::string,
           std::string>     m_oHeaders;    // 요청 헤더 (필드명→값)
  std::string               m_strBody;     // 요청 본문 (POST/PUT)
};

// =============================================================================
// 콜백 함수 타입
// =============================================================================
typedef std::function<void(socket_t a_tSock,
                           const uint8_t* a_pucData,
                           size_t a_ullLen)>                    CBFP_TLS_PLAIN_RECV;
  // TLS 복호화된 평문 수신 콜백 (CSocketPool → CHybridTls → 앱)

typedef std::function<void(socket_t a_tSock,
                           const T_HTTP_REQ& a_rReq)>           CBFP_HTTP_REQUEST;
  // HTTP 요청 1건 파싱 완료 콜백

typedef std::function<bool(socket_t a_tSock,
                           const T_HTTP_REQ& a_rReq,
                           int& a_iStatus,
                           std::string& a_rstrBody,
                           std::string& a_rstrContentType)>       CBFP_ROUTE_HANDLER;
  // 라우트 핸들러 (true=성공, a_rstrContentType 비우면 application/json)

// =============================================================================
// CMtlsAuth : 클라이언트 X.509 인증서 검증·속성 추출
// =============================================================================
class CMtlsAuth
{
  // -------------------------------------
public:
  CMtlsAuth();  // 생성자
  ~CMtlsAuth(); // 소멸자 - X509_STORE 해제

  bool Init(const char* a_pszCaFile);                       // CA PEM → X509_STORE
  bool Verify(X509* a_pX509Cert) const;                     // 체인·유효기간 검증
  bool GetSubjectCN(X509* a_pX509, std::string& a_rstrCn) const;   // Subject CN
  bool GetSubjectSAN(X509* a_pX509, std::vector<std::string>& a_roSans) const; // DNS/URI SAN
  // -------------------------------------
private:
  X509_STORE* m_pStore;   // 신뢰 CA 저장소 (Init에서 로드)
};

// =============================================================================
// CHybridTls : OpenSSL BIO 메모리 + SSL_CTX Hybrid mTLS
// =============================================================================
class CHybridTls
{
  // -------------------------------------
public:
  CHybridTls();  // 생성자
  ~CHybridTls(); // 소멸자 - SSL_CTX·세션 맵 정리

  bool Init(const char* a_pszCertFile,
            const char* a_pszKeyFile,
            const char* a_pszCaFile,
            const char* a_pszKemGroups = D_PQCHTTPS_DEFAULT_KEM,
            bool a_bDualCert = true,
            bool a_bmTLS = true);
    // oqs-provider 로드, SSL_CTX·인증서·KEM·이중인증서·mTLS 설정
  void SetEcdsaCertPaths(const char* a_pszCaFile,
                         const char* a_pszCertFile,
                         const char* a_pszKeyFile);
    // ECDSA CA·서버 인증서·키 경로 (이중 인증서 모드)
  const std::string& GetKemGroups() const;  // 적용된 KEM 그룹 문자열
  void SetSocketPool(CSocketPool* a_pPool);   // 암호화 송신용 CSocketPool (non-owning)
  void SetOnPlainRecv(CBFP_TLS_PLAIN_RECV a_cbf); // TLS 평문 수신 콜백
  void SetMtlsAuth(CMtlsAuth* a_pAuth);       // 핸드셰이크 후 verify_result 검사용

  bool AcceptHandshake(socket_t a_tSock);     // 서버: 소켓별 SSL_accept 세션 생성
  bool CompleteHandshakeBlocking(socket_t a_tSock); // Accept 스레드: IOCP 전 동기 핸드셰이크
  bool ConnectHandshake(socket_t a_tSock);  // 클라이언트: SSL_connect 세션 생성
  int  Read(socket_t a_tSock, uint8_t* a_pucBuf, size_t a_ullLen);   // SSL_read
  int  Write(socket_t a_tSock, const uint8_t* a_pucBuf, size_t a_ullLen); // SSL_write+flush
  bool OnRecvRaw(socket_t a_tSock,
                 const uint8_t* a_pucData,
                 size_t a_ullLen);           // CSocketPool cbfOnRecv 진입점
  void RemoveSession(socket_t a_tSock);       // 연결 종료 시 SSL/BIO 세션 제거
  X509* GetPeerCert(socket_t a_tSock) const;  // 피어 cert (호출자 X509_free)
  bool IsHandshakeDone(socket_t a_tSock) const; // 핸드셰이크 완료 여부
  SSL*  GetSsl(socket_t a_tSock) const;       // SSL* (정보 출력·API용)
  // -------------------------------------
private:
  struct T_TLS_SESSION
  {
    SSL*          m_pSsl           { nullptr }; // OpenSSL SSL 객체
    BIO*          m_pRbio          { nullptr }; // 수신 암호문 메모리 BIO
    BIO*          m_pWbio          { nullptr }; // 송신 암호문 메모리 BIO
    bool          m_bHandshakeDone { false };   // TLS 핸드셰이크 완료 플래그
    bool          m_bIsServer      { true };    // true=accept, false=connect
    mutable std::recursive_mutex m_oMtx;        // 세션별 재진입 가능 잠금
  };

  std::shared_ptr<T_TLS_SESSION> _GetSession(socket_t a_tSock) const;
    // 소켓 fd → TLS 세션 shared_ptr (없으면 nullptr)
  bool _CreateSession(socket_t a_tSock, bool a_bServer);
    // BIO 쌍·SSL_new·accept/connect 상태 등록
  bool _DriveHandshake(T_TLS_SESSION& a_rSess, socket_t a_tSock);
    // SSL_accept/connect 루프 및 wbio flush
  bool _FlushWbio(T_TLS_SESSION& a_rSess, socket_t a_tSock);
    // wbio → CSocketPool::Send 동기 송신 (IOCP 등록 후 사용)
  bool _FlushWbioDirect(T_TLS_SESSION& a_rSess, socket_t a_tSock);
    // 핸드셰이크 전용: wbio → blocking ::send (IOCP 우회, Accept 스레드 전용)
  bool _ReadPlain(T_TLS_SESSION& a_rSess, socket_t a_tSock);
    // SSL_read → m_cbfPlainRecv
  bool _loadServerCertificates();
    // SSL_CTX에 서버 인증서·CA·ClientHello 콜백 적용
  void _configureSslCtxSession();
    // 세션 ID 컨텍스트·재개 비활성 (이중 인증서 ClientHello 콜백과 충돌 방지)
  static int smfClientHelloCb(SSL* a_pSsl, int* a_piAlert, void* a_pvArg);
    // ClientHello signature_algorithms에 따라 ML-DSA/ECDSA 서버 인증서 선택
  // -------------------------------------
private:
  SSL_CTX*              m_pSslCtx     { nullptr }; // 서버/클라이언트 공용 SSL_CTX
  std::string           m_strKem;                  // Init()에 설정된 KEM 목록
  std::string           m_strCaPath;               // ML-DSA CA PEM 경로
  std::string           m_strCertPath;             // ML-DSA 서버 인증서 PEM 경로
  std::string           m_strKeyPath;              // ML-DSA 서버 개인키 PEM 경로
  std::string           m_strEcdsaCaPath;          // ECDSA CA PEM 경로
  std::string           m_strEcdsaCertPath;        // ECDSA 서버 인증서 PEM 경로
  std::string           m_strEcdsaKeyPath;         // ECDSA 서버 개인키 PEM 경로
  bool                  m_bDualCert     { true };  // ClientHello 기반 이중 인증서 모드
  bool                  m_bmTLS         { true };  // mTLS(클라이언트 인증서 필수) 여부
  CSocketPool*          m_pPool       { nullptr }; // Send() 대상 풀
  CMtlsAuth*            m_pAuth       { nullptr }; // mTLS 검증기 (optional)
  CBFP_TLS_PLAIN_RECV   m_cbfPlainRecv;            // 평문 수신 콜백

  mutable std::mutex    m_oMapMtx;                 // m_oSessionMap 보호
  std::mutex            m_oSslAcceptMtx;           // SSL_CTX 공유 시 SSL_accept 직렬화
  std::unordered_map<socket_t,
    std::shared_ptr<T_TLS_SESSION>> m_oSessionMap; // fd → TLS 세션
};

// =============================================================================
// CHttpParser : llhttp 기반 HTTP/1.1 요청 파서
// =============================================================================
class CHttpParser
{
  // -------------------------------------
public:
  CHttpParser();  // 생성자
  ~CHttpParser(); // 소멸자 - pimpl 해제

  bool Init();                                              // llhttp_t·settings 초기화
  bool Execute(const uint8_t* a_pucData, size_t a_ullLen);  // 수신 버퍼 파싱
  void Reset();                                             // llhttp_reset (메시지 간)
  bool ConsumeMessageComplete();                            // 완료 시 true 반환 후 Reset
  void SetOnRequest(CBFP_HTTP_REQUEST a_cbf);               // message_complete 콜백
  void SetSocket(socket_t a_tSock);                         // 콜백에 전달할 fd
  static std::string BuildResponse(int a_iStatus,
                                   const char* a_pszBody,
                                   size_t a_ullLen,
                                   const char* a_pszContentType = "application/json");
    // HTTP/1.1 Connection:close 응답 문자열 생성
  // -------------------------------------
private:
  void* m_pImpl { nullptr };  // T_HTTP_PARSER_IMPL (cpp 전용 pimpl)
};

// =============================================================================
// CHttpRouter : method+path → 핸들러 디스패치
// =============================================================================
class CHttpRouter
{
  // -------------------------------------
public:
  void AddRoute(const char* a_pszMethod,
                const char* a_pszPath,
                CBFP_ROUTE_HANDLER a_cbfHandler);           // 라우트 등록
  void SetStatsProducer(CShmStatsProducer* a_pProducer);    // SHM 통계 (optional)
  bool Dispatch(socket_t a_tSock,
                const T_HTTP_REQ& a_rReq,
                CHybridTls& a_rTls,
                size_t a_ullConnCount);                     // 매칭 핸들러 실행·응답
  // -------------------------------------
private:
  CShmStatsProducer* m_pStatsProducer { nullptr }; // Dispatch 시 OnConnect/AddRequest 등

  struct T_ROUTE_KEY
  {
    std::string m_strMethod;  // HTTP 메서드
    std::string m_strPath;    // URL 경로 (쿼리 제외)
    bool operator==(const T_ROUTE_KEY& a_rOther) const // method·path 동일 여부
    {
      return (m_strMethod == a_rOther.m_strMethod) &&
             (m_strPath == a_rOther.m_strPath);
    }
  };

  struct T_ROUTE_KEY_HASH
  {
    size_t operator()(const T_ROUTE_KEY& a_rKey) const // unordered_map용 해시
    {
      return std::hash<std::string>()(a_rKey.m_strMethod) ^
             (std::hash<std::string>()(a_rKey.m_strPath) << 1);
    }
  };

  std::unordered_map<T_ROUTE_KEY,
    CBFP_ROUTE_HANDLER,
    T_ROUTE_KEY_HASH> m_oRoutes;  // 등록된 라우트 테이블
};

// =============================================================================
// CPqcHttpService : jk-pqc-server SHM 통계 프로듀서
// =============================================================================
class CPqcHttpService
{
  // -------------------------------------
public:
  CPqcHttpService();  // 생성자
  ~CPqcHttpService(); // 소멸자 - Shutdown()

  bool Init();                          // CShmStatsProducer·갱신 스레드
  void Shutdown();                      // 스레드 join·SHM 해제
  CShmStatsProducer* GetStatsProducer(); // SHM 프로듀서 포인터
  // -------------------------------------
private:
  void updateStatsLoop();             // 1초 주기 Update()

  CShmStatsProducer     m_oShmStatsProducer;  // Global\\JK_PQC_STATS 기록
  std::atomic<bool>     m_bRunStatsLoop;      // updateStatsLoop 실행 플래그
  std::thread           m_oStatsThread;       // uptime 갱신 스레드
};
// -----------------------------------------------------------------------------
