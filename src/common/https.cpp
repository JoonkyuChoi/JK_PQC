/*----------------------------------------------------------------------------+-
https.cpp
-+----------------------------------------------------------------------------+-
Description : "mTLS + PQC + HTTPS" 통신을 위한, 클래스들 구현
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
  [2026/05/14] KEM 기본값·그룹 목록 조회·SSL_CTX_set1_groups_list 오류 메시지 보강
  [2026/05/15] 이중 인증서: ClientHello 기반 ML-DSA·ECDSA 선택, SetEcdsaCertPaths
  [2026/05/15] signature_algorithms: ext 13 + 수동 파싱(SSL_client_hello_get0_ext)
-+----------------------------------------------------------------------------*/
#include "https.h"

#include <json.hpp>         // nlohmann::json

#include <stdio.h>
#include <cstdint>

#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/asn1.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

// OpenSSL 버전에 따라 ClientHello 콜백 반환 매크로가 없을 수 있음
#ifndef SSL_CLIENT_HELLO_SUCCESS
#define SSL_CLIENT_HELLO_SUCCESS 1
#endif
#ifndef SSL_CLIENT_HELLO_ERROR
#define SSL_CLIENT_HELLO_ERROR 0
#endif
// -----------------------------------------------------------------------------
// [VARIABLES]
// -----------------------------------------------------------------------------
// 브라우저 응답
static const char* const s_cpszRootHtml = R"(<!DOCTYPE html>
<html lang="ko">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>JK-PQC HTTPS Server</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      background: linear-gradient(135deg, #0a0e1a 0%, #0d1b2a 50%, #0a1628 100%);
      font-family: 'Segoe UI', 'Consolas', monospace;
      color: #e0f0ff;
    }
    .card {
      text-align: center;
      padding: 3rem 4rem;
      border: 1px solid rgba(0, 180, 255, 0.25);
      border-radius: 16px;
      background: rgba(10, 30, 60, 0.6);
      backdrop-filter: blur(12px);
      box-shadow: 0 0 40px rgba(0, 150, 255, 0.12),
                  inset 0 0 40px rgba(0, 100, 200, 0.05);
    }
    .badge {
      display: inline-block;
      font-size: 0.72rem;
      letter-spacing: 0.18em;
      color: #00c8ff;
      border: 1px solid rgba(0, 200, 255, 0.4);
      border-radius: 4px;
      padding: 0.25rem 0.75rem;
      margin-bottom: 1.6rem;
      text-transform: uppercase;
    }
    h1 {
      font-size: 1.9rem;
      font-weight: 600;
      letter-spacing: 0.04em;
      color: #ffffff;
      margin-bottom: 0.5rem;
    }
    .subtitle {
      font-size: 0.95rem;
      color: rgba(160, 210, 255, 0.7);
      margin-bottom: 2.2rem;
      letter-spacing: 0.06em;
    }
    .divider {
      width: 60px;
      height: 1px;
      background: linear-gradient(90deg, transparent, #00c8ff, transparent);
      margin: 0 auto 2rem;
    }
    .info-row {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 0.6rem;
      margin-bottom: 0.75rem;
      font-size: 0.9rem;
    }
    .info-label {
      color: rgba(0, 200, 255, 0.6);
      font-size: 0.75rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      width: 60px;
      text-align: right;
    }
    .info-value {
      color: #c8e8ff;
      letter-spacing: 0.03em;
    }
    .info-value a {
      color: #c8e8ff;
      text-decoration: none;
    }
    .info-value a:hover { color: #00c8ff; }
    .footer {
      margin-top: 2.2rem;
      font-size: 0.72rem;
      color: rgba(100, 160, 220, 0.4);
      letter-spacing: 0.1em;
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="badge">Post-Quantum Cryptography</div>
    <h1>JK-PQC HTTPS Server</h1>
    <p class="subtitle">ML-DSA-65 &nbsp;&middot;&nbsp; ML-KEM &nbsp;&middot;&nbsp; mTLS</p>
    <div class="divider"></div>
    <div class="info-row">
      <span class="info-label">Dev</span>
      <span class="info-value">Joonkyu Choi</span>
    </div>
    <div class="info-row">
      <span class="info-label">Mail</span>
      <span class="info-value"><a href="mailto:osoi@naver.com">osoi@naver.com</a></span>
    </div>
    <p class="footer">TLS 1.3 &nbsp;/&nbsp; X25519MLKEM768 &nbsp;/&nbsp; Dual Certificate</p>
  </div>
</body>
</html>)";
// -----------------------------------------------------------------------------
// CHttpsBase 클래스 구현
// -----------------------------------------------------------------------------
CHttpsBase::CHttpsBase()
{
  m_pSslCtx       = NULL;
  m_bWinsockInited= false;
  m_bDualCert     = false;
  m_bmTLS         = true; 
}

CHttpsBase::~CHttpsBase()
{
  if (m_pSslCtx != NULL)
  {
    SSL_CTX_free(m_pSslCtx);
    m_pSslCtx = NULL;
  }
  _winsock_Close();
}

// [JKC:20260516-1205] `mTLS` 사용 여부 파라미터 추가
// [JKC:20260516-0730] `이중 인증서` 사용 여부 파라미터 추가
bool CHttpsBase::Init(OSSL_LIB_CTX* a_ptLibCtx, const std::string& a_rstrKEM, bool a_bDualCert, bool a_bmTLS)
{
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
  // OQS Provider는 OpenSSL 컨텍스트 단위로 로드한다.
  if (!gfLoadProvider4OQS(a_ptLibCtx))
  {
    return false;
  }
  if (_winsock_Init() != 0)
  {
    fprintf(stderr, "[ERR_] WSAStartup 실패\n");
    return false;
  }
  m_bWinsockInited = true;
  if (m_pSslCtx != NULL)
  {
    SSL_CTX_free(m_pSslCtx);
    m_pSslCtx = NULL;
  }
  m_pSslCtx = SSL_CTX_new(createMethod());
  if (m_pSslCtx == NULL)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  // Hybrid KEM 그룹 (--kem 미지정 시 표준 기본 목록)
  m_strKEM = (a_rstrKEM.size() > 0) ? a_rstrKEM : "X25519MLKEM768:SecP256r1MLKEM768:X25519";
  if (!applyCommonTlsOptions())
  {
    return false;
  }
  
  m_bDualCert = a_bDualCert;  // [JKC:20260516-0730] `이중 인증서` 사용 여부
  m_bmTLS     = a_bmTLS;      // [JKC:20260516-1205] `mTLS` 사용 여부
  return true;
}

const std::string& CHttpsBase::GetKemGroups() const
{
  return m_strKEM;
}

void CHttpsBase::SetCertPaths(const std::string& a_rstrCaPath, const std::string& a_rstrCertPath, const std::string& a_rstrKeyPath)
{
  m_strCaPath   = a_rstrCaPath;
  m_strCertPath = a_rstrCertPath;
  m_strKeyPath  = a_rstrKeyPath;
}

void CHttpsBase::SetEcdsaCertPaths(const std::string& a_rstrCaPath, const std::string& a_rstrCertPath, const std::string& a_rstrKeyPath)
{
  m_strEcdsaCaPath    = a_rstrCaPath;
  m_strEcdsaCertPath  = a_rstrCertPath;
  m_strEcdsaKeyPath   = a_rstrKeyPath;
  // [JKC:20260516-0740] 주석 처리 : 외부(콘솔옵션) `이중 인증서` 사용 여부를 최우선 적용
//m_bDualCert         = true;
}

SSL_CTX* CHttpsBase::GetContext()
{
  return m_pSslCtx;
}

void CHttpsBase::PrintInfo4SSL(SSL* a_pSsl)
{
  gfPrintInfo4SSL(a_pSsl);
}

bool CHttpsBase::applyCommonTlsOptions()
{
  if (m_pSslCtx == NULL)
  {
    fprintf(stderr, "[ERR_] SSL_CTX 미초기화 상태\n");
    return false;
  }
  if (SSL_CTX_set1_groups_list(m_pSslCtx, m_strKEM.c_str()) != 1)
  {
    fprintf(stderr, "[ERR_] SSL_CTX_set1_groups_list 실패: %s\n", m_strKEM.c_str());
    ERR_print_errors_fp(stderr);
    return false;
  }
  return true;
}

int CHttpsBase::smfClientHelloCb(SSL* a_pSsl, int* a_piAlert, void* a_pvArg)
{
  CHttpsBase* l_pThis = static_cast<CHttpsBase*>(a_pvArg);
  // -------------------------------------
  // [JKC:20260515-1520] 식별 정보 추가
  // -------------------------------------
  // 연결 식별자: SSL 객체 포인터 하위 16비트
  const unsigned int  l_uiConnId  = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(a_pSsl) & 0xFFFFu);
  // 클라이언트 IP/Port (ClientHello 시점에서 이미 접근 가능)
  char                l_szPeerAddr[64] = "unknown";
  int                 l_iPeerPort = 0;
  const int           l_iFd       = SSL_get_fd(a_pSsl);

  if (l_iFd >= 0)
  {
    struct sockaddr_storage l_oAddr     = {};
    socklen_t               l_iAddrLen  = sizeof(l_oAddr);
    if (getpeername(l_iFd, reinterpret_cast<struct sockaddr*>(&l_oAddr), &l_iAddrLen) == 0)
    {
      if (l_oAddr.ss_family == AF_INET)
      {
        auto* l_pSin = reinterpret_cast<struct sockaddr_in*>(&l_oAddr);
        inet_ntop(AF_INET, &l_pSin->sin_addr, l_szPeerAddr, sizeof(l_szPeerAddr));
        l_iPeerPort = ntohs(l_pSin->sin_port);
      }
      else if (l_oAddr.ss_family == AF_INET6)
      {
        auto* l_pSin6 = reinterpret_cast<struct sockaddr_in6*>(&l_oAddr);
        inet_ntop(AF_INET6, &l_pSin6->sin6_addr, l_szPeerAddr, sizeof(l_szPeerAddr));
        l_iPeerPort = ntohs(l_pSin6->sin6_port);
      }
    }
  }
  // -------------------------------------
  // ClientHello에서 signature_algorithms 확장(13번)을 수동으로 파싱하여, ML-DSA 지원 여부를 확인한다.
  // -------------------------------------
  // ClientHello extension type 13: signature_algorithms (RFC 5246 / RFC 8446)
  // 바이트 구조: [2바이트 list 길이 big-endian][sigalg 2바이트 단위 …]
  const unsigned char* l_pucExt   = nullptr;
  size_t               l_ullExtLen= 0;
  const int            l_iHasSigAlgsExt = SSL_client_hello_get0_ext(a_pSsl, 13u, &l_pucExt, &l_ullExtLen);

  bool l_bSupportMlDsa = false;
  if ((l_iHasSigAlgsExt == 1) && (l_pucExt != nullptr) && (l_ullExtLen >= 2u))
  {
    const size_t l_ullListLen = (static_cast<size_t>(l_pucExt[0]) << 8) | static_cast<size_t>(l_pucExt[1]);
    if ((l_ullExtLen >= (2u + l_ullListLen)) && ((l_ullListLen % 2u) == 0u))
    {
      for (size_t l_ullI = 0u; l_ullI + 1u < l_ullListLen; l_ullI += 2u)
      {
        const uint16_t l_uiScheme = static_cast<uint16_t>(
          (static_cast<unsigned int>(l_pucExt[2u + l_ullI]) << 8) |
          static_cast<unsigned int>(l_pucExt[2u + l_ullI + 1u]));
        // ML-DSA-44: 0x0904 / ML-DSA-65: 0x0905 / ML-DSA-87: 0x0906
        if ((l_uiScheme == 0x0904u) || (l_uiScheme == 0x0905u) || (l_uiScheme == 0x0906u))
        {
          l_bSupportMlDsa = true;
          break;
        }
      }
    }
  }
  // -------------------------------------
  // 이중 인증서 모드: ML-DSA 지원 여부에 따라, 서버 인증서 선택
  // -------------------------------------
  const char* l_cpszCertPath  = nullptr;
  const char* l_cpszKeyPath   = nullptr;
  // [ML-DSA 지원 클라이언트] ML-DSA 인증서 선택
  if (l_bSupportMlDsa)
  {
    l_cpszCertPath  = l_pThis->m_strCertPath.c_str();
    l_cpszKeyPath   = l_pThis->m_strKeyPath.c_str();
    printf("[INFO:%s:%05d:%04X] Dual Cert : ML-DSA-65\n", l_szPeerAddr, l_iPeerPort, l_uiConnId);
  }
  // [ML-DSA 미지원 클라이언트] ECDSA 인증서 선택 (이중 인증서 모드인 경우에만)
  else
  {
    l_cpszCertPath  = l_pThis->m_strEcdsaCertPath.c_str();
    l_cpszKeyPath   = l_pThis->m_strEcdsaKeyPath.c_str();
    printf("[INFO:%s:%05d:%04X] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)\n", l_szPeerAddr, l_iPeerPort, l_uiConnId);
  }
  // 인증서 로드
  if ((SSL_use_certificate_file(a_pSsl, l_cpszCertPath, SSL_FILETYPE_PEM) != 1) ||
      (SSL_use_PrivateKey_file (a_pSsl, l_cpszKeyPath , SSL_FILETYPE_PEM) != 1))
  {
    fprintf(stderr, "[ERR_:%s:%05d:%04X] 이중 인증서 로드 실패: %s\n", l_szPeerAddr, l_iPeerPort, l_uiConnId, l_cpszCertPath);
    if (a_piAlert != nullptr)
      *a_piAlert = SSL_AD_INTERNAL_ERROR;
    return SSL_CLIENT_HELLO_ERROR;
  }
  // -------------------------------------
  return SSL_CLIENT_HELLO_SUCCESS;
}

bool CHttpsBase::loadCertificates(bool a_bRequirePeerCert)
{
  if (m_pSslCtx == NULL)
  {
    fprintf(stderr, "[ERR_] SSL_CTX 미초기화 상태\n");
    return false;
  }
  if (m_strCaPath.empty() || m_strCertPath.empty() || m_strKeyPath.empty())
  {
    fprintf(stderr, "[ERR_] 인증서 경로 미설정\n");
    return false;
  }
  if (m_bDualCert && (m_strEcdsaCertPath.empty() || m_strEcdsaKeyPath.empty()))
  {
    fprintf(stderr, "[ERR_] ECDSA 인증서 경로 미설정\n");
    return false;
  }
  // mTLS 설정: 클라이언트 인증서 요구 여부에 따라 검증 모드를 설정한다.
  const int l_iVerifyMode = a_bRequirePeerCert ? (SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT) : SSL_VERIFY_PEER;
  SSL_CTX_set_verify(m_pSslCtx, l_iVerifyMode, NULL);
  if (SSL_CTX_load_verify_locations(m_pSslCtx, m_strCaPath.c_str(), NULL) != 1)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  if (m_bDualCert)
  {
    if (SSL_CTX_use_certificate_file(m_pSslCtx, m_strEcdsaCertPath.c_str(), SSL_FILETYPE_PEM) != 1)
    {
      ERR_print_errors_fp(stderr);
      return false;
    }
    if (SSL_CTX_use_PrivateKey_file(m_pSslCtx, m_strEcdsaKeyPath.c_str(), SSL_FILETYPE_PEM) != 1)
    {
      ERR_print_errors_fp(stderr);
      return false;
    }
  }
  else
  {
    if (SSL_CTX_use_certificate_file(m_pSslCtx, m_strCertPath.c_str(), SSL_FILETYPE_PEM) != 1)
    {
      ERR_print_errors_fp(stderr);
      return false;
    }
    if (SSL_CTX_use_PrivateKey_file(m_pSslCtx, m_strKeyPath.c_str(), SSL_FILETYPE_PEM) != 1)
    {
      ERR_print_errors_fp(stderr);
      return false;
    }
  }
  if (SSL_CTX_check_private_key(m_pSslCtx) != 1)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  return true;
}

int CHttpsBase::_winsock_Init()
{
#ifdef _MSC_VER
  WSADATA l_tWsaData = { 0 };
  return WSAStartup(MAKEWORD(2, 2), &l_tWsaData);
#else
  return 0;
#endif
}

void CHttpsBase::_winsock_Close()
{
#ifdef _MSC_VER
  if (m_bWinsockInited)
  {
    WSACleanup();
    m_bWinsockInited = false;
  }
#endif
}

// -----------------------------------------------------------------------------
// CHttpsServer 클래스 구현
// -----------------------------------------------------------------------------
CHttpsServer::CHttpsServer()
  : m_bRunStatsLoop(false)
{
}

CHttpsServer::~CHttpsServer()
{
  m_bRunStatsLoop.store(false);
  if (m_oStatsThread.joinable())
  {
    m_oStatsThread.join();
  }
  m_oShmStatsProducer.Shutdown();
}

const SSL_METHOD* CHttpsServer::createMethod()
{
  return TLS_server_method();
}

bool CHttpsServer::Run(int a_iPort)
{
  if (m_bDualCert)
  {
    if (m_strCaPath.empty() || m_strCertPath.empty() || m_strKeyPath.empty() ||
        m_strEcdsaCaPath.empty() || m_strEcdsaCertPath.empty() || m_strEcdsaKeyPath.empty())
    {
      fprintf(stderr, "[ERR_] 이중 인증서 경로 미설정\n");
      return false;
    }
  }
  else if (m_strCaPath.empty() || m_strCertPath.empty() || m_strKeyPath.empty())
  {
    fprintf(stderr, "[ERR_] 인증서 경로 미설정\n");
    return false;
  }
  // -------------------------------------
  // [JKC:20260515-1300] 오류 해결 : 코드 수정
  // -------------------------------------
  // ClientHello 콜백에서 인증서를 선택하기 위해, SSLServer 생성 시 콜백 기반 설정을 사용
  auto setup_callback = [&](SSL_CTX& ctx) -> bool {
    // 기본 인증서 로드 (ClientHelloCb에서 교체되므로 placeholder)
    if (SSL_CTX_use_certificate_file(&ctx, m_strCertPath.c_str(), SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_use_PrivateKey_file (&ctx, m_strKeyPath.c_str() , SSL_FILETYPE_PEM) != 1)
      return false;
    // 이중 인증서 모드: ClientHello 콜백 등록
    if (m_bDualCert)
      SSL_CTX_set_client_hello_cb(&ctx, smfClientHelloCb, this);  // SSL_CTX_set_cert_cb 제거 - smfClientHelloCb가 담당
    
    // [JKC:20260516-0550] 접속/인증/암호화 완료에 대한, SSL 콜백함수 등록
    SSL_CTX_set_info_callback(&ctx, [](const SSL* ssl, int where, int ret)
    {
      // TLS 핸드셰이크 완료
      if ((where & SSL_CB_HANDSHAKE_DONE) != 0)
      {
        printf("----------------------------------------\n");
        printf("[ACPT] %s Handshake done\n", SSL_get_version(ssl));
      }
    });

    // mTLS 설정
    if (m_bmTLS)
    {
      // -----------------
      // [mTLS] ML-DSA CA + ECDSA CA 모두 로드
      // -----------------
      // [MLDSA] CA 로드
      if (SSL_CTX_load_verify_locations(&ctx, m_strCaPath.c_str(), nullptr) != 1)
        return false;
      // [ECDSA] CA 추가 로드 (브라우저 클라이언트 인증서 검증용)
      if (m_bDualCert && !m_strEcdsaCaPath.empty())
      {
        if (SSL_CTX_load_verify_locations(&ctx, m_strEcdsaCaPath.c_str(), nullptr) != 1)
          return false;
      }
      // -----------------
      SSL_CTX_set_verify(&ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    }
    else
    {
      // [일반 TLS] 클라이언트 인증서 검증 없음
      SSL_CTX_set_verify(&ctx, SSL_VERIFY_NONE, nullptr);
    }

    return true;
  };

  httplib::SSLServer l_oServer(setup_callback);
  // -------------------------------------
  if (!l_oServer.is_valid())
  {
    fprintf(stderr, "[ERR_] httplib::SSLServer 초기화 실패\n");
    ERR_print_errors_fp(stderr);
    return false;
  }
  SSL_CTX* l_pSslCtx = static_cast<SSL_CTX*>(l_oServer.tls_context());
  if (l_pSslCtx == NULL)
  {
    fprintf(stderr, "[ERR_] SSL_CTX 획득 실패\n");
    return false;
  }
  /*------------------------------------+-
  KEM 그룹 설정 (setup_callback 시점에는 불가)
  -+------------------------------------+-
  - KEM 설정을 setup_callback 밖에서 해야 하는 이유
    > SSL_CTX_set1_groups_list()는 m_strKEM 문자열이 필요한데, 이건 setup_callback 안에서도 접근 가능([&] 캡처)하므로, 사실 안으로 넣어도 됩니다.
    > 다만 실패 시, ERR_print_errors_fp 출력 후, return false 하는 패턴이 setup_callback의 bool 반환과 잘 맞으므로, 그대로 밖에 두는 게 가독성상 낫습니다.
  -+------------------------------------*/
  if (SSL_CTX_set1_groups_list(l_pSslCtx, m_strKEM.c_str()) != 1)
  {
    fprintf(stderr, "[ERR_] SSL_CTX_set1_groups_list 실패: %s\n", m_strKEM.c_str());
    ERR_print_errors_fp(stderr);
    return false;
  }
  if (!m_oShmStatsProducer.Init())
  {
    fprintf(stderr, "[ERR_] CShmStatsProducer 초기화 실패\n");
    return false;
  }
  // -------------------------------------
  // 통계 업데이트 쓰레드 시작
  // -------------------------------------
  m_bRunStatsLoop.store(true);
  m_oStatsThread = std::thread(&CHttpsServer::_updateStatsLoop, this);
  // -------------------------------------
  // [JKC:20260516-0520] 연결당 최초요청 전처리 콜백 설정
  // -------------------------------------
  l_oServer.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res)
  {
    printf("[REQ_:%s:%05d] %s %s\n", req.remote_addr.c_str(), req.remote_port, req.method.c_str(), req.path.c_str());
    return httplib::Server::HandlerResponse::Unhandled;
  });
  // -----------------------------------------------------------------------------
  // 경로 라우팅 핸들러 설정
  // -----------------------------------------------------------------------------
  // API : 루트 (브라우저 접속 시 HTML, API 클라이언트 접속 시 간단 텍스트)
  // -------------------------------------
  l_oServer.Get("/", [this](const httplib::Request& a_rReq, httplib::Response& a_rRes)
  {
    m_oShmStatsProducer.OnConnect();
    m_oShmStatsProducer.AddRequest();
    m_oShmStatsProducer.AddRxBytes(static_cast<uint64_t>(a_rReq.body.size()));

    SSL* l_pSsl = (SSL*)a_rReq.ssl;
    if (l_pSsl != NULL)
    {
      PrintInfo4SSL(l_pSsl);
    }
    // -----------------
    // [JKC:20260516-0634] 응답 콘텐츠 수정
    // -----------------
    const std::string&  l_strUA     = a_rReq.get_header_value("User-Agent");
    const std::string&  l_strAccept = a_rReq.get_header_value("Accept");
    const bool          l_bIsBrowser=
      l_strUA.find("Mozilla") != std::string::npos ||
      l_strAccept.find("text/html") != std::string::npos;

    if (l_bIsBrowser)
      a_rRes.set_content(s_cpszRootHtml, "text/html");
    else
      a_rRes.set_content("Hello from PQC (HTTPS) Server", "text/plain");
    // -----------------
    m_oShmStatsProducer.AddTxBytes(static_cast<uint64_t>(a_rRes.body.size()));
    m_oShmStatsProducer.OnDisconnect();
  });
  // -------------------------------------
  // API : 상태 조회
  // -------------------------------------
  l_oServer.Get("/api/status", [this](const httplib::Request& a_rReq, httplib::Response& a_rRes)
  {
    m_oShmStatsProducer.OnConnect();
    m_oShmStatsProducer.AddRequest();
    m_oShmStatsProducer.AddRxBytes(static_cast<uint64_t>(a_rReq.body.size()));

    SSL* l_pSsl = (SSL*)a_rReq.ssl;
    std::string l_strCipher = "N/A";
    if (l_pSsl != NULL)
    {
      const SSL_CIPHER* l_pCipher = SSL_get_current_cipher(l_pSsl);
      if (l_pCipher != NULL)
      {
        l_strCipher = SSL_CIPHER_get_name(l_pCipher);
      }
    }
    
    nlohmann::json  l_oJson;
    std::string     l_strJson;
    
    l_oJson["status"    ] = "ok";
    l_oJson["server"    ] = "JK-PQC HTTPS Server";
    l_oJson["kem"       ] = m_strKEM.c_str();
    l_oJson["signature" ] = "ML-DSA-65";
    l_oJson["cipher"    ] = l_strCipher;
    l_strJson             = l_oJson.dump();
    a_rRes.set_content(l_strJson, "application/json");
    m_oShmStatsProducer.AddTxBytes(static_cast<uint64_t>(a_rRes.body.size()));
    m_oShmStatsProducer.OnDisconnect();
  });
  // -------------------------------------
  // API : 세션/트래픽 통계 조회
  // -------------------------------------
  l_oServer.Get("/api/session", [this](const httplib::Request& a_rReq, httplib::Response& a_rRes)
  {
    m_oShmStatsProducer.OnConnect();
    m_oShmStatsProducer.AddRequest();
    m_oShmStatsProducer.AddRxBytes(static_cast<uint64_t>(a_rReq.body.size()));

    SSL* l_pSsl = (SSL*)a_rReq.ssl;
    std::string l_strKemGroup = "N/A";
    std::string l_strCipherSuite = "N/A";
    std::string l_strTlsVersion = "N/A";
    if (l_pSsl != NULL)
    {
      const char* l_cpszGroupName = SSL_get0_group_name(l_pSsl);
      if ((l_cpszGroupName != NULL) && (l_cpszGroupName[0] != '\0'))
      {
        l_strKemGroup = l_cpszGroupName;
      }
      const SSL_CIPHER* l_pCipher = SSL_get_current_cipher(l_pSsl);
      if (l_pCipher != NULL)
      {
        l_strCipherSuite = SSL_CIPHER_get_name(l_pCipher);
      }
      const char* l_cpszVersion = SSL_get_version(l_pSsl);
      if (l_cpszVersion != NULL)
      {
        l_strTlsVersion = l_cpszVersion;
      }
    }
    nlohmann::json  l_oJson;
    std::string     l_strJson;

    l_oJson["kem_group"   ] = l_strKemGroup;
    l_oJson["cipher_suite"] = l_strCipherSuite;
    l_oJson["tls_version" ] = l_strTlsVersion;
    l_strJson               = l_oJson.dump();
    a_rRes.set_content(l_strJson, "application/json");
    m_oShmStatsProducer.AddTxBytes(static_cast<uint64_t>(a_rRes.body.size()));
    m_oShmStatsProducer.OnDisconnect();
  });
  // -------------------------------------
  // API : 클라이언트 요청/응답 통계 조회
  // -------------------------------------
  l_oServer.Get("/api/client/info", [this](const httplib::Request& a_rReq, httplib::Response& a_rRes)
  {
    m_oShmStatsProducer.OnConnect();
    m_oShmStatsProducer.AddRequest();
    m_oShmStatsProducer.AddRxBytes(static_cast<uint64_t>(a_rReq.body.size()));

    SSL* l_pSsl = (SSL*)a_rReq.ssl;
    std::string l_strSubject = "N/A";
    std::string l_strNotBefore = "N/A";
    std::string l_strNotAfter = "N/A";
    if (l_pSsl != NULL)
    {
      X509* l_pPeerCert = SSL_get_peer_certificate(l_pSsl);
      if (l_pPeerCert != NULL)
      {
        X509_NAME* l_pSubjectName = X509_get_subject_name(l_pPeerCert);
        if (l_pSubjectName != NULL)
        {
          char l_caSubjectBuffer[512] = { 0 };
          X509_NAME_oneline(l_pSubjectName, l_caSubjectBuffer, static_cast<int>(sizeof(l_caSubjectBuffer)));
          l_strSubject = l_caSubjectBuffer;
        }
        const ASN1_TIME* l_pNotBefore = X509_get0_notBefore(l_pPeerCert);
        const ASN1_TIME* l_pNotAfter = X509_get0_notAfter(l_pPeerCert);
        struct tm l_tTimeData = { 0 };
        if ((l_pNotBefore != NULL) && (ASN1_TIME_to_tm(l_pNotBefore, &l_tTimeData) == 1))
        {
          std::ostringstream l_oSs;
          l_oSs << std::setfill('0') << std::setw(4) << (l_tTimeData.tm_year + 1900) << "-"
            << std::setw(2) << (l_tTimeData.tm_mon + 1) << "-"
            << std::setw(2) << l_tTimeData.tm_mday;
          l_strNotBefore = l_oSs.str();
        }
        if ((l_pNotAfter != NULL) && (ASN1_TIME_to_tm(l_pNotAfter, &l_tTimeData) == 1))
        {
          std::ostringstream l_oSs;
          l_oSs << std::setfill('0') << std::setw(4) << (l_tTimeData.tm_year + 1900) << "-"
            << std::setw(2) << (l_tTimeData.tm_mon + 1) << "-"
            << std::setw(2) << l_tTimeData.tm_mday;
          l_strNotAfter = l_oSs.str();
        }
        X509_free(l_pPeerCert);
      }
    }
    nlohmann::json  l_oJson;
    std::string     l_strJson;

    l_oJson["subject"   ] = l_strSubject;
    l_oJson["not_before"] = l_strNotBefore;
    l_oJson["not_after" ] = l_strNotAfter;
    l_strJson             = l_oJson.dump();
    a_rRes.set_content(l_strJson, "application/json");
    m_oShmStatsProducer.AddTxBytes(static_cast<uint64_t>(a_rRes.body.size()));
    m_oShmStatsProducer.OnDisconnect();
  });
  // -------------------------------------
  // API : 통계 정보 조회
  // -------------------------------------
  l_oServer.Get("/api/stats", [this](const httplib::Request& a_rReq, httplib::Response& a_rRes)
  {
    m_oShmStatsProducer.OnConnect();
    m_oShmStatsProducer.AddRequest();
    m_oShmStatsProducer.AddRxBytes(static_cast<uint64_t>(a_rReq.body.size()));

    const T_JKPQC_STATS l_tStats = m_oShmStatsProducer.GetSnapshot();
    nlohmann::json l_oJson;
    std::string l_strJson;

    l_oJson["version"] = l_tStats.m_uiVersion;
    l_oJson["current_connections"] = l_tStats.m_uiCurrentConnections;
    l_oJson["max_connections"] = l_tStats.m_uiMaxConnections;
    l_oJson["total_rx_bytes"] = l_tStats.m_ullTotalBytes4RX;
    l_oJson["total_tx_bytes"] = l_tStats.m_ullTotalBytes4TX;
    l_oJson["total_requests"] = l_tStats.m_ullTotalRequests;
    l_oJson["uptime_seconds"] = l_tStats.m_ullUptimeSeconds;
    l_oJson["last_updated"] = l_tStats.m_ullLastUpdated;
    l_strJson = l_oJson.dump();
    a_rRes.set_content(l_strJson, "application/json");

    m_oShmStatsProducer.AddTxBytes(static_cast<uint64_t>(a_rRes.body.size()));
    m_oShmStatsProducer.OnDisconnect();
  });
  // -------------------------------------
  // API : 에코 (테스트용)
  // -------------------------------------
  l_oServer.Post("/api/echo", [this](const httplib::Request& a_rReq, httplib::Response& a_rRes)
  {
    m_oShmStatsProducer.OnConnect();
    m_oShmStatsProducer.AddRequest();
    m_oShmStatsProducer.AddRxBytes(static_cast<uint64_t>(a_rReq.body.size()));
    a_rRes.set_content(a_rReq.body, "application/json");
    m_oShmStatsProducer.AddTxBytes(static_cast<uint64_t>(a_rRes.body.size()));
    m_oShmStatsProducer.OnDisconnect();
  });
  // -----------------------------------------------------------------------------
  // -------------------------------------
  // 서버 시작
  // -------------------------------------
  printf("서버 대기 중 : 0.0.0.0:%d\n", a_iPort);
  const bool l_bListenOk = l_oServer.listen("0.0.0.0", a_iPort);
  // -------------------------------------
  // 종료 처리
  // -------------------------------------
  // 통계 업데이트 쓰레드 종료
  m_bRunStatsLoop.store(false);
  if (m_oStatsThread.joinable())
  {
    m_oStatsThread.join();
  }
  m_oShmStatsProducer.Shutdown();
  // 서버 종료 오류 처리
  if (!l_bListenOk)
  {
    fprintf(stderr, "[ERR_] HTTPS 서버 listen 실패\n");
    ERR_print_errors_fp(stderr);
    return false;
  }
  // -------------------------------------
  return true;
}

void CHttpsServer::_updateStatsLoop()
{
  while (m_bRunStatsLoop.load())
  {
    m_oShmStatsProducer.Update();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

// -----------------------------------------------------------------------------
// CHttpsClient 클래스 구현
// -----------------------------------------------------------------------------
CHttpsClient::CHttpsClient()
{
  m_bSslInfoPrinted = false;
}

CHttpsClient::~CHttpsClient()
{
}

const SSL_METHOD* CHttpsClient::createMethod()
{
  return TLS_client_method();
}

bool CHttpsClient::Get(const std::string& a_rstrHost, int a_iPort, std::string& a_rstrResponseBody)
{
  return Get(a_rstrHost, a_iPort, "/", a_rstrResponseBody);
}

bool CHttpsClient::Get(const std::string& a_rstrHost, int a_iPort, const std::string& a_rstrPath, std::string& a_rstrResponseBody)
{
  if (m_strCaPath.empty() || m_strCertPath.empty() || m_strKeyPath.empty())
  {
    fprintf(stderr, "[ERR_] 인증서 경로 미설정\n");
    return false;
  }
  httplib::SSLClient l_oClient(a_rstrHost, a_iPort, m_strCertPath, m_strKeyPath);
  if (!l_oClient.is_valid())
  {
    fprintf(stderr, "[ERR_] httplib::SSLClient 초기화 실패\n");
    ERR_print_errors_fp(stderr);
    return false;
  }
  std::ifstream l_oCaFile(m_strCaPath, std::ios::binary);
  if (!l_oCaFile.is_open())
  {
    fprintf(stderr, "[ERR_] CA 인증서 파일 열기 실패: %s\n", m_strCaPath.c_str());
    return false;
  }
  std::stringstream l_oBuffer;
  l_oBuffer << l_oCaFile.rdbuf();
  const std::string l_strCaPem = l_oBuffer.str();
  l_oClient.load_ca_cert_store(l_strCaPem.c_str(), l_strCaPem.size());
  l_oClient.enable_server_certificate_verification(true);
  if ((!m_bSslInfoPrinted) && (a_rstrPath == "/"))
  {
    l_oClient.set_session_verifier([this](httplib::tls::session_t a_tSession)
    {
      SSL* a_pSsl = static_cast<SSL*>(a_tSession);
      PrintInfo4SSL(a_pSsl);
      m_bSslInfoPrinted = true;
      return httplib::NoDecisionMade;
    });
  }
  l_oClient.set_connection_timeout(5, 0);
  l_oClient.set_read_timeout(10, 0);
  SSL_CTX* l_pSslCtx = static_cast<SSL_CTX*>(l_oClient.tls_context());
  if (l_pSslCtx == NULL)
  {
    fprintf(stderr, "[ERR_] SSL_CTX 획득 실패\n");
    return false;
  }
  if (SSL_CTX_set1_groups_list(l_pSslCtx, m_strKEM.c_str()) != 1)
  {
    fprintf(stderr, "[ERR_] SSL_CTX_set1_groups_list 실패: %s\n", m_strKEM.c_str());
    ERR_print_errors_fp(stderr);
    return false;
  }
  auto l_oResult = l_oClient.Get(a_rstrPath.c_str());
  if (!l_oResult)
  {
    auto err = l_oResult.error();
    fprintf(stderr, "[ERR_] HTTPS 요청 실패: %s\n", httplib::to_string(err).c_str());
    ERR_print_errors_fp(stderr);
    return false;
  }
  const auto& l_rResponse = l_oResult.value();
  a_rstrResponseBody = l_rResponse.body;

  return true;
}

bool CHttpsClient::Post(const std::string& a_rstrHost, int a_iPort, const std::string& a_rstrPath, const std::string& a_rstrBody, const std::string& a_rstrContentType, std::string& a_rstrResponseBody)
{
  if (m_strCaPath.empty() || m_strCertPath.empty() || m_strKeyPath.empty())
  {
    fprintf(stderr, "[ERR_] 인증서 경로 미설정\n");
    return false;
  }
  httplib::SSLClient l_oClient(a_rstrHost, a_iPort, m_strCertPath, m_strKeyPath);
  if (!l_oClient.is_valid())
  {
    fprintf(stderr, "[ERR_] httplib::SSLClient 초기화 실패\n");
    ERR_print_errors_fp(stderr);
    return false;
  }
  std::ifstream l_oCaFile(m_strCaPath, std::ios::binary);
  if (!l_oCaFile.is_open())
  {
    fprintf(stderr, "[ERR_] CA 인증서 파일 열기 실패: %s\n", m_strCaPath.c_str());
    return false;
  }
  std::stringstream l_oBuffer;
  l_oBuffer << l_oCaFile.rdbuf();
  const std::string l_strCaPem = l_oBuffer.str();
  l_oClient.load_ca_cert_store(l_strCaPem.c_str(), l_strCaPem.size());
  l_oClient.enable_server_certificate_verification(true);
  l_oClient.set_connection_timeout(5, 0);
  l_oClient.set_read_timeout(10, 0);
  SSL_CTX* l_pSslCtx = static_cast<SSL_CTX*>(l_oClient.tls_context());
  if (l_pSslCtx == NULL)
  {
    fprintf(stderr, "[ERR_] SSL_CTX 획득 실패\n");
    return false;
  }
  if (SSL_CTX_set1_groups_list(l_pSslCtx, m_strKEM.c_str()) != 1)
  {
    fprintf(stderr, "[ERR_] SSL_CTX_set1_groups_list 실패: %s\n", m_strKEM.c_str());
    ERR_print_errors_fp(stderr);
    return false;
  }
  auto l_oResult = l_oClient.Post(a_rstrPath.c_str(), a_rstrBody, a_rstrContentType.c_str());
  if (!l_oResult)
  {
    auto err = l_oResult.error();
    fprintf(stderr, "[ERR_] HTTPS 요청 실패: %s\n", httplib::to_string(err).c_str());
    ERR_print_errors_fp(stderr);
    return false;
  }
  const auto& l_rResponse = l_oResult.value();
  a_rstrResponseBody = l_rResponse.body;
  return true;
}
// -----------------------------------------------------------------------------
