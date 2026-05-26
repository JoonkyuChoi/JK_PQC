/*----------------------------------------------------------------------------+-
SocketPool.cpp
-+----------------------------------------------------------------------------+-
Description	: Windows IOCP / Linux epoll 기반 IPv4/IPv6 소켓 풀 구현체
              서버(Listen/Accept) + 클라이언트(Connect) + 스케즐러 통합
Copyright		: 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/05/21] 추초 작성
-+----------------------------------------------------------------------------*/
#include "SocketPool.h"
#include <cassert>
#include <cstdio>
#include <algorithm>

// =============================================================================
// 플랫폼별 헬퍼
// =============================================================================
#ifdef _WIN32
// OS 소켓 핸들을 closesocket()으로 닫는 플랫폼 래퍼
static void sfCloseRaw(socket_t a_tSock) { closesocket(a_tSock); }
// Winsock 마지막 오류 코드(WSAGetLastError) 반환
static int  sfErrno   ()                 { return static_cast<int>(WSAGetLastError()); }
#else
#  include <cerrno>
// POSIX 소켓 fd를 close()로 닫는 플랫폼 래퍼
static void sfCloseRaw(socket_t a_tSock) { ::close(a_tSock); }
// errno 값 반환
static int  sfErrno   ()                 { return errno; }
#endif

// =============================================================================
// 생성자 / 소멸자
// =============================================================================
CSocketPool::CSocketPool(const T_CONFIG& a_tCfg)
  : m_tCfg(a_tCfg)
{
#ifdef _WIN32
  WSADATA l_tWd{};
  WSAStartup(MAKEWORD(2, 2), &l_tWd);
#endif
}

CSocketPool::~CSocketPool()
{
  Stop();
#ifdef _WIN32
  WSACleanup();
#endif
}

// =============================================================================
// Start : I/O 워커 + 스케즐러 쓰레드 시작
// =============================================================================
bool CSocketPool::Start()
{
  if (m_bRunning.exchange(true)) return true;

#ifdef _WIN32
  m_hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
  if (!m_hIocp) { m_bRunning = false; return false; }

  for (int l_i = 0; l_i < m_tCfg.m_iWorkerThreads; ++l_i)
    m_oWorkers.emplace_back([this]{ _WorkerIOCP(); });
#else
  m_iEpfd = epoll_create1(EPOLL_CLOEXEC);
  if (m_iEpfd < 0) { m_bRunning = false; return false; }

  if (pipe(m_iWakeupFd) != 0) { m_bRunning = false; return false; }

  // wakeup pipe를 epoll에 등록 (워커 종료 신호용)
  epoll_event l_tEv{};
  l_tEv.events  = EPOLLIN;
  l_tEv.data.fd = m_iWakeupFd[0];
  epoll_ctl(m_iEpfd, EPOLL_CTL_ADD, m_iWakeupFd[0], &l_tEv);

  for (int l_i = 0; l_i < m_tCfg.m_iWorkerThreads; ++l_i)
    m_oWorkers.emplace_back([this]{ _WorkerEpoll(); });
#endif

  // 스케즐러 시작
  m_oScheduler = std::thread([this]{ _SchedulerProc(); });
  return true;
}

// =============================================================================
// Stop : 전체 종료 및 소켓 정리
// =============================================================================
void CSocketPool::Stop()
{
  if (!m_bRunning.exchange(false)) return;

  StopListen();

#ifdef _WIN32
  for (size_t l_i = 0; l_i < m_oWorkers.size(); ++l_i)
    PostQueuedCompletionStatus(m_hIocp, 0, 0, nullptr);

  for (auto& l_oT : m_oWorkers)
    if (l_oT.joinable()) l_oT.join();
  m_oWorkers.clear();

  if (m_hIocp) { CloseHandle(m_hIocp); m_hIocp = nullptr; }
#else
  for (int l_i = 0; l_i < m_tCfg.m_iWorkerThreads; ++l_i)
  {
    char l_cB = 1;
    (void)write(m_iWakeupFd[1], &l_cB, 1);
  }
  for (auto& l_oT : m_oWorkers)
    if (l_oT.joinable()) l_oT.join();
  m_oWorkers.clear();

  if (m_iWakeupFd[0] >= 0) { ::close(m_iWakeupFd[0]); m_iWakeupFd[0] = -1; }
  if (m_iWakeupFd[1] >= 0) { ::close(m_iWakeupFd[1]); m_iWakeupFd[1] = -1; }
  if (m_iEpfd        >= 0) { ::close(m_iEpfd);         m_iEpfd        = -1; }
#endif

  if (m_oScheduler.joinable()) m_oScheduler.join();

  // 남은 소켓 전체 닫기
  std::lock_guard<std::mutex> l_oLk(m_oCtxMtx);
  for (auto& [l_tS, l_oCtx] : m_oCtxMap)
    sfCloseRaw(l_tS);
  m_oCtxMap.clear();
}

// =============================================================================
// Listen : IPv4/IPv6 리스닝 시작
// =============================================================================
bool CSocketPool::Listen(const std::string& a_oBindIp,
                         uint16_t           a_usPort,
                         int                a_iBacklog)
{
  if (!m_bRunning) return false;
  if (m_bListening.exchange(true)) return false;

  m_tListenSock = _CreateListenSocket(a_oBindIp, a_usPort, a_iBacklog);
  if (m_tListenSock == D_INVALID_SOCK)
  {
    m_bListening = false;
    return false;
  }

  m_oAcceptor = std::thread([this]{ _AcceptProc(); });
  return true;
}

// =============================================================================
// StopListen : 리스닝 소켓 종료
// =============================================================================
void CSocketPool::StopListen()
{
  if (!m_bListening.exchange(false)) return;

  if (m_tListenSock != D_INVALID_SOCK)
  {
    sfCloseRaw(m_tListenSock);
    m_tListenSock = D_INVALID_SOCK;
  }
  if (m_oAcceptor.joinable()) m_oAcceptor.join();
}

// =============================================================================
// Connect : TCP 접속 (IPv4 / IPv6 자동 선택)
// =============================================================================
socket_t CSocketPool::Connect(const std::string& a_oHost, uint16_t a_usPort)
{
  if (!m_bRunning) return D_INVALID_SOCK;

  addrinfo  l_tHints{};
  addrinfo* l_pRes = nullptr;
  l_tHints.ai_family   = AF_UNSPEC;
  l_tHints.ai_socktype = SOCK_STREAM;

  char l_szPort[8];
  snprintf(l_szPort, sizeof(l_szPort), "%u", a_usPort);

  if (getaddrinfo(a_oHost.c_str(), l_szPort, &l_tHints, &l_pRes) != 0)
    return D_INVALID_SOCK;

  socket_t    l_tSock   = D_INVALID_SOCK;
  std::string l_oPeerIp;

  for (addrinfo* l_pCur = l_pRes; l_pCur; l_pCur = l_pCur->ai_next)
  {
    l_tSock = ::socket(l_pCur->ai_family,
                       l_pCur->ai_socktype,
                       l_pCur->ai_protocol);
    if (l_tSock == D_INVALID_SOCK) continue;

    if (::connect(l_tSock, l_pCur->ai_addr,
                  static_cast<int>(l_pCur->ai_addrlen)) == 0)
    {
      sockaddr_storage l_tSs{};
      memcpy(&l_tSs, l_pCur->ai_addr, l_pCur->ai_addrlen);
      uint16_t l_usDummy = 0;
      _ResolvePeerAddr(l_tSs, l_oPeerIp, l_usDummy);
      break;
    }

    sfCloseRaw(l_tSock);
    l_tSock = D_INVALID_SOCK;
  }
  freeaddrinfo(l_pRes);

  if (l_tSock == D_INVALID_SOCK) return D_INVALID_SOCK;

  _ApplySocketOptions(l_tSock);

  if (!_RegisterContext(l_tSock, ROLE_CLIENT, l_oPeerIp, a_usPort))
  {
    sfCloseRaw(l_tSock);
    return D_INVALID_SOCK;
  }

  if (m_cbfOnConnect_) m_cbfOnConnect_(l_tSock);
  return l_tSock;
}

// =============================================================================
// Disconnect : 소켓 종료 API
// =============================================================================
void CSocketPool::Disconnect(socket_t a_tSock)
{
  _CloseSocket(a_tSock, true);
}

// =============================================================================
// RequestClose : IOCP 수신 콜백 안에서 호출 ? 콜백 반환 후 워커가 소켓 종료
// =============================================================================
void CSocketPool::RequestClose(socket_t a_tSock)
{
  auto l_oCtx = _FindContext(a_tSock);
  if (l_oCtx != nullptr)
  {
    l_oCtx->m_bClosePending.store(true);
  }
}

// =============================================================================
// Send : 데이터 송신
// =============================================================================
bool CSocketPool::Send(socket_t        a_tSock,
                       const uint8_t*  a_pucData,
                       size_t          a_ullLen)
{
  auto l_oCtx = _FindContext(a_tSock);
  if (!l_oCtx || !l_oCtx->m_bAlive) return false;

  std::lock_guard<std::mutex> l_oLk(l_oCtx->m_oSendMtx);

  // TLS 응답은 wbio에서 여러 번 flush될 수 있으므로, Overlapped WSASend 대신
  // 동기 send()로 전체 길이를 보장한다. (버퍼 덮어쓰기·미완료 송신 방지)
  size_t l_ullOff = 0;
  while (l_ullOff < a_ullLen)
  {
#ifdef _WIN32
    const int l_iSent = ::send(a_tSock,
                               reinterpret_cast<const char*>(a_pucData) + l_ullOff,
                               static_cast<int>(a_ullLen - l_ullOff),
                               0);
#else
    const ssize_t l_iSent = ::send(a_tSock,
                                   reinterpret_cast<const char*>(a_pucData) + l_ullOff,
                                   a_ullLen - l_ullOff,
                                   MSG_NOSIGNAL);
#endif
    if (l_iSent <= 0)
    {
      return false;
    }
    l_ullOff += static_cast<size_t>(l_iSent);
  }
  return true;
}

// =============================================================================
// GetConnectionCount : 리스닝 소켓 제외한 접속 수
// =============================================================================
size_t CSocketPool::GetConnectionCount() const
{
  std::lock_guard<std::mutex> l_oLk(m_oCtxMtx);
  return m_oCtxMap.size();
}

// =============================================================================
// GetPeerInfo : 상대방 IP / Port 조회
// =============================================================================
bool CSocketPool::GetPeerInfo(socket_t     a_tSock,
                              std::string& a_oIp,
                              uint16_t&    a_usPort) const
{
  auto l_oCtx = _FindContext(a_tSock);
  if (!l_oCtx) return false;
  a_oIp    = l_oCtx->m_oPeerIp;
  a_usPort = l_oCtx->m_usPeerPort;
  return true;
}

// =============================================================================
// _ApplySocketOptions : TCP_NODELAY / SO_KEEPALIVE 적용
// =============================================================================
bool CSocketPool::_ApplySocketOptions(socket_t a_tSock)
{
  int l_iOn = 1;
  if (m_tCfg.m_bTcpNoDelay)
    setsockopt(a_tSock, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&l_iOn), sizeof(l_iOn));
  if (m_tCfg.m_bKeepAlive)
    setsockopt(a_tSock, SOL_SOCKET, SO_KEEPALIVE,
               reinterpret_cast<const char*>(&l_iOn), sizeof(l_iOn));
  return true;
}

// =============================================================================
// BeginAcceptedSocket : Accept 직후 컨텍스트·IOCP 등록 (수신 시작 전)
// =============================================================================
bool CSocketPool::BeginAcceptedSocket(socket_t           a_tSock,
                                      const std::string& a_oIp,
                                      uint16_t           a_usPort)
{
  auto l_oCtx          = std::make_shared<T_SOCKET_CTX>();
  l_oCtx->m_tSock      = a_tSock;
  l_oCtx->m_eRole      = ROLE_ACCEPTED;
  l_oCtx->m_oPeerIp    = a_oIp;
  l_oCtx->m_usPeerPort = a_usPort;
  l_oCtx->m_bAlive     = true;
  l_oCtx->m_tLastRecv  = std::chrono::steady_clock::now();

#ifdef _WIN32
  l_oCtx->m_tRecvOp.m_pCtx  = l_oCtx.get();
  l_oCtx->m_tRecvOp.m_eType = T_SOCKET_CTX::T_IOCP_OP::eOpType::Recv;
  l_oCtx->m_tSendOp.m_pCtx  = l_oCtx.get();
  l_oCtx->m_tSendOp.m_eType = T_SOCKET_CTX::T_IOCP_OP::eOpType::Send;

  if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(a_tSock),
                              m_hIocp,
                              reinterpret_cast<ULONG_PTR>(l_oCtx.get()),
                              0))
  {
    return false;
  }
#else
  // 핸드셰이크 동안 blocking recv 사용 → epoll 등록은 StartRecvForSocket에서
#endif
  {
    std::lock_guard<std::mutex> l_oLk(m_oCtxMtx);
    m_oCtxMap[a_tSock] = l_oCtx;
  }
  return true;
}

// =============================================================================
// StartRecvForSocket : 비동기 수신(WSARecv/epoll) 시작
// =============================================================================
bool CSocketPool::StartRecvForSocket(socket_t a_tSock)
{
  auto l_oCtx = _FindContext(a_tSock);
  if (!l_oCtx || !l_oCtx->m_bAlive)
  {
    return false;
  }
#ifdef _WIN32
  return _PostRecv(*l_oCtx);
#else
  _SetNonBlocking(a_tSock);
  return _AddToEpoll(a_tSock);
#endif
}

// =============================================================================
// _RegisterContext : 컨텍스트 생성 + IOCP/epoll 등록 + 즉시 수신 시작
// =============================================================================
bool CSocketPool::_RegisterContext(socket_t           a_tSock,
                                   eSocketRole        a_eRole,
                                   const std::string& a_oIp,
                                   uint16_t           a_usPort)
{
  if (a_eRole == ROLE_ACCEPTED)
  {
    if (!BeginAcceptedSocket(a_tSock, a_oIp, a_usPort))
    {
      return false;
    }
    return StartRecvForSocket(a_tSock);
  }
  auto l_oCtx          = std::make_shared<T_SOCKET_CTX>();
  l_oCtx->m_tSock      = a_tSock;
  l_oCtx->m_eRole      = a_eRole;
  l_oCtx->m_oPeerIp    = a_oIp;
  l_oCtx->m_usPeerPort = a_usPort;
  l_oCtx->m_bAlive     = true;
  l_oCtx->m_tLastRecv  = std::chrono::steady_clock::now();

#ifdef _WIN32
  l_oCtx->m_tRecvOp.m_pCtx  = l_oCtx.get();
  l_oCtx->m_tRecvOp.m_eType = T_SOCKET_CTX::T_IOCP_OP::eOpType::Recv;
  l_oCtx->m_tSendOp.m_pCtx  = l_oCtx.get();
  l_oCtx->m_tSendOp.m_eType = T_SOCKET_CTX::T_IOCP_OP::eOpType::Send;

  if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(a_tSock),
                              m_hIocp,
                              reinterpret_cast<ULONG_PTR>(l_oCtx.get()),
                              0))
  {
    return false;
  }
  {
    std::lock_guard<std::mutex> l_oLk(m_oCtxMtx);
    m_oCtxMap[a_tSock] = l_oCtx;
  }
  return _PostRecv(*l_oCtx);
#else
  _SetNonBlocking(a_tSock);
  {
    std::lock_guard<std::mutex> l_oLk(m_oCtxMtx);
    m_oCtxMap[a_tSock] = l_oCtx;
  }
  return _AddToEpoll(a_tSock);
#endif
}

// =============================================================================
// _CloseSocket : 소켓 종료 + 컨텍스트 제거 + 콜백 호출
// =============================================================================
void CSocketPool::_CloseSocket(socket_t a_tSock, bool a_bNotify)
{
  std::shared_ptr<T_SOCKET_CTX> l_oCtx;
  {
    std::lock_guard<std::mutex> l_oLk(m_oCtxMtx);
    auto l_oIt = m_oCtxMap.find(a_tSock);
    if (l_oIt == m_oCtxMap.end()) return;
    l_oCtx = l_oIt->second;
    if (!l_oCtx->m_bAlive.exchange(false)) return;
    m_oCtxMap.erase(l_oIt);
  }

#ifndef _WIN32
  epoll_ctl(m_iEpfd, EPOLL_CTL_DEL, a_tSock, nullptr);
#endif
  sfCloseRaw(a_tSock);

  if (a_bNotify && m_cbfOnDisconnect_)
    m_cbfOnDisconnect_(a_tSock);
}

// =============================================================================
// _RemoveContext : 컨텍스트 맵에서만 제거
// =============================================================================
void CSocketPool::_RemoveContext(socket_t a_tSock)
{
  std::lock_guard<std::mutex> l_oLk(m_oCtxMtx);
  m_oCtxMap.erase(a_tSock);
}

// =============================================================================
// _FindContext : 소켓에 해당하는 컨텍스트 조회
// =============================================================================
std::shared_ptr<T_SOCKET_CTX> CSocketPool::_FindContext(socket_t a_tSock) const
{
  std::lock_guard<std::mutex> l_oLk(m_oCtxMtx);
  auto l_oIt = m_oCtxMap.find(a_tSock);
  return (l_oIt != m_oCtxMap.end()) ? l_oIt->second : nullptr;
}

// =============================================================================
// _ResolvePeerAddr : sockaddr_storage -> IP 문자열 + 포트 변환
// =============================================================================
bool CSocketPool::_ResolvePeerAddr(const sockaddr_storage& a_tSs,
                                   std::string&            a_oIp,
                                   uint16_t&               a_usPort)
{
  char l_szBuf[INET6_ADDRSTRLEN] = {};

  if (a_tSs.ss_family == AF_INET)
  {
    const sockaddr_in* l_pV4 =
      reinterpret_cast<const sockaddr_in*>(&a_tSs);
    inet_ntop(AF_INET, &l_pV4->sin_addr, l_szBuf, sizeof(l_szBuf));
    a_usPort = ntohs(l_pV4->sin_port);
  }
  else if (a_tSs.ss_family == AF_INET6)
  {
    const sockaddr_in6* l_pV6 =
      reinterpret_cast<const sockaddr_in6*>(&a_tSs);
    inet_ntop(AF_INET6, &l_pV6->sin6_addr, l_szBuf, sizeof(l_szBuf));
    a_usPort = ntohs(l_pV6->sin6_port);
  }
  else return false;

  a_oIp = l_szBuf;
  return true;
}

// =============================================================================
// _CreateListenSocket : IPv6 듀얼스택 리스닝 소켓 생성
//   "0.0.0.0" or IPv4 주소 -> AF_INET
//   "::" or 빈 문자열   -> AF_INET6 + 듀얼스택(설정에 따라)
// =============================================================================
socket_t CSocketPool::_CreateListenSocket(const std::string& a_oIp,
                                          uint16_t           a_usPort,
                                          int                a_iBacklog)
{
  bool l_bForceIpv4 = (a_oIp == "0.0.0.0" || (!a_oIp.empty() && a_oIp.find(':') == std::string::npos && a_oIp != "::"));
  int  l_iFamily    = l_bForceIpv4 ? AF_INET : AF_INET6;

  socket_t l_tSock  = ::socket(l_iFamily, SOCK_STREAM, IPPROTO_TCP);
  if (l_tSock == D_INVALID_SOCK) return D_INVALID_SOCK;

  int l_iOn = 1;
  setsockopt(l_tSock, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&l_iOn), sizeof(l_iOn));

  if (l_iFamily == AF_INET6)
  {
    int l_iV6Only = m_tCfg.m_bDualStack ? 0 : 1;
    setsockopt(l_tSock, IPPROTO_IPV6, IPV6_V6ONLY,
               reinterpret_cast<const char*>(&l_iV6Only), sizeof(l_iV6Only));
  }

  sockaddr_storage l_tAddr{};
  int              l_iAddrLen = 0;

  if (l_iFamily == AF_INET)
  {
    sockaddr_in* l_pV4   = reinterpret_cast<sockaddr_in*>(&l_tAddr);
    l_pV4->sin_family    = AF_INET;
    l_pV4->sin_port      = htons(a_usPort);
    if (a_oIp.empty() || a_oIp == "0.0.0.0")
      l_pV4->sin_addr.s_addr = INADDR_ANY;
    else
      inet_pton(AF_INET, a_oIp.c_str(), &l_pV4->sin_addr);
    l_iAddrLen = sizeof(sockaddr_in);
  }
  else
  {
    sockaddr_in6* l_pV6  = reinterpret_cast<sockaddr_in6*>(&l_tAddr);
    l_pV6->sin6_family   = AF_INET6;
    l_pV6->sin6_port     = htons(a_usPort);
    if (a_oIp.empty() || a_oIp == "::")
      l_pV6->sin6_addr = in6addr_any;
    else
      inet_pton(AF_INET6, a_oIp.c_str(), &l_pV6->sin6_addr);
    l_iAddrLen = sizeof(sockaddr_in6);
  }

  if (bind(l_tSock,
           reinterpret_cast<sockaddr*>(&l_tAddr),
           l_iAddrLen) != 0)
  {
    sfCloseRaw(l_tSock);
    return D_INVALID_SOCK;
  }

  if (listen(l_tSock, a_iBacklog) != 0)
  {
    sfCloseRaw(l_tSock);
    return D_INVALID_SOCK;
  }

#ifndef _WIN32
  _SetNonBlocking(l_tSock);
#endif

  return l_tSock;
}

// =============================================================================
// _AcceptProc : Accept 전용 쓰레드
// =============================================================================
void CSocketPool::_AcceptProc()
{
  while (m_bListening && m_bRunning)
  {
    sockaddr_storage l_tPeerAddr{};
#ifdef _WIN32
    int       l_iAddrLen = sizeof(l_tPeerAddr);
#else
    socklen_t l_iAddrLen = sizeof(l_tPeerAddr);
#endif

    socket_t l_tClient = ::accept(
      m_tListenSock,
      reinterpret_cast<sockaddr*>(&l_tPeerAddr),
      &l_iAddrLen);

    if (l_tClient == D_INVALID_SOCK)
    {
      if (!m_bListening || !m_bRunning) break;
      continue;
    }

    std::string l_oPeerIp;
    uint16_t    l_usPeerPort = 0;
    _ResolvePeerAddr(l_tPeerAddr, l_oPeerIp, l_usPeerPort);
    _ApplySocketOptions(l_tClient);

    // IOCP 등록 후 Accept 콜백에서 TLS 핸드셰이크(동기)를 마친 뒤 WSARecv 시작
    if (!BeginAcceptedSocket(l_tClient, l_oPeerIp, l_usPeerPort))
    {
      sfCloseRaw(l_tClient);
      continue;
    }
    if (m_cbfOnAccept_)
    {
      if (!m_cbfOnAccept_(l_tClient, l_oPeerIp, l_usPeerPort))
      {
        _CloseSocket(l_tClient, false);
        continue;
      }
    }
    if (!StartRecvForSocket(l_tClient))
    {
      _CloseSocket(l_tClient, false);
      continue;
    }
  }
}

// =============================================================================
// _SchedulerProc : 주기적 소켓 상태 점검
//   - m_bAlive == false 소켓 제거
//   - 무통신 타임아웃 감지 시 강제 종료
// =============================================================================
void CSocketPool::_SchedulerProc()
{
  while (m_bRunning)
  {
    std::this_thread::sleep_for(
      std::chrono::milliseconds(D_SP_SCHED_INTERVAL_MS));

    if (!m_bRunning) break;

    std::vector<socket_t> l_oKillList;
    auto l_tNow = std::chrono::steady_clock::now();

    {
      std::lock_guard<std::mutex> l_oLk(m_oCtxMtx);
      for (auto& [l_tS, l_oCtx] : m_oCtxMap)
      {
        if (!l_oCtx->m_bAlive)
        {
          l_oKillList.push_back(l_tS);
          continue;
        }

        if (m_tCfg.m_uiIdleTimeoutSec > 0)
        {
          auto l_llElapsed =
            std::chrono::duration_cast<std::chrono::seconds>(
              l_tNow - l_oCtx->m_tLastRecv).count();
          if (l_llElapsed >= static_cast<long long>(m_tCfg.m_uiIdleTimeoutSec))
            l_oKillList.push_back(l_tS);
        }
      }
    }

    for (socket_t l_tS : l_oKillList)
      _CloseSocket(l_tS, true);
  }
}

// =============================================================================
// ==========================  Windows IOCP 구현  ==============================
// =============================================================================
#ifdef _WIN32

bool CSocketPool::_PostRecv(T_SOCKET_CTX& a_tCtx)
{
  auto& l_tOp = a_tCtx.m_tRecvOp;
  memset(&l_tOp.m_tOvlp, 0, sizeof(l_tOp.m_tOvlp));
  l_tOp.m_tWsaBuf.buf = reinterpret_cast<char*>(l_tOp.m_ucBuf);
  l_tOp.m_tWsaBuf.len = D_SP_RECV_BUF_SIZE;
  l_tOp.m_eType       = T_SOCKET_CTX::T_IOCP_OP::eOpType::Recv;
  l_tOp.m_dwFlags     = 0;

  DWORD l_dwRecvd = 0;
  int   l_iRc     = WSARecv(a_tCtx.m_tSock,
                            &l_tOp.m_tWsaBuf, 1,
                            &l_dwRecvd, &l_tOp.m_dwFlags,
                            &l_tOp.m_tOvlp, nullptr);

  if (l_iRc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    return false;
  return true;
}

void CSocketPool::_WorkerIOCP()
{
  while (m_bRunning)
  {
    DWORD       l_dwBytes = 0;
    ULONG_PTR   l_ulKey   = 0;
    OVERLAPPED* l_pOvlp   = nullptr;
    // [JKC:20260521-180000] 소켓종료 감지 추가
    BOOL        l_bOk     = GetQueuedCompletionStatus(m_hIocp, &l_dwBytes, &l_ulKey, &l_pOvlp, INFINITE);
    
    // 종료 요청 확인
    if (!m_bRunning) break;

    // IOCP 자체 오류(타임아웃 등) -> 무시
    if (l_pOvlp == nullptr) continue;

    auto* l_pOp  = reinterpret_cast<T_SOCKET_CTX::T_IOCP_OP*>(l_pOvlp);
    auto* l_pCtx = l_pOp->m_pCtx;

    // GetQueuedCompletionStatus 실패 + l_pOvlp 유효
    // -> 소켓 I/O 오류 (강제 종료, 네트워크 끊김 등) -> 반드시 소켓 종료
    if (!l_bOk)
    {
      int l_iErr = static_cast<int>(WSAGetLastError());
      if (m_cbfOnError_)
        m_cbfOnError_(l_pCtx->m_tSock, l_iErr);
      _CloseSocket(l_pCtx->m_tSock, true);
      continue;
    }
    // 피어 정상 소켓종료 (FIN 수신)
    if (l_dwBytes == 0)
    {
      _CloseSocket(l_pCtx->m_tSock, true);
      continue;
    }
    // 수신 이벤트 발샘 : 콜백 호출 + 다음 수신 요청
    if (l_pOp->m_eType == T_SOCKET_CTX::T_IOCP_OP::eOpType::Recv)
    {
      l_pCtx->m_tLastRecv = std::chrono::steady_clock::now();
      if (m_cbfOnRecv_)
        m_cbfOnRecv_(l_pCtx->m_tSock, l_pOp->m_ucBuf, l_dwBytes);

      // 콜백에서 Disconnect()를 호출하면 컨텍스트가 파괴된 뒤 _PostRecv가
      // 실행되어 프로세스가 비정상 종료될 수 있으므로, RequestClose()만 허용한다.
      if (!l_pCtx->m_bAlive.load())
      {
        continue;
      }
      if (l_pCtx->m_bClosePending.load())
      {
        _CloseSocket(l_pCtx->m_tSock, true);
        continue;
      }

      // _PostRecv 실패 -> 소켓 오류로 간주하고 종료
      if (!_PostRecv(*l_pCtx))
      {
        int l_iErr = static_cast<int>(WSAGetLastError());
        if (m_cbfOnError_)
          m_cbfOnError_(l_pCtx->m_tSock, l_iErr);
        _CloseSocket(l_pCtx->m_tSock, true);
      }
    }
  }
}

// =============================================================================
// =====================  Linux epoll 구현  =======================================
// =============================================================================
#else

bool CSocketPool::_SetNonBlocking(socket_t a_tSock)
{
  int l_iFlags = fcntl(a_tSock, F_GETFL, 0);
  return fcntl(a_tSock, F_SETFL, l_iFlags | O_NONBLOCK) == 0;
}

bool CSocketPool::_AddToEpoll(socket_t a_tSock)
{
  epoll_event l_tEv{};
  l_tEv.events  = EPOLLIN | EPOLLET | EPOLLRDHUP;
  l_tEv.data.fd = a_tSock;
  return epoll_ctl(m_iEpfd, EPOLL_CTL_ADD, a_tSock, &l_tEv) == 0;
}

void CSocketPool::_HandleRead(T_SOCKET_CTX& a_tCtx)
{
  while (true)
  {
    ssize_t l_llN = recv(a_tCtx.m_tSock,
                         a_tCtx.m_ucRecvBuf,
                         D_SP_RECV_BUF_SIZE, 0);
    if (l_llN > 0)
    {
      a_tCtx.m_tLastRecv = std::chrono::steady_clock::now();
      if (m_cbfOnRecv_)
        m_cbfOnRecv_(a_tCtx.m_tSock,
                     a_tCtx.m_ucRecvBuf,
                     static_cast<size_t>(l_llN));
    }
    else if (l_llN == 0)
    {
      _CloseSocket(a_tCtx.m_tSock, true);
      return;
    }
    else
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      if (errno == EINTR) continue;
      if (m_cbfOnError_) m_cbfOnError_(a_tCtx.m_tSock, errno);
      _CloseSocket(a_tCtx.m_tSock, true);
      return;
    }
  }
}

void CSocketPool::_WorkerEpoll()
{
  epoll_event l_tEvents[D_SP_MAX_EPOLL_EVENTS];

  while (m_bRunning)
  {
    int l_iN = epoll_wait(m_iEpfd, l_tEvents, D_SP_MAX_EPOLL_EVENTS, -1);
    if (l_iN < 0)
    {
      if (errno == EINTR) continue;
      break;
    }

    for (int l_i = 0; l_i < l_iN; ++l_i)
    {
      int l_iFd = l_tEvents[l_i].data.fd;

      if (l_iFd == m_iWakeupFd[0]) return;

      auto l_oCtx = _FindContext(static_cast<socket_t>(l_iFd));
      if (!l_oCtx) continue;

      uint32_t l_uiEv = l_tEvents[l_i].events;

      if (l_uiEv & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
      {
        _CloseSocket(l_oCtx->m_tSock, true);
        continue;
      }

      if (l_uiEv & EPOLLIN)
        _HandleRead(*l_oCtx);
    }
  }
}

#endif  // _WIN32
// -----------------------------------------------------------------------------
