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
// 플랫폼 헤더·소켓 타입
// =============================================================================
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  pragma comment(lib, "ws2_32.lib")
   using socket_t  = SOCKET;                          // Windows 소켓 핸들 타입
   using ssize_t   = SSIZE_T;                         // signed size (POSIX 호환)
   static constexpr socket_t D_INVALID_SOCK = INVALID_SOCKET; // 유효하지 않은 소켓
#else
#  include <sys/epoll.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <fcntl.h>
   using socket_t = int;                              // POSIX 소켓 fd
   static constexpr socket_t D_INVALID_SOCK = -1;      // 유효하지 않은 fd
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
#define D_SP_DEFAULT_WORKERS      4         // Start() 기본 IOCP/epoll 워커 수
#define D_SP_RECV_BUF_SIZE        65536     // 소켓당 수신 버퍼 (64KB)
#define D_SP_MAX_EPOLL_EVENTS     256       // epoll_wait 최대 이벤트 수
#define D_SP_DEFAULT_BACKLOG      128       // listen() backlog
#define D_SP_SCHED_INTERVAL_MS    5000      // _SchedulerProc 주기 (ms)
#define D_SP_DEFAULT_IDLE_SEC     0         // 무통신 타임아웃 (0=비활성)

// =============================================================================
// 콜백 함수 타입
// =============================================================================
typedef std::function<bool(socket_t, const std::string&, uint16_t)>  CBFP_ACCEPT;
  // Accept 직후 호출 (false 반환 시 소켓 등록·수신 시작 안 함)
typedef std::function<void(socket_t)>                                 CBFP_CONNECT;
  // Connect() 성공 후 IOCP/epoll 등록 완료 시
typedef std::function<void(socket_t)>                                 CBFP_DISCONNECT;
  // _CloseSocket() 시 알림 (TLS·파서 정리용)
typedef std::function<void(socket_t, const uint8_t*, size_t)>         CBFP_RECV;
  // 수신 데이터 (워커 스레드 컨텍스트)
typedef std::function<void(socket_t, int)>                            CBFP_ERROR;
  // I/O 오류 코드 전달

// =============================================================================
// eSocketRole : T_SOCKET_CTX 역할
// =============================================================================
enum eSocketRole
{
  ROLE_CLIENT   = 0,  // Connect()로 생성된 아웃바운드 소켓
  ROLE_ACCEPTED = 1,  // Accept()로 수락된 인바운드 소켓
  ROLE_LISTEN   = 2   // Listen() 리스닝 소켓 (내부용)
};

// =============================================================================
// T_SOCKET_CTX : 소켓당 I/O·상태 컨텍스트
// =============================================================================
struct T_SOCKET_CTX
{
  socket_t          m_tSock    { D_INVALID_SOCK };    // OS 소켓 핸들
  eSocketRole       m_eRole    { ROLE_CLIENT };       // CLIENT / ACCEPTED / LISTEN
  std::string       m_oPeerIp;                        // 상대 IP (IPv4/IPv6 문자열)
  uint16_t          m_usPeerPort { 0 };               // 상대 포트 (호스트 바이트 순서)
  std::atomic<bool> m_bAlive    { true };             // false면 스케줄러·워커가 정리
  std::atomic<bool> m_bClosePending { false };        // OnRecv 콜백 후 지연 종료 요청

  std::chrono::steady_clock::time_point m_tLastRecv { std::chrono::steady_clock::now() };
    // 마지막 수신 시각 (idle timeout 판단)

#ifdef _WIN32
  // ---------------------------------------------------------------------------
  // T_IOCP_OP : WSARecv/WSASend OVERLAPPED 연산 단위
  // ---------------------------------------------------------------------------
  struct T_IOCP_OP
  {
    OVERLAPPED      m_tOvlp    {};                    // 반드시 첫 멤버 (OVERLAPPED*)
    WSABUF          m_tWsaBuf  {};                    // WSABuf {buf,len}
    DWORD           m_dwFlags  { 0 };                 // WSARecv 플래그
    uint8_t         m_ucBuf[D_SP_RECV_BUF_SIZE] {};   // recv/send 공용 버퍼

    enum class eOpType { Recv, Send } m_eType { eOpType::Recv }; // I/O 방향
    T_SOCKET_CTX*   m_pCtx { nullptr };               // 역참조 (completion key 보조)
  };

  T_IOCP_OP   m_tRecvOp;    // WSARecv overlapped 상태
  T_IOCP_OP   m_tSendOp;    // WSASend overlapped 상태 (현재 Send는 sync send 사용)
#else
  uint8_t     m_ucRecvBuf[D_SP_RECV_BUF_SIZE] {};     // epoll ET drain 버퍼
#endif

  std::mutex  m_oSendMtx;   // Send() 직렬화 (동시 송신 방지)
};

// =============================================================================
// CSocketPool : 고성능 이벤트 기반 TCP 소켓 풀
// =============================================================================
class CSocketPool
{
  // ---------------------------------------------------------------------------
public:
  // -----------------
  // T_CONFIG : Start()/Listen() 전 설정
  // -----------------
  struct T_CONFIG
  {
    int       m_iWorkerThreads  { D_SP_DEFAULT_WORKERS };   // I/O 워커 스레드 수
    bool      m_bTcpNoDelay     { true  };                  // TCP_NODELAY (Nagle off)
    bool      m_bKeepAlive      { true  };                  // SO_KEEPALIVE
    bool      m_bDualStack      { true  };                  // IPv6에서 IPv4-mapped 허용
    uint32_t  m_uiIdleTimeoutSec{ D_SP_DEFAULT_IDLE_SEC };  // 무수신 강제 종료 (초)
  };
  // -----------------
  // 생성 / 소멸
  // -----------------
  explicit CSocketPool(const T_CONFIG& a_tCfg = {}); // 설정 복사 저장
  ~CSocketPool();                                     // Stop() 호출

  CSocketPool(const CSocketPool&)            = delete;
  CSocketPool& operator=(const CSocketPool&) = delete;
  // -----------------
  // 라이프사이클
  // -----------------
  bool  Start();   // 워커·스케줄러·IOCP/epoll 초기화
  void  Stop();    // m_bRunning=false, 전 소켓 종료, 스레드 join
  // -----------------
  // 서버 모드
  // -----------------
  bool  Listen   (const std::string& a_oBindIp,
                  uint16_t           a_usPort,
                  int                a_iBacklog = D_SP_DEFAULT_BACKLOG);
    // bind+listen, Accept 스레드 시작
  void  StopListen();  // Accept 루프 중지·리스닝 소켓 close
  // -----------------
  // 클라이언트 모드
  // -----------------
  socket_t  Connect   (const std::string& a_oHost, uint16_t a_usPort);
    // TCP connect 후 풀·IOCP 등록, fd 반환
  void      Disconnect(socket_t a_tSock);   // 즉시 _CloseSocket (콜백 밖에서 사용)
  void      RequestClose(socket_t a_tSock); // OnRecv 콜백 내 - 워커가 콜백 후 종료
  // -----------------
  // 송신
  // -----------------
  bool  Send(socket_t        a_tSock,
             const uint8_t*  a_pucData,
             size_t          a_ullLen);   // 동기 send() 전체 길이 송신
  // -----------------
  // 콜백 등록 (std::move)
  // -----------------
  void SetOnAccept    (CBFP_ACCEPT     a_cbfOnAccept)    { m_cbfOnAccept_     = std::move(a_cbfOnAccept);    };
  void SetOnConnect   (CBFP_CONNECT    a_cbfOnConnect)   { m_cbfOnConnect_    = std::move(a_cbfOnConnect);   };
  void SetOnDisconnect(CBFP_DISCONNECT a_cbfOnDisconn)   { m_cbfOnDisconnect_ = std::move(a_cbfOnDisconn);   };
  void SetOnRecv      (CBFP_RECV       a_cbfOnRecv)      { m_cbfOnRecv_       = std::move(a_cbfOnRecv);      };
  void SetOnError     (CBFP_ERROR      a_cbfOnError)     { m_cbfOnError_      = std::move(a_cbfOnError);     };
  // -----------------
  // 상태 조회
  // -----------------
  size_t    GetConnectionCount() const;     // m_oCtxMap 크기 (listen 제외)
  bool      GetPeerInfo(socket_t      a_tSock,
                        std::string&  a_oIp,
                        uint16_t&     a_usPort) const;  // Accept/Connect 시 저장값
  // Accept 직후: IOCP/epoll 등록만 하고 WSARecv/epoll 수신은 시작하지 않음 (TLS 핸드셰이크용)
  bool      BeginAcceptedSocket(socket_t           a_tSock,
                                const std::string& a_oIp,
                                uint16_t           a_usPort);
  // BeginAcceptedSocket 이후 비동기 수신(WSARecv/epoll) 시작
  bool      StartRecvForSocket(socket_t a_tSock);

  // ---------------------------------------------------------------------------
private:
  // -----------------
  // 내부 공통
  // -----------------
  bool  _ApplySocketOptions (socket_t a_tSock);   // NODELAY·KEEPALIVE
  bool  _RegisterContext    (socket_t a_tSock,
                             eSocketRole        a_eRole,
                             const std::string& a_oIp,
                             uint16_t           a_usPort);
    // m_oCtxMap 삽입 + IOCP CreateIoCompletionPort / epoll ADD
  void  _CloseSocket        (socket_t a_tSock, bool a_bNotify);
    // alive=false, map erase, ::closesocket, OnDisconnect
  void  _RemoveContext      (socket_t a_tSock);   // map만 erase (레거시)
  std::shared_ptr<T_SOCKET_CTX>
        _FindContext        (socket_t a_tSock) const; // map 조회
  bool  _ResolvePeerAddr    (const sockaddr_storage& a_tSs,
                             std::string& a_oIp,
                             uint16_t&    a_usPort);
    // accept/connect 주소 → 문자열 IP·포트
  socket_t
        _CreateListenSocket (const std::string& a_oIp,
                             uint16_t           a_usPort,
                             int                a_iBacklog);
    // IPv4/IPv6 dual-stack listen fd
  // -----------------
  // 백그라운드 스레드
  // -----------------
  void  _SchedulerProc();  // idle·dead 소켓 주기 검사
  void  _AcceptProc();     // blocking accept 루프

#ifdef _WIN32
  bool  _PostRecv   (T_SOCKET_CTX& a_tCtx);   // WSARecv overlapped 재등록
  void  _WorkerIOCP ();                       // GetQueuedCompletionStatus 루프
#else
  bool  _SetNonBlocking (socket_t a_tSock);   // O_NONBLOCK
  bool  _AddToEpoll     (socket_t a_tSock);   // EPOLLIN|ET
  void  _HandleRead     (T_SOCKET_CTX& a_tCtx); // recv ET drain
  void  _WorkerEpoll    ();                   // epoll_wait 루프
#endif
  // ---------------------------------------------------------------------------
  // 멤버 변수
  // ---------------------------------------------------------------------------
  T_CONFIG                    m_tCfg;                 // 생성 시 T_CONFIG 복사본
  std::atomic<bool>           m_bRunning  { false };  // Start~Stop 구간 true
  std::atomic<bool>           m_bListening{ false };  // Listen~StopListen true

  std::vector<std::thread>    m_oWorkers;             // IOCP/epoll 워커들
  std::thread                 m_oScheduler;           // _SchedulerProc
  std::thread                 m_oAcceptor;            // _AcceptProc (서버)

  mutable std::mutex          m_oCtxMtx;              // m_oCtxMap mutex
  std::unordered_map<socket_t,
    std::shared_ptr<T_SOCKET_CTX>> m_oCtxMap;         // fd → 컨텍스트

  socket_t                    m_tListenSock { D_INVALID_SOCK }; // Listen fd

  CBFP_ACCEPT       m_cbfOnAccept_;      // SetOnAccept
  CBFP_CONNECT      m_cbfOnConnect_;     // SetOnConnect
  CBFP_DISCONNECT   m_cbfOnDisconnect_;  // SetOnDisconnect
  CBFP_RECV         m_cbfOnRecv_;        // SetOnRecv
  CBFP_ERROR        m_cbfOnError_;       // SetOnError

#ifdef _WIN32
  HANDLE            m_hIocp { nullptr };          // CreateIoCompletionPort 핸들
#else
  int               m_iEpfd         { -1 };       // epoll_create1 fd
  int               m_iWakeupFd[2]  { -1, -1 };   // epoll 워커 깨우기 pipe
#endif
  // ---------------------------------------------------------------------------
};
