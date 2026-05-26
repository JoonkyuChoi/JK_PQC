/*----------------------------------------------------------------------------+-
jk-pqc-server.cpp
-+----------------------------------------------------------------------------+-
Description : CSocketPool + llhttp + OpenSSL Hybrid mTLS 고성능 HTTP 서버
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/05/24] 최초 작성
-+----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <csignal>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

// [JKC:20260428-1034] OPENSSL_Applink (DLL OpenSSL 사용 시)
#include <openssl/applink.c>

#include "pqchttps.h"
#include "pqc_utils.h"

// =============================================================================
// 전역 서버 컨텍스트
// =============================================================================
// jk-pqc-server 프로세스 전역: 소켓 풀·TLS·HTTP 파서·라우터 연동 상태
struct T_SERVER_CTX
{
  CSocketPool*                          m_pPool     { nullptr }; // IOCP TCP 소켓 풀
  CHybridTls*                           m_pTls      { nullptr }; // Hybrid mTLS 세션 관리
  CMtlsAuth*                            m_pAuth     { nullptr }; // 클라이언트 X.509 검증
  CHttpRouter*                          m_pRouter   { nullptr }; // HTTP 라우트 디스패처
  CPqcHttpService*                      m_pService  { nullptr }; // API·SHM 통계 서비스
  std::string                           m_strBindIp;              // Listen 바인딩 IP
  uint16_t                              m_usPort    { 18080 };    // Listen 포트
  std::mutex                            m_oParserMtx;             // m_oParserMap 보호
  std::unordered_map<socket_t,
    std::unique_ptr<CHttpParser>>       m_oParserMap;             // fd별 HTTP 파서
};

static std::atomic<bool>  g_bStop { false }; // Ctrl+C 등 종료 신호 (main 루프)
static T_SERVER_CTX       g_tCtx;            // 전역 서버 컨텍스트

// HTTP 파싱 완료 콜백 (CHttpParser::SetOnRequest, 전방 선언)
static void cbfOnHttpRequest(socket_t a_tSock, const T_HTTP_REQ& a_rReq);

// =============================================================================
// 헬퍼
// =============================================================================
// CLI 사용법·옵션 설명 출력
static void sfPrintUsage(const char* a_pszProg)
{
  printf("\n");
  printf("  사용법: %s [options]\n", a_pszProg);
  printf("\n");
  printf("    -b <ip>       바인딩 IP (:: = IPv6 듀얼스택, 0.0.0.0 = IPv4전용)\n");
  printf("    -p <port>     리스닝 포트 (기본값: 18080)\n");
  printf("    -c <file>     서버 인증서 파일 (.pem)\n");
  printf("    -k <file>     서버 개인키 파일 (.pem)\n");
  printf("    -a <file>     CA 인증서 파일 (.pem) - mTLS 클라이언트 검증용\n");
  printf("    -w <count>    워커 쓰레드 수 (기본값: 4)\n");
  printf("    -t <sec>      무통신 타임아웃 (초, 0=비활성화)\n");
  printf("    --help        사용법 출력\n");
  printf("\n");
}

// SIGINT/SIGTERM 수신 시 g_bStop 설정
static void sfSignalHandler(int a_iSig)
{
  (void)a_iSig;
  printf("\n[!] 종료 신호 수신...\n");
  g_bStop = true;
}

// 소켓 fd별 CHttpParser 조회·없으면 생성 후 m_oParserMap 등록
static CHttpParser* ufGetOrCreateParser(socket_t a_tSock)
{
  std::lock_guard<std::mutex> l_oLock(g_tCtx.m_oParserMtx);
  const auto l_oIt = g_tCtx.m_oParserMap.find(a_tSock);
  if (l_oIt != g_tCtx.m_oParserMap.end())
  {
    return l_oIt->second.get();
  }
  auto l_pParser = std::make_unique<CHttpParser>();
  if (!l_pParser->Init())
  {
    return nullptr;
  }
  l_pParser->SetSocket(a_tSock);
  l_pParser->SetOnRequest(cbfOnHttpRequest);
  CHttpParser* l_pRaw = l_pParser.get();
  g_tCtx.m_oParserMap[a_tSock] = std::move(l_pParser);
  return l_pRaw;
}

// 연결 종료 시 fd에 대응하는 HTTP 파서 map 항목 제거
static void ufRemoveParser(socket_t a_tSock)
{
  std::lock_guard<std::mutex> l_oLock(g_tCtx.m_oParserMtx);
  g_tCtx.m_oParserMap.erase(a_tSock);
}

// =============================================================================
// CSocketPool 콜백
// =============================================================================
// CSocketPool Accept 콜백: TLS 핸드셰이크·HTTP 파서 생성 (false면 등록 생략)
static bool cbfOnAccept(socket_t a_tSock,
                        const std::string& a_oIp,
                        uint16_t a_usPort)
{
  printf("[+] Accept  fd=%-5d  %s:%u\n", (int)a_tSock, a_oIp.c_str(), a_usPort);
  if (g_tCtx.m_pTls != nullptr)
  {
    if (!g_tCtx.m_pTls->AcceptHandshake(a_tSock))
    {
      fprintf(stderr, "[ERR_] TLS 세션 생성 실패 fd=%d\n", (int)a_tSock);
      return false;
    }
  }
  if (ufGetOrCreateParser(a_tSock) == nullptr)
  {
    fprintf(stderr, "[ERR_] HTTP 파서 생성 실패 fd=%d\n", (int)a_tSock);
    return false;
  }
  return true;
}

// CSocketPool Disconnect 콜백: TLS 세션·HTTP 파서 정리
static void cbfOnDisconnect(socket_t a_tSock)
{
  printf("[-] Disconnect  fd=%-5d\n", (int)a_tSock);
  printf("----------------------------------------\n");
  if (g_tCtx.m_pTls != nullptr)
  {
    g_tCtx.m_pTls->RemoveSession(a_tSock);
  }
  ufRemoveParser(a_tSock);
}

// CSocketPool Recv 콜백: 암호문을 CHybridTls::OnRecvRaw로 전달
static void cbfOnRecv(socket_t a_tSock, const uint8_t* a_pucData, size_t a_ullLen)
{
  if (g_tCtx.m_pTls == nullptr)
  {
    return;
  }
  if (!g_tCtx.m_pTls->OnRecvRaw(a_tSock, a_pucData, a_ullLen))
  {
    if (g_tCtx.m_pPool != nullptr)
    {
      g_tCtx.m_pPool->RequestClose(a_tSock);
    }
  }
}

// CSocketPool Error 콜백: I/O 오류 로그
static void cbfOnError(socket_t a_tSock, int a_iErr)
{
  fprintf(stderr, "[E] fd=%-5d  err=%d\n", (int)a_tSock, a_iErr);
}

// CHybridTls 평문 수신 콜백: llhttp 파싱·메시지 완료 시 RequestClose
static void cbfOnPlainRecv(socket_t a_tSock,
                           const uint8_t* a_pucData,
                           size_t a_ullLen)
{
  CHttpParser* l_pParser = ufGetOrCreateParser(a_tSock);
  if (l_pParser == nullptr)
  {
    return;
  }
  if (!l_pParser->Execute(a_pucData, a_ullLen))
  {
    if (g_tCtx.m_pPool != nullptr)
    {
      g_tCtx.m_pPool->RequestClose(a_tSock);
    }
    return;
  }
  // 요청 전체(헤더+바디)가 수신·처리된 경우에만 reset (분할 수신 POST 대응)
  if (l_pParser->ConsumeMessageComplete())
  {
    // Connection: close ? IOCP 콜백 반환 후 워커가 소켓 종료
    if (g_tCtx.m_pPool != nullptr)
    {
      g_tCtx.m_pPool->RequestClose(a_tSock);
    }
  }
}

// llhttp message_complete: CHttpRouter::Dispatch로 라우트 처리
static void cbfOnHttpRequest(socket_t a_tSock, const T_HTTP_REQ& a_rReq)
{
  if (g_tCtx.m_pRouter == nullptr || g_tCtx.m_pTls == nullptr)
  {
    return;
  }
  const size_t l_ullConn = (g_tCtx.m_pPool != nullptr)
    ? g_tCtx.m_pPool->GetConnectionCount() : 0u;
  g_tCtx.m_pRouter->Dispatch(a_tSock, a_rReq, *g_tCtx.m_pTls, l_ullConn);
}

// =============================================================================
// main
// =============================================================================
int main(int a_iArgc, char** a_ppszArgv)
{
  signal(SIGINT, sfSignalHandler);
#ifndef _WIN32
  signal(SIGTERM, sfSignalHandler);
  signal(SIGPIPE, SIG_IGN);
#endif

  std::string l_strBindIp   = "::";
  uint16_t    l_usPort      = 18080;
  std::string l_strCert     = "certs/server/server-cert.pem";
  std::string l_strKey      = "certs/server/server-key.pem";
  std::string l_strCa       = "certs/ca/ca-cert.pem";
  int         l_iWorkers    = D_SP_DEFAULT_WORKERS;
  uint32_t    l_uiIdleSec   = D_SP_DEFAULT_IDLE_SEC;

  for (int l_i = 1; l_i < a_iArgc; ++l_i)
  {
    if ((strcmp(a_ppszArgv[l_i], "--help") == 0) || (strcmp(a_ppszArgv[l_i], "-?") == 0))
    {
      sfPrintUsage(a_ppszArgv[0]);
      return 0;
    }
    else if (strcmp(a_ppszArgv[l_i], "-b") == 0 && (l_i + 1) < a_iArgc)
      l_strBindIp = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-p") == 0 && (l_i + 1) < a_iArgc)
      l_usPort = static_cast<uint16_t>(atoi(a_ppszArgv[++l_i]));
    else if (strcmp(a_ppszArgv[l_i], "-c") == 0 && (l_i + 1) < a_iArgc)
      l_strCert = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-k") == 0 && (l_i + 1) < a_iArgc)
      l_strKey = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-a") == 0 && (l_i + 1) < a_iArgc)
      l_strCa = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-w") == 0 && (l_i + 1) < a_iArgc)
      l_iWorkers = atoi(a_ppszArgv[++l_i]);
    else if (strcmp(a_ppszArgv[l_i], "-t") == 0 && (l_i + 1) < a_iArgc)
      l_uiIdleSec = static_cast<uint32_t>(atoi(a_ppszArgv[++l_i]));
    else
    {
      fprintf(stderr, "[ERR_] 알 수 없는 옵션: %s\n", a_ppszArgv[l_i]);
      sfPrintUsage(a_ppszArgv[0]);
      return 1;
    }
  }

  g_tCtx.m_strBindIp = l_strBindIp;
  g_tCtx.m_usPort    = l_usPort;

  CMtlsAuth       l_oAuth;
  CHybridTls      l_oTls;
  CHttpRouter     l_oRouter;
  CPqcHttpService l_oService;

  if (!l_oAuth.Init(l_strCa.c_str()))
  {
    return 1;
  }
  if (!l_oTls.Init(l_strCert.c_str(), l_strKey.c_str(), l_strCa.c_str()))
  {
    return 1;
  }
  if (!l_oService.Init())
  {
    return 1;
  }
  l_oTls.SetMtlsAuth(&l_oAuth);
  l_oTls.SetOnPlainRecv(cbfOnPlainRecv);
  l_oService.SetContext(&l_oTls, &l_oAuth, l_usPort);
  l_oService.RegisterRoutes(l_oRouter);

  CSocketPool::T_CONFIG l_tCfg;
  l_tCfg.m_iWorkerThreads   = l_iWorkers;
  l_tCfg.m_bTcpNoDelay      = true;
  l_tCfg.m_bKeepAlive       = true;
  l_tCfg.m_bDualStack       = true;
  l_tCfg.m_uiIdleTimeoutSec = l_uiIdleSec;

  CSocketPool l_oPool(l_tCfg);
  l_oTls.SetSocketPool(&l_oPool);

  g_tCtx.m_pPool    = &l_oPool;
  g_tCtx.m_pTls     = &l_oTls;
  g_tCtx.m_pAuth    = &l_oAuth;
  g_tCtx.m_pRouter  = &l_oRouter;
  g_tCtx.m_pService = &l_oService;

  l_oPool.SetOnAccept    (cbfOnAccept);
  l_oPool.SetOnDisconnect(cbfOnDisconnect);
  l_oPool.SetOnRecv      (cbfOnRecv);
  l_oPool.SetOnError     (cbfOnError);

  if (!l_oPool.Start())
  {
    fprintf(stderr, "[ERR_] CSocketPool::Start 실패\n");
    return 1;
  }
  if (!l_oPool.Listen(l_strBindIp, l_usPort))
  {
    fprintf(stderr, "[ERR_] Listen 실패  bind=%s port=%u\n",
            l_strBindIp.c_str(), l_usPort);
    l_oPool.Stop();
    return 1;
  }

  printf("jk-pqc-server v%s\n", D_PQCHTTPS_VERSION);
  printf("  bind=%s  port=%u  workers=%d  idle=%us\n",
         l_strBindIp.c_str(), l_usPort, l_iWorkers, l_uiIdleSec);
  printf("  cert=%s\n  key=%s\n  ca=%s\n",
         l_strCert.c_str(), l_strKey.c_str(), l_strCa.c_str());
  printf("  routes: GET /, GET /health, GET /info, POST /echo\n");
  printf("          GET /api/status, /api/session, /api/client/info, /api/stats\n");
  printf("          POST /api/echo\n");
  printf("대기 중... (Ctrl+C 로 종료)\n\n");

  int     l_iTick   = 0;
  size_t  l_ullConn = 0;
  while (!g_bStop)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
#if 0
    if (++l_iTick % 20 == 0)
    {
      printf("[stats] 접속 수: %zu\n", l_oPool.GetConnectionCount());
    }
#else
    if (l_ullConn != l_oPool.GetConnectionCount())
    {
      l_ullConn = l_oPool.GetConnectionCount();
      printf("[stats] 접속 수: %zu\n", l_ullConn);
    }
#endif
  }

  l_oPool.Stop();
  l_oService.Shutdown();
  g_tCtx.m_pPool    = nullptr;
  g_tCtx.m_pTls     = nullptr;
  g_tCtx.m_pAuth    = nullptr;
  g_tCtx.m_pRouter  = nullptr;
  g_tCtx.m_pService = nullptr;
  printf("jk-pqc-server 종료\n");
  return 0;
}
