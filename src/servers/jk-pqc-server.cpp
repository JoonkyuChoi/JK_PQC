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
#include <chrono>
#include <csignal>
#include <fstream>
#include <thread>

// [JKC:20260428-1034] OPENSSL_Applink (DLL OpenSSL 사용 시)
#include <openssl/applink.c>

#include <json.hpp>

#include "jk-pqc-server.h"
#include "pqc_utils.h"

// jk-pqc-server 기동 옵션 (CLI·config.json 공통)
struct T_RUN_OPTS
{
  std::string m_strBindIp     { "::" };
  uint16_t    m_usPort        { 18080 };
  int         m_iWorkers      { D_SP_DEFAULT_WORKERS };
  uint32_t    m_uiIdleSec     { D_SP_DEFAULT_IDLE_SEC };
  std::string m_strKemGroups;                                       // 빈 문자열 = OpenSSL 기본
  bool        m_bDualCert     { true };
  bool        m_bmTLS         { true };
  std::string m_strCert       { "certs/server/server-cert.pem" };
  std::string m_strKey        { "certs/server/server-key.pem" };
  std::string m_strCa         { "certs/ca/ca-cert.pem" };
  std::string m_strEcdsaCa    { "certs/ecdsa/ca/ca-cert.pem" };
  std::string m_strEcdsaCert  { "certs/ecdsa/server/server-cert.pem" };
  std::string m_strEcdsaKey   { "certs/ecdsa/server/server-key.pem" };
};

// =============================================================================
// 전역 서버 컨텍스트 정의 (선언은 jk-pqc-server.h)
// =============================================================================
static std::atomic<bool>  g_bStop { false }; // Ctrl+C 등 종료 신호 (main 루프)
T_SERVER_CTX              g_tCtx;            // 전역 서버 컨텍스트 (extern in .h)

// HTTP 파싱 완료 콜백 (CHttpParser::SetOnRequest, 전방 선언)
static void cbfOnHttpRequest(socket_t a_tSock, const T_HTTP_REQ& a_rReq);

// =============================================================================
// 헬퍼
// =============================================================================
// CLI 사용법·옵션 설명 출력
static void sfPrintUsage(const char* a_pszProg)
{
  printf("\n");
  printf("  Usage: %s [options]\n", a_pszProg);
  printf("\n");
  printf("    -f <file>       구성정보 파일 경로 (기본값: ./config.json)\n");
  printf("    -b <ip>         바인딩 IP (:: = IPv6 듀얼스택, 0.0.0.0 = IPv4전용)\n");
  printf("    -p <port>       리스닝 포트 (기본값: 18080)\n");
  printf("    -c <file>       서버 인증서 파일 (.pem)\n");
  printf("    -k <file>       서버 개인키 파일 (.pem)\n");
  printf("    -a <file>       CA 인증서 파일 (.pem) - mTLS 클라이언트 검증용 (ML-DSA)\n");
  printf("    -w <count>      워커 쓰레드 수 (기본값: 4)\n");
  printf("    -t <sec>        무통신 타임아웃 (초, 0=비활성화)\n");
  printf("    --kem <groups>  사용할 KEM 그룹 목록 (예: X25519MLKEM768:SecP256r1MLKEM768:X25519)\n");
  printf("    --single-cert   ML-DSA 인증서만 사용 (이중 인증서 비활성화)\n");
  printf("    --no-mtls       일반 TLS 동작 (클라이언트 인증서 불필요)\n");
  printf("    --help          사용법 출력\n");
  printf("\n");
  printf("  우선순위 : CLI 옵션 > config.json > 코드 기본값\n");
  printf("\n");
}

// config.json 파일을 읽어 a_rOpts 에 적용 (없거나 잘못된 키는 무시, 파싱 오류 시 false)
static bool sfLoadConfig(const std::string& a_strPath, T_RUN_OPTS& a_rOpts)
{
  std::ifstream l_oIfs(a_strPath, std::ios::binary);
  if (!l_oIfs.is_open())
  {
    return false;
  }
  nlohmann::json l_oJson;
  try
  {
    l_oIfs >> l_oJson;
  }
  catch (const std::exception& l_oEx)
  {
    printf("[ERR_] config 파싱 실패 (%s) : %s\n",
            a_strPath.c_str(), l_oEx.what());
    return false;
  }
  // 키별로 존재할 때만 덮어씀 (부분 설정 허용)
  if (l_oJson.contains("bind_ip")          && l_oJson["bind_ip"].is_string())          a_rOpts.m_strBindIp    = l_oJson["bind_ip"].get<std::string>();
  if (l_oJson.contains("port")             && l_oJson["port"].is_number_integer())     a_rOpts.m_usPort       = static_cast<uint16_t>(l_oJson["port"].get<int>());
  if (l_oJson.contains("workers")          && l_oJson["workers"].is_number_integer())  a_rOpts.m_iWorkers     = l_oJson["workers"].get<int>();
  if (l_oJson.contains("idle_timeout_sec") && l_oJson["idle_timeout_sec"].is_number_integer()) a_rOpts.m_uiIdleSec = static_cast<uint32_t>(l_oJson["idle_timeout_sec"].get<int>());
  if (l_oJson.contains("kem_groups")       && l_oJson["kem_groups"].is_string())       a_rOpts.m_strKemGroups = l_oJson["kem_groups"].get<std::string>();
  if (l_oJson.contains("dual_cert")        && l_oJson["dual_cert"].is_boolean())       a_rOpts.m_bDualCert    = l_oJson["dual_cert"].get<bool>();
  if (l_oJson.contains("mtls")             && l_oJson["mtls"].is_boolean())            a_rOpts.m_bmTLS        = l_oJson["mtls"].get<bool>();
  if (l_oJson.contains("cert")             && l_oJson["cert"].is_string())             a_rOpts.m_strCert      = l_oJson["cert"].get<std::string>();
  if (l_oJson.contains("key")              && l_oJson["key"].is_string())              a_rOpts.m_strKey       = l_oJson["key"].get<std::string>();
  if (l_oJson.contains("ca")               && l_oJson["ca"].is_string())               a_rOpts.m_strCa        = l_oJson["ca"].get<std::string>();
  if (l_oJson.contains("ecdsa_ca")         && l_oJson["ecdsa_ca"].is_string())         a_rOpts.m_strEcdsaCa   = l_oJson["ecdsa_ca"].get<std::string>();
  if (l_oJson.contains("ecdsa_cert")       && l_oJson["ecdsa_cert"].is_string())       a_rOpts.m_strEcdsaCert = l_oJson["ecdsa_cert"].get<std::string>();
  if (l_oJson.contains("ecdsa_key")        && l_oJson["ecdsa_key"].is_string())        a_rOpts.m_strEcdsaKey  = l_oJson["ecdsa_key"].get<std::string>();
  return true;
}

// 현재 a_tOpts 의 모든 항목을 a_strPath 에 저장 (2칸 들여쓰기 pretty)
static bool sfSaveConfig(const std::string& a_strPath, const T_RUN_OPTS& a_tOpts)
{
  nlohmann::ordered_json l_oJson;
  l_oJson["bind_ip"]          = a_tOpts.m_strBindIp;
  l_oJson["port"]             = static_cast<int>(a_tOpts.m_usPort);
  l_oJson["workers"]          = a_tOpts.m_iWorkers;
  l_oJson["idle_timeout_sec"] = static_cast<int>(a_tOpts.m_uiIdleSec);
  l_oJson["kem_groups"]       = a_tOpts.m_strKemGroups;
  l_oJson["dual_cert"]        = a_tOpts.m_bDualCert;
  l_oJson["mtls"]             = a_tOpts.m_bmTLS;
  l_oJson["cert"]             = a_tOpts.m_strCert;
  l_oJson["key"]              = a_tOpts.m_strKey;
  l_oJson["ca"]               = a_tOpts.m_strCa;
  l_oJson["ecdsa_ca"]         = a_tOpts.m_strEcdsaCa;
  l_oJson["ecdsa_cert"]       = a_tOpts.m_strEcdsaCert;
  l_oJson["ecdsa_key"]        = a_tOpts.m_strEcdsaKey;

  std::ofstream l_oOfs(a_strPath, std::ios::binary | std::ios::trunc);
  if (!l_oOfs.is_open())
  {
    printf("[ERR_] config 저장 실패 - 파일 열기 불가: %s\n", a_strPath.c_str());
    return false;
  }
  try
  {
    l_oOfs << l_oJson.dump(2) << "\n";
  }
  catch (const std::exception& l_oEx)
  {
    printf("[ERR_] config 저장 직렬화 실패 (%s) : %s\n",
            a_strPath.c_str(), l_oEx.what());
    return false;
  }
  return true;
}

// SIGINT/SIGTERM 수신 시 g_bStop 설정
static void sfSignalHandler(int a_iSig)
{
  (void)a_iSig;
  printf("\n[INFO] 종료 신호 수신...\n");
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
  printf("[INFO:RECV:TCP_:%05d] Accept %s:%u\n",
         static_cast<int>(a_tSock), a_oIp.c_str(), a_usPort);
  if (g_tCtx.m_pTls != nullptr)
  {
    if (!g_tCtx.m_pTls->AcceptHandshake(a_tSock))
    {
      printf("[ERR_:RECV:TCP_:%05d] TLS 세션 생성 실패\n",
              static_cast<int>(a_tSock));
      return false;
    }
    if (!g_tCtx.m_pTls->CompleteHandshakeBlocking(a_tSock))
    {
      // CompleteHandshakeBlocking 내부에서 peer alert 종류에 따라
      // [INFO] 또는 [ERR_]를 이미 출력한다. 여기서는 세션만 정리한다.
      g_tCtx.m_pTls->RemoveSession(a_tSock);
      return false;
    }
  }
  if (ufGetOrCreateParser(a_tSock) == nullptr)
  {
    printf("[ERR_:RECV:HTTP:%05d] HTTP 파서 생성 실패\n",
            static_cast<int>(a_tSock));
    return false;
  }
  return true;
}

// CSocketPool Disconnect 콜백: TLS 세션·HTTP 파서 정리
static void cbfOnDisconnect(socket_t a_tSock)
{
  printf("[INFO:SEND:TCP_:%05d] Disconnect\n", static_cast<int>(a_tSock));
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
#ifdef _DEBUG
  printf("[DBG_:RECV:TCP_:%05d] 암호문 %zu bytes\n",
         static_cast<int>(a_tSock), a_ullLen);
#endif
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
  printf("[ERR_:RECV:TCP_:%05d] err=%d\n",
          static_cast<int>(a_tSock), a_iErr);
}

// CHybridTls 평문 수신 콜백: llhttp 파싱·메시지 완료 시 RequestClose
static void cbfOnPlainRecv(socket_t a_tSock,
                           const uint8_t* a_pucData,
                           size_t a_ullLen)
{
#ifdef _DEBUG
  printf("[DBG_:RECV:HTTP:%05d] 평문 %zu bytes (TLS 복호화 완료)\n",
         static_cast<int>(a_tSock), a_ullLen);
#endif
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
#ifdef _DEBUG
  printf("[DBG_:RECV:HTTP:%05d] %s %s (body=%zuB)\n",
         static_cast<int>(a_tSock),
         a_rReq.m_strMethod.c_str(),
         a_rReq.m_strUrl.c_str(),
         a_rReq.m_strBody.size());
#endif
  if (g_tCtx.m_pRouter  == nullptr ||
      g_tCtx.m_pTls     == nullptr)
  {
    return;
  }
  const size_t  l_ullConn = (g_tCtx.m_pPool != nullptr) ? g_tCtx.m_pPool->GetConnectionCount() : 0u;
  const bool    l_bOk     =  g_tCtx.m_pRouter->Dispatch(a_tSock, a_rReq, *g_tCtx.m_pTls, l_ullConn);

#ifdef _DEBUG
  printf("[DBG_:SEND:HTTP:%05d] %s %s -> %s\n",
         static_cast<int>(a_tSock),
         a_rReq.m_strMethod.c_str(),
         a_rReq.m_strUrl.c_str(),
         l_bOk ? "200 OK 송신" : "404/500");
#endif
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

  T_RUN_OPTS  l_tOpts;
  std::string l_strConfigPath = "config.json";   // 기본 경로
  bool        l_bConfigExplicit = false;         // -f 로 명시되었는지

  // [1] -f 옵션 사전 스캔 (--help 도 우선 처리)
  for (int l_i = 1; l_i < a_iArgc; ++l_i)
  {
    if ((strcmp(a_ppszArgv[l_i], "--help") == 0) || (strcmp(a_ppszArgv[l_i], "-?") == 0))
    {
      sfPrintUsage(a_ppszArgv[0]);
      return 0;
    }
    if ((strcmp(a_ppszArgv[l_i], "-f") == 0) && ((l_i + 1) < a_iArgc))
    {
      l_strConfigPath   = a_ppszArgv[l_i + 1];
      l_bConfigExplicit = true;
    }
  }

  // [2] config.json 로드 (코드 기본값 위에 덮어쓰기)
  if (sfLoadConfig(l_strConfigPath, l_tOpts))
  {
    printf("[INFO] config 로드: %s\n", l_strConfigPath.c_str());
  }
  else if (l_bConfigExplicit)
  {
    printf("[ERR_] -f 로 지정한 config 파일을 열 수 없습니다: %s\n",
            l_strConfigPath.c_str());
    return 1;
  }
  else
  {
    printf("[INFO] config 파일 없음 (%s), 코드 기본값 사용\n", l_strConfigPath.c_str());
  }

  // [3] CLI 옵션 파싱 (config 위에 덮어쓰기 → 최종 우선순위 적용)
  for (int l_i = 1; l_i < a_iArgc; ++l_i)
  {
    if (strcmp(a_ppszArgv[l_i], "-f") == 0)
    {
      ++l_i; // 사전 스캔에서 처리, 값은 skip
    }
    else if (strcmp(a_ppszArgv[l_i], "--single-cert") == 0)
      l_tOpts.m_bDualCert = false;
    else if (strcmp(a_ppszArgv[l_i], "--no-mtls") == 0)
      l_tOpts.m_bmTLS = false;
    else if (strcmp(a_ppszArgv[l_i], "--kem") == 0)
    {
      if ((l_i + 1) >= a_iArgc)
      {
        printf("[ERR_] --kem 옵션에 값이 필요합니다.\n");
        return 1;
      }
      l_tOpts.m_strKemGroups = a_ppszArgv[++l_i];
    }
    else if (strcmp(a_ppszArgv[l_i], "-b") == 0 && (l_i + 1) < a_iArgc)
      l_tOpts.m_strBindIp = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-p") == 0 && (l_i + 1) < a_iArgc)
      l_tOpts.m_usPort = static_cast<uint16_t>(atoi(a_ppszArgv[++l_i]));
    else if (strcmp(a_ppszArgv[l_i], "-c") == 0 && (l_i + 1) < a_iArgc)
      l_tOpts.m_strCert = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-k") == 0 && (l_i + 1) < a_iArgc)
      l_tOpts.m_strKey = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-a") == 0 && (l_i + 1) < a_iArgc)
      l_tOpts.m_strCa = a_ppszArgv[++l_i];
    else if (strcmp(a_ppszArgv[l_i], "-w") == 0 && (l_i + 1) < a_iArgc)
      l_tOpts.m_iWorkers = atoi(a_ppszArgv[++l_i]);
    else if (strcmp(a_ppszArgv[l_i], "-t") == 0 && (l_i + 1) < a_iArgc)
      l_tOpts.m_uiIdleSec = static_cast<uint32_t>(atoi(a_ppszArgv[++l_i]));
    else
    {
      printf("[ERR_] 알 수 없는 옵션: %s\n", a_ppszArgv[l_i]);
      sfPrintUsage(a_ppszArgv[0]);
      return 1;
    }
  }

  g_tCtx.m_strBindIp = l_tOpts.m_strBindIp;
  g_tCtx.m_usPort    = l_tOpts.m_usPort;

  CMtlsAuth       l_oAuth;
  CHybridTls      l_oTls;
  CHttpRouter     l_oRouter;
  CPqcHttpService l_oService;

  if (l_tOpts.m_bmTLS)
  {
    if (!l_oAuth.Init(l_tOpts.m_strCa.c_str()))
    {
      return 1;
    }
  }
  if (l_tOpts.m_bDualCert)
  {
    l_oTls.SetEcdsaCertPaths(
      l_tOpts.m_strEcdsaCa.c_str(),
      l_tOpts.m_strEcdsaCert.c_str(),
      l_tOpts.m_strEcdsaKey.c_str());
  }
  const char* l_cpszKem = l_tOpts.m_strKemGroups.empty() ? nullptr : l_tOpts.m_strKemGroups.c_str();
  if (!l_oTls.Init(l_tOpts.m_strCert.c_str(), l_tOpts.m_strKey.c_str(), l_tOpts.m_strCa.c_str(),
                   l_cpszKem, l_tOpts.m_bDualCert, l_tOpts.m_bmTLS))
  {
    return 1;
  }
  if (!l_oService.Init())
  {
    return 1;
  }
  if (l_tOpts.m_bmTLS)
  {
    l_oTls.SetMtlsAuth(&l_oAuth);
  }
  l_oTls.SetOnPlainRecv(cbfOnPlainRecv);
  sfRegisterRoutes(l_oRouter, l_oService);

  CSocketPool::T_CONFIG l_tCfg;
  l_tCfg.m_iWorkerThreads   = l_tOpts.m_iWorkers;
  l_tCfg.m_bTcpNoDelay      = true;
  l_tCfg.m_bKeepAlive       = true;
  l_tCfg.m_bDualStack       = true;
  l_tCfg.m_uiIdleTimeoutSec = l_tOpts.m_uiIdleSec;

  CSocketPool l_oPool(l_tCfg);
  l_oTls.SetSocketPool(&l_oPool);

  g_tCtx.m_pPool    = &l_oPool;
  g_tCtx.m_pTls     = &l_oTls;
  g_tCtx.m_pAuth    = l_tOpts.m_bmTLS ? &l_oAuth : nullptr;
  g_tCtx.m_pRouter  = &l_oRouter;
  g_tCtx.m_pService = &l_oService;

  l_oPool.SetOnAccept    (cbfOnAccept);
  l_oPool.SetOnDisconnect(cbfOnDisconnect);
  l_oPool.SetOnRecv      (cbfOnRecv);
  l_oPool.SetOnError     (cbfOnError);

  if (!l_oPool.Start())
  {
    printf("[ERR_] CSocketPool::Start 실패\n");
    return 1;
  }
  if (!l_oPool.Listen(l_tOpts.m_strBindIp, l_tOpts.m_usPort))
  {
    printf("[ERR_] Listen 실패  bind=%s port=%u\n",
            l_tOpts.m_strBindIp.c_str(), l_tOpts.m_usPort);
    l_oPool.Stop();
    return 1;
  }

  printf("[INFO] jk-pqc-server v%s\n", D_PQCHTTPS_VERSION);
  printf("[INFO]   bind=%s  port=%u  workers=%d  idle=%us\n",
         l_tOpts.m_strBindIp.c_str(), l_tOpts.m_usPort,
         l_tOpts.m_iWorkers, l_tOpts.m_uiIdleSec);
  printf("[INFO]   Dual Cert  : %s\n", l_tOpts.m_bDualCert ? "ML-DSA + ECDSA" : "ML-DSA only");
  printf("[INFO]   mTLS       : %s\n", l_tOpts.m_bmTLS ? "enabled" : "disabled (TLS only)");
  if (l_tOpts.m_bmTLS)
  {
    printf("[INFO]   (참고: 브라우저 접속은 --no-mtls 또는 클라이언트 인증서 등록 필요)\n");
  }
  printf("[INFO]   KEM Groups : %s\n", l_oTls.GetKemGroups().c_str());
  printf("[INFO]   cert=%s\n", l_tOpts.m_strCert.c_str());
  printf("[INFO]   key=%s\n",  l_tOpts.m_strKey.c_str());
  printf("[INFO]   ca=%s\n",   l_tOpts.m_strCa.c_str());
  if (l_tOpts.m_bDualCert)
  {
    printf("[INFO]   ecdsa-ca=%s\n",   l_tOpts.m_strEcdsaCa.c_str());
    printf("[INFO]   ecdsa-cert=%s\n", l_tOpts.m_strEcdsaCert.c_str());
    printf("[INFO]   ecdsa-key=%s\n",  l_tOpts.m_strEcdsaKey.c_str());
  }
  printf("[INFO]   routes: GET /, GET /health, GET /info, POST /echo\n");
  printf("[INFO]           GET /api/status, /api/session, /api/client/info, /api/stats\n");
  printf("[INFO]           POST /api/echo\n");
  printf("[INFO] 대기 중... (Ctrl+C 로 종료)\n\n");

  int     l_iTick   = 0;
  size_t  l_ullConn = 0;
  while (!g_bStop)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
#if 0
    if (++l_iTick % 20 == 0)
    {
      printf("[INFO] 접속 수: %zu\n", l_oPool.GetConnectionCount());
    }
#else
    if (l_ullConn != l_oPool.GetConnectionCount())
    {
      l_ullConn = l_oPool.GetConnectionCount();
      printf("[INFO] 접속 수: %zu\n", l_ullConn);
    }
#endif
  }

  // 종료 직전 - 현재 구성 정보를 구동 시 로드한 config 파일에 저장
  // KEM 그룹은 사용자가 비워둔 경우 OpenSSL 이 실제 적용한 값으로 반영한다.
  if (l_tOpts.m_strKemGroups.empty())
  {
    l_tOpts.m_strKemGroups = l_oTls.GetKemGroups();
  }
  if (sfSaveConfig(l_strConfigPath, l_tOpts))
  {
    printf("[INFO] config 저장: %s\n", l_strConfigPath.c_str());
  }

  l_oPool.Stop();
  l_oService.Shutdown();
  g_tCtx.m_pPool    = nullptr;
  g_tCtx.m_pTls     = nullptr;
  g_tCtx.m_pAuth    = nullptr;
  g_tCtx.m_pRouter  = nullptr;
  g_tCtx.m_pService = nullptr;
  printf("[INFO] jk-pqc-server 종료\n");
  return 0;
}
// -----------------------------------------------------------------------------
