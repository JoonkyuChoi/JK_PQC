/*----------------------------------------------------------------------------+-
SocketPool.h
-+----------------------------------------------------------------------------+-
Description	: Windows IOCP / Linux epoll 기반 IPv4/IPv6 실시간 소켓 풀
              클라이언트(Connect) 및 서버(Listen/Accept) 모드 통합 지원
Copyright		: 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/05/21] 추초 작성 - IPv4/IPv6, Server/Client 통합, 스케줄러 추가
-+----------------------------------------------------------------------------*/
#pragma once

// =============================================================================
// 플랫폼 헤더
// =============================================================================
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  pragma comment(lib, "ws2_32.lib")
   using socket_t  = SOCKET;
   using ssize_t   = SSIZE_T;
   static constexpr socket_t D_INVALID_SOCK = INVALID_SOCKET;
#else
#  include <sys/epoll.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <fcntl.h>
   using socket_t = int;
   static constexpr socket_t D_INVALID_SOCK = -1;
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// =============================================================================
// 상수 정의
// =============================================================================
#define D_SP_DEFAULT_WORKERS      4         // 기본 워커 쓰레드 수
#define D_SP_RECV_BUF_SIZE        65536     // 수신 버퍼 크기 (64KB)
#define D_SP_MAX_EPOLL_EVENTS     256       // epoll 일회 최대 이벤트 수
#define D_SP_DEFAULT_BACKLOG      128       // listen 백로그 크기
#define D_SP_SCHED_INTERVAL_MS    5000      // 스케줄러 점검 주기 (ms)
#define D_SP_DEFAULT_IDLE_SEC     0         // 무통신 타임아웃 (0=비활성화)

// =============================================================================
// 콜백 함수 타입
// =============================================================================
typedef std::function<void(socket_t, const std::string&, uint16_t)>   CBFP_ACCEPT;      // 커넥션 수락
typedef std::function<void(socket_t)>                                 CBFP_CONNECT;     // 커넥션 성립
typedef std::function<void(socket_t)>                                 CBFP_DISCONNECT;  // 커넥션 해제
typedef std::function<void(socket_t, const uint8_t*, size_t)>         CBFP_RECV;        // 데이터 수신
typedef std::function<void(socket_t, int)>                            CBFP_ERROR;       // 오류 발생

// =============================================================================
// eSocketRole : 소켓 역할 구분
// =============================================================================
enum eSocketRole
{
  ROLE_CLIENT = 0,  // 0 Connect() 로 연결한 클라이언트 소켓
  ROLE_ACCEPTED,    // 1 Accept() 로 수락한 서버 스아 소켓
  ROLE_LISTEN       // 2 리스닝 소켓 (풀 내부 관리용)
};

// =============================================================================
// T_SOCKET_CTX : 소켓당 상태 컨텍스트
// =============================================================================
struct T_SOCKET_CTX
{
  socket_t          m_tSock    { D_INVALID_SOCK };    // 소켓 핸들
  eSocketRole       m_eRole    { ROLE_CLIENT };       // 소켓 역할
  std::string       m_oPeerIp;                        // 상대방 IP 문자열
  uint16_t          m_usPeerPort { 0 };               // 상대방 포트
  std::atomic<bool> m_bAlive    { true };             // 생존 여부 (스케줄러에서 검토)

  // 마지막 수신 시각 (타임아웃 지원용)
  std::chrono::steady_clock::time_point m_tLastRecv { std::chrono::steady_clock::now() };

#ifdef _WIN32
  // ---------------------------------------------------------------------------
  // IOCP Overlapped 연산 단위
  // ---------------------------------------------------------------------------
  struct T_IOCP_OP
  {
    OVERLAPPED      m_tOvlp    {};                    // Overlapped (반드시 첫 멤버)
    WSABUF          m_tWsaBuf  {};                    // WSA 버퍼 디스크립터
    DWORD           m_dwFlags  { 0 };                 // WSARecv/WSASend 플래그
    uint8_t         m_ucBuf[D_SP_RECV_BUF_SIZE] {};   // I/O 버퍼

    enum class eOpType { Recv, Send } m_eType { eOpType::Recv };  // 연산 방향
    T_SOCKET_CTX*   m_pCtx { nullptr };               // 컨텍스트 역참조
  };

  T_IOCP_OP   m_tRecvOp;    // Recv Overlapped
  T_IOCP_OP   m_tSendOp;    // Send Overlapped
#else
  uint8_t     m_ucRecvBuf[D_SP_RECV_BUF_SIZE] {};     // epoll 수신 버퍼
#endif

  std::mutex  m_oSendMtx;   // 동시 송신 직렬화 Mutex
};

// =============================================================================
// CSocketPool : IOCP(Windows) / epoll(Linux) 통합 소켓 풀
//               IPv4 / IPv6 동시 지원
//               클라이언트 모드 : Connect()
//               서버       모드 : Listen() + 자동 Accept
// =============================================================================
class CSocketPool
{
  // ---------------------------------------------------------------------------
public:
  // -----------------
  // 설정 구조체
  // -----------------
  struct T_CONFIG
  {
    int       m_iWorkerThreads  { D_SP_DEFAULT_WORKERS };   // 워커 쓰레드 수
    bool      m_bTcpNoDelay     { true  };                  // TCP_NODELAY
    bool      m_bKeepAlive      { true  };                  // SO_KEEPALIVE
    bool      m_bDualStack      { true  };                  // IPv6 소켓에서 IPv4도 수용 (IPV6_V6ONLY=0)
    uint32_t  m_uiIdleTimeoutSec{ D_SP_DEFAULT_IDLE_SEC };  // 무통신 타임아웃 (0=비활성화)
  };
  // -----------------
  // 생성자 / 소멸자
  // -----------------
  explicit CSocketPool(const T_CONFIG& a_tCfg = {});
  ~CSocketPool();

  CSocketPool(const CSocketPool&)            = delete;
  CSocketPool& operator=(const CSocketPool&) = delete;
  // -----------------
  // 라이프사이클
  // -----------------
  bool  Start();   // 워커 쓰레드 + 스케줄러 시작
  void  Stop();    // 전체 종료 및 소켓 정리
  // -----------------
  // 서버 모드
  // -----------------
  bool  Listen   (const std::string& a_oBindIp,
                  uint16_t           a_usPort,
                  int                a_iBacklog = D_SP_DEFAULT_BACKLOG);  // 바인드 + 리스닝 시작
  void  StopListen();                                                     // 리스닝 소켓 종료
  // -----------------
  // 클라이언트 모드
  // -----------------
  socket_t  Connect   (const std::string& a_oHost, uint16_t a_usPort);    // TCP 접속 후 풀 등록
  void      Disconnect(socket_t a_tSock);                                 // 소켓 종료 및 풀 제거
  // -----------------
  // I/O
  // -----------------
  bool  Send(socket_t        a_tSock,
             const uint8_t*  a_pucData,
             size_t          a_ullLen);   // 데이터 송신
  // -----------------
  // 콜백 등록
  // -----------------
  void SetOnAccept    (CBFP_ACCEPT     a_cbfOnAccept)    { m_cbfOnAccept_     = std::move(a_cbfOnAccept);    };
  void SetOnConnect   (CBFP_CONNECT    a_cbfOnConnect)   { m_cbfOnConnect_    = std::move(a_cbfOnConnect);   };
  void SetOnDisconnect(CBFP_DISCONNECT a_cbfOnDisconn)   { m_cbfOnDisconnect_ = std::move(a_cbfOnDisconn);   };
  void SetOnRecv      (CBFP_RECV       a_cbfOnRecv)      { m_cbfOnRecv_       = std::move(a_cbfOnRecv);      };
  void SetOnError     (CBFP_ERROR      a_cbfOnError)     { m_cbfOnError_      = std::move(a_cbfOnError);     };
  // -----------------
  // 상태 조회
  // -----------------
  size_t    GetConnectionCount() const;                           // 현재 접속 수 (리스닝 소켓 제외)
  bool      GetPeerInfo(socket_t      a_tSock,
                        std::string&  a_oIp,
                        uint16_t&     a_usPort) const;            // 상대방 주소 조회

  // ---------------------------------------------------------------------------
private:
  // -----------------
  // 공통 내부 함수
  // -----------------
  bool  _ApplySocketOptions (socket_t a_tSock);                   // TCP 옵션 적용
  bool  _RegisterContext    (socket_t a_tSock,
                             eSocketRole        a_eRole,
                             const std::string& a_oIp,
                             uint16_t           a_usPort);        // 컨텍스트 등록 + IOCP/epoll 연결
  void  _CloseSocket        (socket_t a_tSock, bool a_bNotify);   // 소켓 종료 + 컨텍스트 제거 + 콜백
  void  _RemoveContext      (socket_t a_tSock);                   // 컨텍스트 맵에서만 제거
  std::shared_ptr<T_SOCKET_CTX>
        _FindContext        (socket_t a_tSock) const;             // 컨텍스트 조회
  bool  _ResolvePeerAddr    (const sockaddr_storage& a_tSs,
                             std::string& a_oIp,
                             uint16_t&    a_usPort);              // sockaddr → IP문자열/포트 변환
  socket_t
        _CreateListenSocket (const std::string& a_oIp,
                             uint16_t           a_usPort,
                             int                a_iBacklog);      // 리스닝 소켓 생성
  // -----------------
  // 스케줄러
  // -----------------
  void  _SchedulerProc();    // 주기적 소켓 상태 점검 쓰레드
  // -----------------
  // Accept 루프
  // -----------------
  void  _AcceptProc();       // 리스닝 소켓 Accept 쓰레드

#ifdef _WIN32
  // -----------------
  // IOCP 내부 함수
  // -----------------
  bool  _PostRecv   (T_SOCKET_CTX& a_tCtx);   // WSARecv Overlapped 등록
  void  _WorkerIOCP ();                       // IOCP 워커 쓰레드 진입점
#else
  // -----------------
  // epoll 내부 함수
  // -----------------
  bool  _SetNonBlocking (socket_t a_tSock);         // O_NONBLOCK 설정
  bool  _AddToEpoll     (socket_t a_tSock);         // epoll ET 모드 등록
  void  _HandleRead     (T_SOCKET_CTX& a_tCtx);     // ET 수신 drain 처리
  void  _WorkerEpoll    ();                         // epoll 워커 쓰레드 진입점
#endif
  // ---------------------------------------------------------------------------
  // 멤버 변수
  // ---------------------------------------------------------------------------
  T_CONFIG                    m_tCfg;                 // 설정값
  std::atomic<bool>           m_bRunning  { false };  // 전체 실행 상태
  std::atomic<bool>           m_bListening{ false };  // 리스닝 상태

  // 워커 / 스케줄러 / 엑셉 쓰레드
  std::vector<std::thread>    m_oWorkers;             // I/O 워커 쓰레드 목록
  std::thread                 m_oScheduler;           // 스케줄러 쓰레드
  std::thread                 m_oAcceptor;            // Accept 쓰레드

  // 컨텍스트 맵
  mutable std::mutex          m_oCtxMtx;              // 컨텍스트 맵 보호 Mutex
  std::unordered_map<socket_t,
    std::shared_ptr<T_SOCKET_CTX>> m_oCtxMap;         // 소켓 컨텍스트 맵

  // 리스닝 소켓
  socket_t                    m_tListenSock { D_INVALID_SOCK };  // 리스닝 소켓 핸들

  // 콜백
  CBFP_ACCEPT       m_cbfOnAccept_;      // 커넥션 수락 콜백
  CBFP_CONNECT      m_cbfOnConnect_;     // 커넥션 성립 콜백
  CBFP_DISCONNECT   m_cbfOnDisconnect_;  // 커넥션 해제 콜백
  CBFP_RECV         m_cbfOnRecv_;        // 수신 콜백
  CBFP_ERROR        m_cbfOnError_;       // 오류 콜백

#ifdef _WIN32
  HANDLE            m_hIocp { nullptr };          // IOCP 커널 핸들
#else
  int               m_iEpfd         { -1 };       // epoll 파일 디스크립터
  int               m_iWakeupFd[2]  { -1, -1 };   // 워커 종료용 pipe (read/write)
#endif
  // ---------------------------------------------------------------------------
};
