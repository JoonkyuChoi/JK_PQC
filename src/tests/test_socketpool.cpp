/*----------------------------------------------------------------------------+-
test_socketpool.cpp
-+----------------------------------------------------------------------------+-
Description	: CSocketPool 테스트 예제
              -h : 접속 서버 IP (Client 모드)
              -b : 리스닝 바인딩 IP (Server 모드)
              -p : 포트 (서버/클라이언트 공통)
              -t : 무통신 타임아웃 초 (0=비활성화, 기본값=0)

  사용예:
    [Server] test_socketpool -b :: -p 9000
    [Client] test_socketpool -h 127.0.0.1 -p 9000
    [Client] test_socketpool -h ::1 -p 9000

Copyright		: 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/05/21] 추초 작성
-+----------------------------------------------------------------------------*/
#include "SocketPool.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <string>
#include <thread>

#ifdef _WIN32
#  include <conio.h>
#else
#  include <unistd.h>
#endif

// =============================================================================
// 전역 변수
// =============================================================================
static std::atomic<bool>  g_bStop { false };   // 종료 신호 플래그
static CSocketPool*       g_pPool { nullptr }; // 에코서버용 전역 풀 포인터

// =============================================================================
// 헬퍼 함수
// =============================================================================

// -----------------------------------------------------------------------------
// sfPrintUsage : 사용법 출력
// -----------------------------------------------------------------------------
static void sfPrintUsage(const char* a_pszProg)
{
  printf("\n");
  printf("  사용법: %s [options]\n", a_pszProg);
  printf("\n");
  printf("  [Server 모드]\n");
  printf("    -b <bind_ip>  : 리스닝 바인딩 IP\n");
  printf("                    (IPv4: 0.0.0.0 | IPv6+듀얼스택: ::)\n");
  printf("    -p <port>     : 리스닝 포트\n");
  printf("    -t <sec>      : 무통신 타임아웃 (초, 0=비활성화)\n");
  printf("\n");
  printf("  [Client 모드]\n");
  printf("    -h <host>     : 접속 서버 IP 또는 호스트명\n");
  printf("    -p <port>     : 접속 포트\n");
  printf("\n");
  printf("  예시:\n");
  printf("    %s -b :: -p 9000         (IPv6 듀얼스택 서버)\n", a_pszProg);
  printf("    %s -b 0.0.0.0 -p 9000    (IPv4 서버)\n", a_pszProg);
  printf("    %s -h 127.0.0.1 -p 9000  (IPv4 클라이언트)\n", a_pszProg);
  printf("    %s -h ::1 -p 9000        (IPv6 클라이언트)\n", a_pszProg);
  printf("\n");
}

// -----------------------------------------------------------------------------
// sfSignalHandler : Ctrl+C 신호 처리
// -----------------------------------------------------------------------------
static void sfSignalHandler(int a_iSig)
{
  (void)a_iSig;
  printf("\n[INFO] 종료 신호 수신...\n");
  g_bStop = true;
}

// =============================================================================
// 콜백 함수 (Server 모드)
// =============================================================================

// -----------------------------------------------------------------------------
// cbfServerAccept : 클라이언트 접속 수락
// -----------------------------------------------------------------------------
static bool cbfServerAccept(socket_t        a_tSock,
                            const std::string& a_oIp,
                            uint16_t           a_usPort)
{
  printf("[INFO:RECV:TCP_:%05d] S Accept %s:%u\n",
         static_cast<int>(a_tSock), a_oIp.c_str(), a_usPort);
  return true;
}

// -----------------------------------------------------------------------------
// cbfServerDisconnect : 클라이언트 접속 해제
// -----------------------------------------------------------------------------
static void cbfServerDisconnect(socket_t a_tSock)
{
  printf("[INFO:SEND:TCP_:%05d] S Disconnect\n", static_cast<int>(a_tSock));
}

// -----------------------------------------------------------------------------
// cbfServerRecv : 수신 데이터 처리 (에코 반사)
// -----------------------------------------------------------------------------
static void cbfServerRecv(socket_t        a_tSock,
                          const uint8_t*  a_pucData,
                          size_t          a_ullLen)
{
  printf("[DBG_:RECV:TCP_:%05d] S len=%zu data=%.*s\n",
         static_cast<int>(a_tSock),
         a_ullLen,
         static_cast<int>(a_ullLen),
         reinterpret_cast<const char*>(a_pucData));

  // 에코: 수신한 데이터 원문을 그대로 반송
  if (g_pPool)
    g_pPool->Send(a_tSock, a_pucData, a_ullLen);
}

// -----------------------------------------------------------------------------
// cbfServerError : 에러 처리
// -----------------------------------------------------------------------------
static void cbfServerError(socket_t a_tSock, int a_iErr)
{
  printf("[ERR_:RECV:TCP_:%05d] S Error err=%d\n", static_cast<int>(a_tSock), a_iErr);
}

// =============================================================================
// 콜백 함수 (Client 모드)
// =============================================================================

// -----------------------------------------------------------------------------
// cbfClientConnect : 접속 성립
// -----------------------------------------------------------------------------
static void cbfClientConnect(socket_t a_tSock)
{
  printf("[INFO:RECV:TCP_:%05d] C Connected\n", static_cast<int>(a_tSock));
}

// -----------------------------------------------------------------------------
// cbfClientDisconnect : 접속 해제
// -----------------------------------------------------------------------------
static void cbfClientDisconnect(socket_t a_tSock)
{
  printf("[INFO:SEND:TCP_:%05d] C Disconnected\n", static_cast<int>(a_tSock));
}

// -----------------------------------------------------------------------------
// cbfClientRecv : 에코 응답 수신
// -----------------------------------------------------------------------------
static void cbfClientRecv(socket_t        a_tSock,
                          const uint8_t*  a_pucData,
                          size_t          a_ullLen)
{
  printf("[DBG_:RECV:TCP_:%05d] C len=%zu echo=%.*s\n",
         static_cast<int>(a_tSock),
         a_ullLen,
         static_cast<int>(a_ullLen),
         reinterpret_cast<const char*>(a_pucData));
}

// -----------------------------------------------------------------------------
// cbfClientError : 에러 처리
// -----------------------------------------------------------------------------
static void cbfClientError(socket_t a_tSock, int a_iErr)
{
  printf("[ERR_:RECV:TCP_:%05d] C Error err=%d\n", static_cast<int>(a_tSock), a_iErr);
}

// =============================================================================
// ufRunServer : 서버 모드 실행
// =============================================================================
static int ufRunServer(const std::string& a_oBindIp,
                       uint16_t           a_usPort,
                       uint32_t           a_uiIdleSec)
{
  printf("[INFO] Server 시작  bind=%s  port=%u  idle=%us\n", a_oBindIp.c_str(), a_usPort, a_uiIdleSec);

  CSocketPool::T_CONFIG l_tCfg;
  l_tCfg.m_iWorkerThreads   = 4;
  l_tCfg.m_bTcpNoDelay      = true;
  l_tCfg.m_bKeepAlive       = true;
  l_tCfg.m_bDualStack       = true;
  l_tCfg.m_uiIdleTimeoutSec = a_uiIdleSec;

  CSocketPool l_oPool(l_tCfg);
  g_pPool = &l_oPool;

  l_oPool.SetOnAccept    (cbfServerAccept);
  l_oPool.SetOnDisconnect(cbfServerDisconnect);
  l_oPool.SetOnRecv      (cbfServerRecv);
  l_oPool.SetOnError     (cbfServerError);

  if (!l_oPool.Start())
  {
    printf("[ERR_] Server Start() 실패\n");
    return 1;
  }

  if (!l_oPool.Listen(a_oBindIp, a_usPort))
  {
    printf("[ERR_] Server Listen() 실패  bind=%s port=%u\n", a_oBindIp.c_str(), a_usPort);
    l_oPool.Stop();
    return 1;
  }

  printf("[INFO] Server 대기 중... (Ctrl+C 로 종료)\n\n");

  while (!g_bStop)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    static int l_iCnt = 0;  // 접속 수 주기 출력용 루프 카운터 (10초마다)
    if (++l_iCnt % 20 == 0)   // 10초마다 접속 수 출력
      printf("[INFO] Server 현재 접속 수: %zu\n", l_oPool.GetConnectionCount());
  }

  l_oPool.Stop();
  g_pPool = nullptr;
  printf("[INFO] Server 종료\n");
  return 0;
}

// =============================================================================
// ufRunClient : 클라이언트 모드 실행
// =============================================================================
static int ufRunClient(const std::string& a_oHost,
                       uint16_t           a_usPort)
{
  printf("[INFO] Client 접속  host=%s  port=%u\n",
         a_oHost.c_str(), a_usPort);

  CSocketPool::T_CONFIG l_tCfg;
  l_tCfg.m_iWorkerThreads = 2;
  l_tCfg.m_bTcpNoDelay    = true;
  l_tCfg.m_bKeepAlive     = true;

  CSocketPool l_oPool(l_tCfg);
  g_pPool = &l_oPool;

  l_oPool.SetOnConnect   (cbfClientConnect);
  l_oPool.SetOnDisconnect(cbfClientDisconnect);
  l_oPool.SetOnRecv      (cbfClientRecv);
  l_oPool.SetOnError     (cbfClientError);

  if (!l_oPool.Start())
  {
    printf("[ERR_] Client Start() 실패\n");
    return 1;
  }

  socket_t l_tSock = l_oPool.Connect(a_oHost, a_usPort);
  if (l_tSock == D_INVALID_SOCK)
  {
    printf("[ERR_] Client Connect() 실패\n");
    l_oPool.Stop();
    return 1;
  }

  // 접속 포인트 에코 테스트
  printf("[INFO] Client 메시지를 송신합니다. 에코 응답을 확인하세요.\n");
  printf("[INFO] Client Ctrl+C 로 종료\n\n");

  int l_iSeq = 0;
  while (!g_bStop)
  {
    char l_szMsg[64];
    snprintf(l_szMsg, sizeof(l_szMsg), "Hello #%d from CSocketPool!", ++l_iSeq);

    if (!l_oPool.Send(l_tSock,
                      reinterpret_cast<const uint8_t*>(l_szMsg),
                      strlen(l_szMsg)))
    {
      printf("[WARN] Client Send 실패 또는 접속 종료\n");
      break;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  l_oPool.Disconnect(l_tSock);
  l_oPool.Stop();
  g_pPool = nullptr;
  printf("[INFO] Client 종료\n");
  return 0;
}

// =============================================================================
// main
// =============================================================================
int main(int a_iArgc, char** a_ppszArgv)
{
  // 신호 등록
  signal(SIGINT,  sfSignalHandler);
#ifndef _WIN32
  signal(SIGTERM, sfSignalHandler);
  signal(SIGPIPE, SIG_IGN);         // 파이프 에러 무시
#endif

  // ----------------------------------------
  // 인자 파싱
  // ----------------------------------------
  std::string l_strBindIp;
  std::string l_strHost;
  uint16_t    l_usPort      = 0;
  uint32_t    l_uiIdleSec   = 0;

  for (int l_i = 1; l_i < a_iArgc; ++l_i)
  {
    if (strcmp(a_ppszArgv[l_i], "-b") == 0 && l_i + 1 < a_iArgc)
      l_strBindIp = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-h") == 0 && l_i + 1 < a_iArgc)
      l_strHost   = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-p") == 0 && l_i + 1 < a_iArgc)
      l_usPort    = static_cast<uint16_t>(atoi(a_ppszArgv[++l_i]));
    else if (strcmp(a_ppszArgv[l_i], "-t") == 0 && l_i + 1 < a_iArgc)
      l_uiIdleSec = static_cast<uint32_t>(atoi(a_ppszArgv[++l_i]));
    else if (strcmp(a_ppszArgv[l_i], "--help") == 0 ||
             strcmp(a_ppszArgv[l_i], "-?") == 0)
    {
      sfPrintUsage(a_ppszArgv[0]);
      return 0;
    }
  }

  // ----------------------------------------
  // 모드 판단 및 실행
  // ----------------------------------------
  if (l_usPort == 0)
  {
    printf("[ERR_] -p 옵션이 필수입니다.\n");
    sfPrintUsage(a_ppszArgv[0]);
    return 1;
  }

  if (!l_strBindIp.empty() && l_strHost.empty())
  {
    // Server 모드: -b 있고 -h 없음
    return ufRunServer(l_strBindIp, l_usPort, l_uiIdleSec);
  }
  else if (l_strBindIp.empty() && !l_strHost.empty())
  {
    // Client 모드: -h 있고 -b 없음
    return ufRunClient(l_strHost, l_usPort);
  }
  else
  {
    printf("[ERR_] -b (Server) 또는 -h (Client) 중 하나만 지정하세요.\n");
    sfPrintUsage(a_ppszArgv[0]);
    return 1;
  }
}
