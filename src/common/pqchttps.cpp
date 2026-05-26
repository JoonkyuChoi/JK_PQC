/*----------------------------------------------------------------------------+-
pqchttps.cpp
-+----------------------------------------------------------------------------+-
Description : CSocketPool + llhttp + OpenSSL Hybrid mTLS HTTP 서버 모듈 구현
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/05/24] 최초 작성
  [2026/05/24] shm_stats·대시보드 API(CPqcHttpService), CHttpRouter 통계 연동
  [2026/05/24] 이중 인증서: ClientHello 기반 ML-DSA·ECDSA 선택 (jk-https-server 동일)
-+----------------------------------------------------------------------------*/
#include "pqchttps.h"
#include "pqc_utils.h"

#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <llhttp.h>

// OpenSSL 버전에 따라 ClientHello 콜백 반환 매크로가 없을 수 있음
#ifndef SSL_CLIENT_HELLO_SUCCESS
#define SSL_CLIENT_HELLO_SUCCESS 1
#endif
#ifndef SSL_CLIENT_HELLO_ERROR
#define SSL_CLIENT_HELLO_ERROR 0
#endif

// SSL ex_data 슬롯 (SSL_get_ex_new_index로 할당, index 0 고정 사용 금지)
static int s_iSslExSocketFdIdx = -1;

// SSL_CTX_set_session_id_context용 고정 컨텍스트 (브라우저 세션 재개 오류 방지)
static const unsigned char D_SSL_SID_CTX[] = "jk-pqc-server";

static int sfEnsureSslExSocketFdIdx()
{
  if (s_iSslExSocketFdIdx < 0)
  {
    s_iSslExSocketFdIdx = SSL_get_ex_new_index(
      0, const_cast<char*>("jk-pqc-socket-fd"), nullptr, nullptr, nullptr);
  }
  return s_iSslExSocketFdIdx;
}

// memory BIO 사용 시 SSL_get_fd() 대신 ex_data에서 fd 조회
static socket_t sfGetSslSocketFd(const SSL* a_pSsl)
{
  if (a_pSsl == nullptr)
  {
    return static_cast<socket_t>(-1);
  }
  const int l_iIdx = sfEnsureSslExSocketFdIdx();
  if (l_iIdx < 0)
  {
    return static_cast<socket_t>(-1);
  }
  return static_cast<socket_t>(
    reinterpret_cast<intptr_t>(SSL_get_ex_data(a_pSsl, l_iIdx)));
}

// Accept 스레드 핸드셰이크용 blocking recv (IOCP 등록 전)
static int sfBlockingRecv(socket_t a_tSock, uint8_t* a_pucBuf, size_t a_ullLen)
{
#ifdef _WIN32
  return ::recv(a_tSock,
                reinterpret_cast<char*>(a_pucBuf),
                static_cast<int>(a_ullLen),
                0);
#else
  return static_cast<int>(::recv(a_tSock, a_pucBuf, a_ullLen, 0));
#endif
}

// Accept 스레드 핸드셰이크용 blocking send (IOCP 비동기 send 우회)
// _FlushWbio가 CSocketPool::Send를 호출하면 IOCP 큐잉만 보장하고 즉시 반환되어
// 다음 sfBlockingRecv가 서버 flight 전송 완료 전에 실행될 수 있다.
// 그 결과 브라우저가 서버 flight를 받지 못해 alert 46(certificate_unknown)을 던진다.
static bool sfBlockingSend(socket_t       a_tSock,
                           const uint8_t* a_pucBuf,
                           size_t         a_ullLen)
{
  size_t l_ullOff = 0;
  while (l_ullOff < a_ullLen)
  {
#ifdef _WIN32
    const int l_iN = ::send(a_tSock,
                            reinterpret_cast<const char*>(a_pucBuf) + l_ullOff,
                            static_cast<int>(a_ullLen - l_ullOff),
                            0);
#else
    const int l_iN = static_cast<int>(
      ::send(a_tSock,
             reinterpret_cast<const char*>(a_pucBuf) + l_ullOff,
             a_ullLen - l_ullOff,
             MSG_NOSIGNAL));
#endif
    if (l_iN <= 0)
    {
      printf("[ERR_:SEND:TCP_:%05d] sfBlockingSend 실패\n",
              static_cast<int>(a_tSock));
      return false;
    }
    l_ullOff += static_cast<size_t>(l_iN);
  }
  return true;
}

// TLS 핸드셰이크 상태 추적 (info callback) ? fd는 ex_data 기준
static void sfSslInfoCb(const SSL* a_pSsl, int a_iWhere, int a_iRet)
{
  (void)a_iRet;
  const int l_iFd = static_cast<int>(sfGetSslSocketFd(a_pSsl));
  const char* l_cpszState = SSL_state_string_long(a_pSsl);
  if ((a_iWhere & SSL_CB_HANDSHAKE_START) != 0)
  {
    printf("[DBG_:RECV:TCP_:%05d] TLS Handshake Started\n", l_iFd);
  }
  else if ((a_iWhere & SSL_CB_HANDSHAKE_DONE) != 0)
  {
    printf("[DBG_:RECV:TCP_:%05d] TLS Handshake Done\n", l_iFd);
  }
  else if ((a_iWhere & SSL_CB_LOOP) != 0)
  {
    printf("[DBG_:RECV:TCP_:%05d] TLS State: %s\n", l_iFd, l_cpszState);
  }
}

// =============================================================================
// CMtlsAuth
// =============================================================================
CMtlsAuth::CMtlsAuth()
  : m_pStore(nullptr)
{
}

CMtlsAuth::~CMtlsAuth()
{
  if (m_pStore != nullptr)
  {
    X509_STORE_free(m_pStore);
    m_pStore = nullptr;
  }
}

bool CMtlsAuth::Init(const char* a_pszCaFile)
{
  if ((a_pszCaFile == nullptr) || (a_pszCaFile[0] == '\0'))
  {
    printf("[ERR_] CMtlsAuth::Init CA 경로 미지정\n");
    return false;
  }
  m_pStore = X509_STORE_new();
  if (m_pStore == nullptr)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  if (X509_STORE_load_locations(m_pStore, a_pszCaFile, nullptr) != 1)
  {
    printf("[ERR_] CMtlsAuth::Init CA 로드 실패: %s\n", a_pszCaFile);
    ERR_print_errors_fp(stderr);
    return false;
  }
  return true;
}

bool CMtlsAuth::Verify(X509* a_pX509Cert) const
{
  if ((m_pStore == nullptr) || (a_pX509Cert == nullptr))
  {
    return false;
  }
  X509_STORE_CTX* l_pCtx = X509_STORE_CTX_new();
  if (l_pCtx == nullptr)
  {
    return false;
  }
  bool l_bOk = false;
  if (X509_STORE_CTX_init(l_pCtx, m_pStore, a_pX509Cert, nullptr) == 1)
  {
    if (X509_verify_cert(l_pCtx) == 1)
    {
      l_bOk = true;
    }
    else
    {
      printf("[ERR_] CMtlsAuth::Verify 인증서 검증 실패\n");
      ERR_print_errors_fp(stderr);
    }
  }
  X509_STORE_CTX_free(l_pCtx);

  if (l_bOk)
  {
    const ASN1_TIME* l_pNotBefore = X509_get0_notBefore(a_pX509Cert);
    const ASN1_TIME* l_pNotAfter = X509_get0_notAfter(a_pX509Cert);
    const int l_iBefore = X509_cmp_current_time(l_pNotBefore);
    const int l_iAfter = X509_cmp_current_time(l_pNotAfter);
    if ((l_iBefore > 0) || (l_iAfter < 0))
    {
      printf("[ERR_] CMtlsAuth::Verify 인증서 유효기간 만료\n");
      l_bOk = false;
    }
  }
  return l_bOk;
}

bool CMtlsAuth::GetSubjectCN(X509* a_pX509, std::string& a_rstrCn) const
{
  a_rstrCn.clear();
  if (a_pX509 == nullptr)
  {
    return false;
  }
  X509_NAME* l_pName = X509_get_subject_name(a_pX509);
  if (l_pName == nullptr)
  {
    return false;
  }
  const int l_iIdx = X509_NAME_get_index_by_NID(l_pName, NID_commonName, -1);
  if (l_iIdx < 0)
  {
    return false;
  }
  X509_NAME_ENTRY* l_pEntry = X509_NAME_get_entry(l_pName, l_iIdx);
  if (l_pEntry == nullptr)
  {
    return false;
  }
  ASN1_STRING* l_pData = X509_NAME_ENTRY_get_data(l_pEntry);
  if (l_pData == nullptr)
  {
    return false;
  }
  const unsigned char* l_pucUtf8 = ASN1_STRING_get0_data(l_pData);
  const int l_iLen = ASN1_STRING_length(l_pData);
  if ((l_pucUtf8 == nullptr) || (l_iLen <= 0))
  {
    return false;
  }
  a_rstrCn.assign(reinterpret_cast<const char*>(l_pucUtf8),
                  static_cast<size_t>(l_iLen));
  return true;
}

bool CMtlsAuth::GetSubjectSAN(X509* a_pX509, std::vector<std::string>& a_roSans) const
{
  a_roSans.clear();
  if (a_pX509 == nullptr)
  {
    return false;
  }
  STACK_OF(GENERAL_NAME)* l_pSanStack = static_cast<STACK_OF(GENERAL_NAME)*>(
    X509_get_ext_d2i(a_pX509, NID_subject_alt_name, nullptr, nullptr));
  if (l_pSanStack == nullptr)
  {
    return false;
  }
  const int l_iCount = sk_GENERAL_NAME_num(l_pSanStack);
  for (int l_i = 0; l_i < l_iCount; ++l_i)
  {
    const GENERAL_NAME* l_pGen = sk_GENERAL_NAME_value(l_pSanStack, l_i);
    if (l_pGen == nullptr)
    {
      continue;
    }
    if ((l_pGen->type == GEN_DNS) || (l_pGen->type == GEN_URI))
    {
      const unsigned char* l_pucStr = ASN1_STRING_get0_data(l_pGen->d.dNSName);
      const int l_iLen = ASN1_STRING_length(l_pGen->d.dNSName);
      if ((l_pucStr != nullptr) && (l_iLen > 0))
      {
        a_roSans.emplace_back(reinterpret_cast<const char*>(l_pucStr),
                              static_cast<size_t>(l_iLen));
      }
    }
  }
  sk_GENERAL_NAME_pop_free(l_pSanStack, GENERAL_NAME_free);
  return !a_roSans.empty();
}

// =============================================================================
// CHybridTls
// =============================================================================
CHybridTls::CHybridTls()
  : m_pSslCtx(nullptr)
  , m_bDualCert(true)
  , m_bmTLS(true)
  , m_pPool(nullptr)
  , m_pAuth(nullptr)
{
}

void CHybridTls::SetEcdsaCertPaths(const char* a_pszCaFile,
                                   const char* a_pszCertFile,
                                   const char* a_pszKeyFile)
{
  if (a_pszCaFile != nullptr)
  {
    m_strEcdsaCaPath = a_pszCaFile;
  }
  if (a_pszCertFile != nullptr)
  {
    m_strEcdsaCertPath = a_pszCertFile;
  }
  if (a_pszKeyFile != nullptr)
  {
    m_strEcdsaKeyPath = a_pszKeyFile;
  }
}

int CHybridTls::smfClientHelloCb(SSL* a_pSsl, int* a_piAlert, void* a_pvArg)
{
  CHybridTls* l_pThis = static_cast<CHybridTls*>(a_pvArg);
  if (l_pThis == nullptr)
  {
    if (a_piAlert != nullptr)
    {
      *a_piAlert = SSL_AD_INTERNAL_ERROR;
    }
    return SSL_CLIENT_HELLO_ERROR;
  }
  const unsigned int l_uiConnId = static_cast<unsigned int>(
    reinterpret_cast<uintptr_t>(a_pSsl) & 0xFFFFu);
  char     l_szPeerAddr[64] = "unknown";
  uint16_t l_usPeerPort     = 0;
  const socket_t l_tSock = sfGetSslSocketFd(a_pSsl);
  if (l_pThis->m_pPool != nullptr)
  {
    std::string l_oIp;
    uint16_t    l_usPort = 0;
    if (l_pThis->m_pPool->GetPeerInfo(l_tSock, l_oIp, l_usPort))
    {
      strncpy(l_szPeerAddr, l_oIp.c_str(), sizeof(l_szPeerAddr) - 1u);
      l_szPeerAddr[sizeof(l_szPeerAddr) - 1u] = '\0';
      l_usPeerPort = l_usPort;
    }
  }
  // ClientHello extension 13: signature_algorithms
  const unsigned char* l_pucExt    = nullptr;
  size_t               l_ullExtLen = 0;
  const int l_iHasSigAlgsExt = SSL_client_hello_get0_ext(a_pSsl, 13u,
                                                         &l_pucExt, &l_ullExtLen);
  bool l_bSupportMlDsa = false;
  if ((l_iHasSigAlgsExt == 1) && (l_pucExt != nullptr) && (l_ullExtLen >= 2u))
  {
    const size_t l_ullListLen = (static_cast<size_t>(l_pucExt[0]) << 8) |
                                static_cast<size_t>(l_pucExt[1]);
    if ((l_ullExtLen >= (2u + l_ullListLen)) && ((l_ullListLen % 2u) == 0u))
    {
      for (size_t l_ullI = 0u; l_ullI + 1u < l_ullListLen; l_ullI += 2u)
      {
        const uint16_t l_uiScheme = static_cast<uint16_t>(
          (static_cast<unsigned int>(l_pucExt[2u + l_ullI]) << 8) |
          static_cast<unsigned int>(l_pucExt[2u + l_ullI + 1u]));
        // ML-DSA-44: 0x0904 / ML-DSA-65: 0x0905 / ML-DSA-87: 0x0906
        if ((l_uiScheme == 0x0904u) || (l_uiScheme == 0x0905u) ||
            (l_uiScheme == 0x0906u))
        {
          l_bSupportMlDsa = true;
          break;
        }
      }
    }
  }
  const char* l_cpszCertPath = nullptr;
  const char* l_cpszKeyPath  = nullptr;
  if (l_bSupportMlDsa)
  {
    l_cpszCertPath = l_pThis->m_strCertPath.c_str();
    l_cpszKeyPath  = l_pThis->m_strKeyPath.c_str();
    printf("[INFO:%s:%05u:%04X] Dual Cert : ML-DSA-65\n",
           l_szPeerAddr, l_usPeerPort, l_uiConnId);
  }
  else
  {
    l_cpszCertPath = l_pThis->m_strEcdsaCertPath.c_str();
    l_cpszKeyPath  = l_pThis->m_strEcdsaKeyPath.c_str();
    printf("[INFO:%s:%05u:%04X] Dual Cert : ECDSA P-256 (PQC 미지원 클라이언트)\n",
           l_szPeerAddr, l_usPeerPort, l_uiConnId);
  }
  // ClientHello 콜백과 TLS 1.3 session resumption은 OpenSSL에서 호환되지 않음.
  // 브라우저가 이전 서버(jk-https-server 등)의 ticket/PSK를 보내면 alert 46 유발 가능.
  (void)SSL_set_session(a_pSsl, nullptr);
  SSL_set_options(a_pSsl, SSL_get_options(a_pSsl) | SSL_OP_NO_TICKET);
  (void)SSL_set_num_tickets(a_pSsl, 0);
  const unsigned char* l_pucPskExt    = nullptr;
  size_t               l_ullPskExtLen = 0;
  if ((SSL_client_hello_get0_ext(a_pSsl, 41u, &l_pucPskExt, &l_ullPskExtLen) == 1) &&
      (l_ullPskExtLen > 0u))
  {
    printf("[INFO:%s:%05u:%04X] ClientHello PSK/ticket 무시 → full handshake\n",
           l_szPeerAddr, l_usPeerPort, l_uiConnId);
  }
  // CTX placeholder(ML-DSA) 체인·leaf 제거 후 선택 인증서 로드
  SSL_clear_chain_certs(a_pSsl);
  if ((SSL_use_certificate_chain_file(a_pSsl, l_cpszCertPath) != 1) ||
      (SSL_use_PrivateKey_file(a_pSsl, l_cpszKeyPath, SSL_FILETYPE_PEM) != 1))
  {
    printf("[ERR_:%s:%05u:%04X] 이중 인증서 로드 실패: %s\n",
            l_szPeerAddr, l_usPeerPort, l_uiConnId, l_cpszCertPath);
    if (a_piAlert != nullptr)
    {
      *a_piAlert = SSL_AD_INTERNAL_ERROR;
    }
    return SSL_CLIENT_HELLO_ERROR;
  }
  if (SSL_check_private_key(a_pSsl) != 1)
  {
    printf("[ERR_:%s:%05u:%04X] 인증서·개인키 불일치: %s\n",
            l_szPeerAddr, l_usPeerPort, l_uiConnId, l_cpszCertPath);
    ERR_print_errors_fp(stderr);
    if (a_piAlert != nullptr)
    {
      *a_piAlert = SSL_AD_INTERNAL_ERROR;
    }
    return SSL_CLIENT_HELLO_ERROR;
  }
  return SSL_CLIENT_HELLO_SUCCESS;
}

bool CHybridTls::_loadServerCertificates()
{
  if (m_pSslCtx == nullptr)
  {
    return false;
  }
  if (m_strCaPath.empty() || m_strCertPath.empty() || m_strKeyPath.empty())
  {
    printf("[ERR_] CHybridTls 인증서 경로 미설정\n");
    return false;
  }
  if (m_bDualCert &&
      (m_strEcdsaCertPath.empty() || m_strEcdsaKeyPath.empty()))
  {
    printf("[ERR_] ECDSA 인증서 경로 미설정 (SetEcdsaCertPaths 필요)\n");
    return false;
  }
  // jk-https-server setup_callback 와 동일 순서:
  // 1) CTX placeholder (이중 인증서: ML-DSA, 단일: ML-DSA)
  // 2) ClientHello 콜백
  // 3) mTLS 검증·CA 로드
  if (SSL_CTX_use_certificate_file(m_pSslCtx, m_strCertPath.c_str(),
                                   SSL_FILETYPE_PEM) != 1)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  if (SSL_CTX_use_PrivateKey_file(m_pSslCtx, m_strKeyPath.c_str(),
                                  SSL_FILETYPE_PEM) != 1)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  if (m_bDualCert)
  {
    SSL_CTX_set_client_hello_cb(m_pSslCtx, smfClientHelloCb, this);
  }

  // 브라우저 TLS-Handshake 절차 분석용 info callback은 _configureSslCtxSession에서
  // 이미 sfSslInfoCb로 등록되어 있다. 여기서 다시 등록하면 SSL_get_fd()를 사용하는
  // 람다가 sfSslInfoCb를 덮어써 memory BIO 모드에서 fd=-1만 출력된다.
  if (m_bmTLS)
  {
    if (SSL_CTX_load_verify_locations(m_pSslCtx, m_strCaPath.c_str(), nullptr) != 1)
    {
      ERR_print_errors_fp(stderr);
      return false;
    }
    if (m_bDualCert && !m_strEcdsaCaPath.empty())
    {
      if (SSL_CTX_load_verify_locations(m_pSslCtx, m_strEcdsaCaPath.c_str(),
                                        nullptr) != 1)
      {
        ERR_print_errors_fp(stderr);
        return false;
      }
    }
    SSL_CTX_set_verify(m_pSslCtx,
                       SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                       nullptr);
  }
  else
  {
    SSL_CTX_set_verify(m_pSslCtx, SSL_VERIFY_NONE, nullptr);
  }
  if (SSL_CTX_check_private_key(m_pSslCtx) != 1)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  return true;
}

void CHybridTls::_configureSslCtxSession()
{
  if (m_pSslCtx == nullptr)
  {
    return;
  }
  // ClientHello 콜백·이중 인증서와 TLS 세션 재개(resumption)는 호환되지 않음
  (void)SSL_CTX_set_session_id_context(m_pSslCtx,
                                       D_SSL_SID_CTX,
                                       static_cast<unsigned int>(sizeof(D_SSL_SID_CTX) - 1u));
  SSL_CTX_set_session_cache_mode(m_pSslCtx, SSL_SESS_CACHE_OFF);
  SSL_CTX_set_num_tickets(m_pSslCtx, 0);
  SSL_CTX_set_options(m_pSslCtx, SSL_OP_NO_TICKET);
  SSL_CTX_set_max_early_data(m_pSslCtx, 0);
  // jk-https-server 등 이전 프로세스 ticket과의 resumption 차단 (프로세스마다 무작위 키)
  {
    unsigned char l_aucTicketKey[80] = {};
    if (RAND_bytes(l_aucTicketKey, static_cast<int>(sizeof(l_aucTicketKey))) == 1)
    {
      (void)SSL_CTX_set_tlsext_ticket_keys(m_pSslCtx,
                                           l_aucTicketKey,
                                           static_cast<int>(sizeof(l_aucTicketKey)));
    }
  }
  // memory BIO + SSL_accept: OpenSSL 내부 버퍼와 wbio flush 타이밍 정합
  SSL_CTX_set_mode(m_pSslCtx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
  SSL_CTX_set_info_callback(m_pSslCtx, sfSslInfoCb);
}

CHybridTls::~CHybridTls()
{
  {
    std::lock_guard<std::mutex> l_oLock(m_oMapMtx);
    for (auto& l_oPair : m_oSessionMap)
    {
      if (l_oPair.second != nullptr)
      {
        std::lock_guard<std::recursive_mutex> l_oSessLock(l_oPair.second->m_oMtx);
        if (l_oPair.second->m_pSsl != nullptr)
        {
          SSL_free(l_oPair.second->m_pSsl);
          l_oPair.second->m_pSsl = nullptr;
        }
        l_oPair.second->m_pRbio = nullptr;
        l_oPair.second->m_pWbio = nullptr;
      }
    }
    m_oSessionMap.clear();
  }
  if (m_pSslCtx != nullptr)
  {
    SSL_CTX_free(m_pSslCtx);
    m_pSslCtx = nullptr;
  }
}

bool CHybridTls::Init(const char* a_pszCertFile,
                      const char* a_pszKeyFile,
                      const char* a_pszCaFile,
                      const char* a_pszKemGroups,
                      bool a_bDualCert,
                      bool a_bmTLS)
{
  if ((a_pszCertFile == nullptr) || (a_pszKeyFile == nullptr) || (a_pszCaFile == nullptr))
  {
    printf("[ERR_] CHybridTls::Init 인증서 경로 미지정\n");
    return false;
  }
  m_strCertPath = a_pszCertFile;
  m_strKeyPath  = a_pszKeyFile;
  m_strCaPath   = a_pszCaFile;
  m_bDualCert   = a_bDualCert;
  m_bmTLS       = a_bmTLS;

  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();

  if (!gfLoadProvider4OQS(nullptr))
  {
    return false;
  }
  if (m_pSslCtx != nullptr)
  {
    SSL_CTX_free(m_pSslCtx);
    m_pSslCtx = nullptr;
  }
  m_pSslCtx = SSL_CTX_new(TLS_server_method());
  if (m_pSslCtx == nullptr)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  if (sfEnsureSslExSocketFdIdx() < 0)
  {
    printf("[ERR_] SSL_get_ex_new_index 실패\n");
    return false;
  }
  _configureSslCtxSession();
  if (!_loadServerCertificates())
  {
    return false;
  }
  if ((a_pszKemGroups != nullptr) && (a_pszKemGroups[0] != '\0'))
  {
    m_strKem = a_pszKemGroups;
  }
  else
  {
    m_strKem = D_PQCHTTPS_DEFAULT_KEM;
  }
  if (SSL_CTX_set1_groups_list(m_pSslCtx, m_strKem.c_str()) != 1)
  {
    printf("[ERR_] SSL_CTX_set1_groups_list 실패: %s\n", m_strKem.c_str());
    ERR_print_errors_fp(stderr);
    return false;
  }
  return true;
}

const std::string& CHybridTls::GetKemGroups() const
{
  return m_strKem;
}

void CHybridTls::SetSocketPool(CSocketPool* a_pPool)
{
  m_pPool = a_pPool;
}

void CHybridTls::SetOnPlainRecv(CBFP_TLS_PLAIN_RECV a_cbf)
{
  m_cbfPlainRecv = std::move(a_cbf);
}

void CHybridTls::SetMtlsAuth(CMtlsAuth* a_pAuth)
{
  m_pAuth = a_pAuth;
}

std::shared_ptr<CHybridTls::T_TLS_SESSION> CHybridTls::_GetSession(socket_t a_tSock) const
{
  std::lock_guard<std::mutex> l_oLock(m_oMapMtx);
  const auto l_oIt = m_oSessionMap.find(a_tSock);
  if (l_oIt == m_oSessionMap.end())
  {
    return nullptr;
  }
  return l_oIt->second;
}

bool CHybridTls::_CreateSession(socket_t a_tSock, bool a_bServer)
{
  if (m_pSslCtx == nullptr)
  {
    return false;
  }
  auto l_pSess = std::make_shared<T_TLS_SESSION>();
  l_pSess->m_bIsServer = a_bServer;
  l_pSess->m_pRbio = BIO_new(BIO_s_mem());
  l_pSess->m_pWbio = BIO_new(BIO_s_mem());
  if ((l_pSess->m_pRbio == nullptr) || (l_pSess->m_pWbio == nullptr))
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  // SSL이 rbio에 더 많은 암호문이 필요할 때 WANT_READ를 올바르게 반환하도록 설정
  BIO_set_mem_eof_return(l_pSess->m_pRbio, -1);
  l_pSess->m_pSsl = SSL_new(m_pSslCtx);
  if (l_pSess->m_pSsl == nullptr)
  {
    ERR_print_errors_fp(stderr);
    BIO_free(l_pSess->m_pRbio);
    BIO_free(l_pSess->m_pWbio);
    return false;
  }
  // memory BIO 모드: SSL_set_fd() 사용 불가. fd는 ex_data에 저장.
  const int l_iExIdx = sfEnsureSslExSocketFdIdx();
  SSL_set_ex_data(l_pSess->m_pSsl, l_iExIdx,
                  reinterpret_cast<void*>(static_cast<intptr_t>(a_tSock)));
  SSL_set_bio(l_pSess->m_pSsl, l_pSess->m_pRbio, l_pSess->m_pWbio);
  (void)SSL_set_session_id_context(l_pSess->m_pSsl,
                                   D_SSL_SID_CTX,
                                   static_cast<unsigned int>(sizeof(D_SSL_SID_CTX) - 1u));
  SSL_set_session(l_pSess->m_pSsl, nullptr);
  SSL_set_options(l_pSess->m_pSsl, SSL_OP_NO_TICKET);
  (void)SSL_set_num_tickets(l_pSess->m_pSsl, 0);
  if (a_bServer)
  {
    SSL_set_accept_state(l_pSess->m_pSsl);
  }
  else
  {
    SSL_set_connect_state(l_pSess->m_pSsl);
  }
  {
    std::lock_guard<std::mutex> l_oLock(m_oMapMtx);
    m_oSessionMap[a_tSock] = l_pSess;
  }
  return true;
}

bool CHybridTls::AcceptHandshake(socket_t a_tSock)
{
  return _CreateSession(a_tSock, true);
}

bool CHybridTls::CompleteHandshakeBlocking(socket_t a_tSock)
{
  auto l_pSess = _GetSession(a_tSock);
  if ((l_pSess == nullptr) || !l_pSess->m_bIsServer)
  {
    return false;
  }
  if (l_pSess->m_bHandshakeDone)
  {
    return true;
  }
  std::lock_guard<std::recursive_mutex> l_oLock(l_pSess->m_oMtx);
  uint8_t l_aucBuf[D_PQCHTTPS_RECV_BUF];

  while (!l_pSess->m_bHandshakeDone)
  {
    // SSL_accept 루프 (rbio 소진까지 반복)
    // 핵심: wbio flush를 IOCP가 아닌 blocking ::send 로 직접 보내야
    //       브라우저가 서버 flight를 즉시 받고 다음 flight를 보낸다.
    for (;;)
    {
      int l_iRet = 0;
      {
        std::lock_guard<std::mutex> l_oAccLk(m_oSslAcceptMtx);
        l_iRet = SSL_accept(l_pSess->m_pSsl);
      }
      if (l_iRet == 1)
      {
        l_pSess->m_bHandshakeDone = true;
        if (!_FlushWbioDirect(*l_pSess, a_tSock))
        {
          return false;
        }
        break;
      }
      const int l_iErr = SSL_get_error(l_pSess->m_pSsl, l_iRet);
      if (l_iErr == SSL_ERROR_WANT_READ)
      {
        // 서버 flight를 먼저 blocking send로 송신
        if (!_FlushWbioDirect(*l_pSess, a_tSock))
        {
          return false;
        }
        // rbio에 데이터가 남아 있으면 recv 없이 재시도
        if ((l_pSess->m_pRbio != nullptr) &&
            (BIO_pending(l_pSess->m_pRbio) > 0))
        {
          continue;
        }
        // 다음 클라이언트 flight 수신 필요 → 내부 루프 탈출
        break;
      }
      if (l_iErr == SSL_ERROR_WANT_WRITE)
      {
        if (!_FlushWbioDirect(*l_pSess, a_tSock))
        {
          return false;
        }
        continue;
      }
      // peer alert · 핸드셰이크 오류
      (void)_FlushWbioDirect(*l_pSess, a_tSock);
      // peer alert (certificate_unknown=46, unknown_ca=48 등)는 브라우저 mTLS UI
      // 동작 중 발생하는 정상 종료(probe connection 해제)일 수 있다.
      // 실제 페이지 로드는 사용자가 클라이언트 인증서를 선택한 후속 연결에서 완료된다.
      // OpenSSL 버전 간 reason 매크로 명칭 차이를 피하기 위해 reason 문자열로 분기.
      const unsigned long l_ulErr      = ERR_peek_error();
      const char*         l_cpszReason = ERR_reason_error_string(l_ulErr);
      const bool          l_bPeerAlert =
        (l_cpszReason != nullptr) && (strstr(l_cpszReason, "alert") != nullptr);
      if (l_bPeerAlert)
      {
        printf("[INFO:RECV:TCP_:%05d] 클라이언트가 핸드셰이크 중단 (TLS %s, mTLS UI 정리)\n",
               static_cast<int>(a_tSock),
               l_cpszReason);
        ERR_clear_error();
      }
      else
      {
        printf("[ERR_] CompleteHandshakeBlocking SSL_accept 실패 err=%d\n",
                l_iErr);
        ERR_print_errors_fp(stderr);
      }
      return false;
    }
    if (l_pSess->m_bHandshakeDone)
    {
      break;
    }
    // 다음 클라이언트 flight blocking recv
    const int l_iN = sfBlockingRecv(a_tSock, l_aucBuf, sizeof(l_aucBuf));
    if (l_iN <= 0)
    {
      printf("[ERR_:RECV:TCP_:%05d] CompleteHandshakeBlocking recv 실패 n=%d\n",
              static_cast<int>(a_tSock),
              l_iN);
      return false;
    }
    if (l_pSess->m_pRbio == nullptr)
    {
      return false;
    }
    if (BIO_write(l_pSess->m_pRbio, l_aucBuf, l_iN) <= 0)
    {
      ERR_print_errors_fp(stderr);
      return false;
    }
  }
  // mTLS 인증서 검증
  if (m_bmTLS && (m_pAuth != nullptr))
  {
    const long l_lVerify = SSL_get_verify_result(l_pSess->m_pSsl);
    if (l_lVerify != X509_V_OK)
    {
      printf("[ERR_] CHybridTls: 피어 인증서 검증 실패 verify=%ld\n",
              l_lVerify);
      ERR_print_errors_fp(stderr);
      return false;
    }
  }
  // 동일 TCP 세그먼트에 HTTP 요청이 포함된 경우 선처리
  // (IOCP 등록 이후 호출되므로 _FlushWbio = CSocketPool::Send 경로 사용 가능)
  if ((l_pSess->m_pRbio != nullptr) && (BIO_pending(l_pSess->m_pRbio) > 0))
  {
    if (!_ReadPlain(*l_pSess, a_tSock))
    {
      return false;
    }
  }
  return true;
}

bool CHybridTls::ConnectHandshake(socket_t a_tSock)
{
  return _CreateSession(a_tSock, false);
}

bool CHybridTls::_FlushWbioDirect(T_TLS_SESSION& a_rSess, socket_t a_tSock)
{
  // 핸드셰이크 전용: IOCP 비동기 send 우회용 blocking ::send 송신
  if (a_rSess.m_pWbio == nullptr)
  {
    return true;
  }
  std::vector<uint8_t> l_vecOut;
  l_vecOut.reserve(static_cast<size_t>(BIO_pending(a_rSess.m_pWbio)));
  uint8_t l_aucChunk[4096];
  while (true)
  {
    const int l_iN = BIO_read(a_rSess.m_pWbio,
                              l_aucChunk,
                              static_cast<int>(sizeof(l_aucChunk)));
    if (l_iN <= 0)
    {
      break;
    }
    l_vecOut.insert(l_vecOut.end(), l_aucChunk, l_aucChunk + l_iN);
  }
  if (l_vecOut.empty())
  {
    return true;
  }
  return sfBlockingSend(a_tSock, l_vecOut.data(), l_vecOut.size());
}

bool CHybridTls::_FlushWbio(T_TLS_SESSION& a_rSess, socket_t a_tSock)
{
  if ((m_pPool == nullptr) || (a_rSess.m_pWbio == nullptr))
  {
    return false;
  }
  // wbio에 쌓인 TLS 레코드를 모두 읽은 뒤 단일 send()로 송신한다.
  // (기존: BIO_read마다 send() → TCP_NODELAY 환경에서 flight가 TCP 패킷 단위로 분할됨)
  std::vector<uint8_t> l_vecOut;
  l_vecOut.reserve(static_cast<size_t>(BIO_pending(a_rSess.m_pWbio)));
  uint8_t l_aucChunk[4096];
  while (true)
  {
    const int l_iN = BIO_read(a_rSess.m_pWbio,
                              l_aucChunk,
                              static_cast<int>(sizeof(l_aucChunk)));
    if (l_iN <= 0)
    {
      break;
    }
    l_vecOut.insert(l_vecOut.end(), l_aucChunk, l_aucChunk + l_iN);
  }
  if (l_vecOut.empty())
  {
    return true;
  }
  return m_pPool->Send(a_tSock, l_vecOut.data(), l_vecOut.size());
}

bool CHybridTls::_DriveHandshake(T_TLS_SESSION& a_rSess, socket_t a_tSock)
{
  if (a_rSess.m_bHandshakeDone)
  {
    return true;
  }
  // SSL_CTX + client_hello_cb 공유 시 병렬 SSL_accept가 CTX 내부 상태를 오염시킬 수 있음
  std::lock_guard<std::mutex> l_oAcceptLock(m_oSslAcceptMtx);
  for (;;)
  {
    int l_iRet = 0;
    if (a_rSess.m_bIsServer)
    {
      l_iRet = SSL_accept(a_rSess.m_pSsl);
    }
    else
    {
      l_iRet = SSL_connect(a_rSess.m_pSsl);
    }
    if (l_iRet == 1)
    {
      a_rSess.m_bHandshakeDone = true;
      if (m_bmTLS && (m_pAuth != nullptr))
      {
        const long l_lVerify = SSL_get_verify_result(a_rSess.m_pSsl);
        if (l_lVerify != X509_V_OK)
        {
          printf("[ERR_] CHybridTls: 피어 인증서 검증 실패 verify=%ld\n",
                  l_lVerify);
          ERR_print_errors_fp(stderr);
          return false;
        }
      }
      return _FlushWbio(a_rSess, a_tSock);
    }
    const int l_iErr = SSL_get_error(a_rSess.m_pSsl, l_iRet);
    if (l_iErr == SSL_ERROR_WANT_READ)
    {
      if (!_FlushWbio(a_rSess, a_tSock))
      {
        return false;
      }
      // memory BIO: rbio에 암호문이 남아 있으면 네트워크 recv 없이 SSL_accept 재시도
      if ((a_rSess.m_pRbio != nullptr) && (BIO_pending(a_rSess.m_pRbio) > 0))
      {
        continue;
      }
      return true;
    }
    if (l_iErr == SSL_ERROR_WANT_WRITE)
    {
      if (!_FlushWbio(a_rSess, a_tSock))
      {
        return false;
      }
      continue;
    }
    // peer alert 등: wbio에 남은 TLS 레코드 flush 후 종료
    (void)_FlushWbio(a_rSess, a_tSock);
    printf("[ERR_] CHybridTls 핸드쉐이크 실패 err=%d\n", l_iErr);
    ERR_print_errors_fp(stderr);
    return false;
  }
}

bool CHybridTls::_ReadPlain(T_TLS_SESSION& a_rSess, socket_t a_tSock)
{
  if (!m_cbfPlainRecv)
  {
    return true;
  }
  uint8_t l_aucBuf[D_PQCHTTPS_RECV_BUF];
  while (true)
  {
    const int l_iN = SSL_read(a_rSess.m_pSsl,
                              l_aucBuf,
                              static_cast<int>(sizeof(l_aucBuf)));
    if (l_iN > 0)
    {
      m_cbfPlainRecv(a_tSock, l_aucBuf, static_cast<size_t>(l_iN));
    }
    else if (l_iN == 0)
    {
      break;
    }
    else
    {
      const int l_iErr = SSL_get_error(a_rSess.m_pSsl, l_iN);
      if (l_iErr == SSL_ERROR_WANT_READ)
      {
        if ((a_rSess.m_pRbio != nullptr) && (BIO_pending(a_rSess.m_pRbio) > 0))
        {
          continue;
        }
        break;
      }
      if (l_iErr == SSL_ERROR_ZERO_RETURN)
      {
        break;
      }
      if (l_iErr == SSL_ERROR_SYSCALL)
      {
        if (ERR_peek_error() == 0)
        {
          break;
        }
      }
      if (l_iErr == SSL_ERROR_SSL)
      {
        // Connection: close 응답 후 peer close_notify·alert ? 정상 종료로 처리
        break;
      }
      printf("[ERR_] CHybridTls::SSL_read 실패 err=%d\n", l_iErr);
      ERR_print_errors_fp(stderr);
      return false;
    }
  }
  return _FlushWbio(a_rSess, a_tSock);
}

bool CHybridTls::OnRecvRaw(socket_t a_tSock,
                           const uint8_t* a_pucData,
                           size_t a_ullLen)
{
  auto l_pSess = _GetSession(a_tSock);
  if (l_pSess == nullptr)
  {
    printf("[ERR_:RECV:TCP_:%05d] CHybridTls::OnRecvRaw TLS 세션 없음\n",
            static_cast<int>(a_tSock));
    return false;
  }
  if ((a_pucData == nullptr) || (a_ullLen == 0))
  {
    return false;
  }
  std::lock_guard<std::recursive_mutex> l_oLock(l_pSess->m_oMtx);
  if (l_pSess->m_pRbio == nullptr)
  {
    return false;
  }
  const int l_iWritten = BIO_write(l_pSess->m_pRbio,
                                   a_pucData,
                                   static_cast<int>(a_ullLen));
  if (l_iWritten <= 0)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }
  if (!l_pSess->m_bHandshakeDone)
  {
    // IOCP recv 1회에 TLS 레코드가 여러 개 올 수 있음 → rbio 소진까지 핸드셰이크 진행
    for (;;)
    {
      if (!_DriveHandshake(*l_pSess, a_tSock))
      {
        return false;
      }
      if (l_pSess->m_bHandshakeDone)
      {
        break;
      }
      if ((l_pSess->m_pRbio == nullptr) || (BIO_pending(l_pSess->m_pRbio) <= 0))
      {
        break;
      }
    }
    if (!l_pSess->m_bHandshakeDone)
    {
      return true;
    }
  }
  return _ReadPlain(*l_pSess, a_tSock);
}

int CHybridTls::Read(socket_t a_tSock, uint8_t* a_pucBuf, size_t a_ullLen)
{
  auto l_pSess = _GetSession(a_tSock);
  if ((l_pSess == nullptr) || (a_pucBuf == nullptr) || (a_ullLen == 0))
  {
    return -1;
  }
  std::lock_guard<std::recursive_mutex> l_oLock(l_pSess->m_oMtx);
  if (!l_pSess->m_bHandshakeDone)
  {
    return -1;
  }
  return SSL_read(l_pSess->m_pSsl, a_pucBuf, static_cast<int>(a_ullLen));
}

int CHybridTls::Write(socket_t a_tSock, const uint8_t* a_pucBuf, size_t a_ullLen)
{
  auto l_pSess = _GetSession(a_tSock);
  if ((l_pSess == nullptr) || (a_pucBuf == nullptr) || (a_ullLen == 0))
  {
    return -1;
  }
  std::lock_guard<std::recursive_mutex> l_oLock(l_pSess->m_oMtx);
  if (!l_pSess->m_bHandshakeDone)
  {
    return -1;
  }
  size_t l_ullOff = 0;
  while (l_ullOff < a_ullLen)
  {
    const int l_iN = SSL_write(l_pSess->m_pSsl,
                               a_pucBuf + l_ullOff,
                               static_cast<int>(a_ullLen - l_ullOff));
    if (l_iN > 0)
    {
      l_ullOff += static_cast<size_t>(l_iN);
      continue;
    }
    if (l_iN == 0)
    {
      return 0;
    }
    const int l_iErr = SSL_get_error(l_pSess->m_pSsl, l_iN);
    if ((l_iErr == SSL_ERROR_WANT_WRITE) || (l_iErr == SSL_ERROR_WANT_READ))
    {
      if (!_FlushWbio(*l_pSess, a_tSock))
      {
        return -1;
      }
      continue;
    }
    (void)_FlushWbio(*l_pSess, a_tSock);
    return l_iN;
  }
  if (!_FlushWbio(*l_pSess, a_tSock))
  {
    return -1;
  }
  return static_cast<int>(a_ullLen);
}

void CHybridTls::RemoveSession(socket_t a_tSock)
{
  std::shared_ptr<T_TLS_SESSION> l_pSess;
  {
    std::lock_guard<std::mutex> l_oLock(m_oMapMtx);
    const auto l_oIt = m_oSessionMap.find(a_tSock);
    if (l_oIt == m_oSessionMap.end())
    {
      return;
    }
    l_pSess = l_oIt->second;
    m_oSessionMap.erase(l_oIt);
  }
  if (l_pSess != nullptr)
  {
    std::lock_guard<std::recursive_mutex> l_oSessLock(l_pSess->m_oMtx);
    if (l_pSess->m_pSsl != nullptr)
    {
      if (l_pSess->m_bHandshakeDone)
      {
        if ((m_pPool != nullptr) && (l_pSess->m_pWbio != nullptr))
        {
          (void)_FlushWbio(*l_pSess, a_tSock);
        }
        SSL_shutdown(l_pSess->m_pSsl);
        if ((m_pPool != nullptr) && (l_pSess->m_pWbio != nullptr))
        {
          (void)_FlushWbio(*l_pSess, a_tSock);
        }
      }
      SSL_free(l_pSess->m_pSsl);
      l_pSess->m_pSsl = nullptr;
    }
    l_pSess->m_pRbio = nullptr;
    l_pSess->m_pWbio = nullptr;
  }
}

X509* CHybridTls::GetPeerCert(socket_t a_tSock) const
{
  auto l_pSess = _GetSession(a_tSock);
  if ((l_pSess == nullptr) || !l_pSess->m_bHandshakeDone)
  {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> l_oLock(l_pSess->m_oMtx);
  return SSL_get_peer_certificate(l_pSess->m_pSsl);
}

bool CHybridTls::IsHandshakeDone(socket_t a_tSock) const
{
  auto l_pSess = _GetSession(a_tSock);
  if (l_pSess == nullptr)
  {
    return false;
  }
  std::lock_guard<std::recursive_mutex> l_oLock(l_pSess->m_oMtx);
  return l_pSess->m_bHandshakeDone;
}

SSL* CHybridTls::GetSsl(socket_t a_tSock) const
{
  auto l_pSess = _GetSession(a_tSock);
  if (l_pSess == nullptr)
  {
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> l_oLock(l_pSess->m_oMtx);
  return l_pSess->m_pSsl;
}

// =============================================================================
// CHttpParser (llhttp pimpl)
// =============================================================================
namespace {

// CHttpParser pimpl: llhttp 콜백 상태·파싱 중간값 보관
struct T_HTTP_PARSER_IMPL
{
  llhttp_t*           m_pParser   { nullptr }; // llhttp 파서 인스턴스
  llhttp_settings_t   m_tSettings {};          // llhttp 콜백 settings
  CBFP_HTTP_REQUEST   m_cbfOnRequest;          // message_complete 시 호출 콜백
  T_HTTP_REQ          m_tReq;                  // 현재 메시지 파싱 결과
  std::string         m_strCurHeaderField;     // header_field 콜백 임시 필드명
  socket_t            m_tSock     { D_INVALID_SOCK }; // 콜백에 전달할 소켓 fd
  bool                m_bMessageComplete { false };   // HTTP 메시지 1건 완료 플래그
};

} // namespace

// void* pimpl 포인터를 T_HTTP_PARSER_IMPL*로 캐스팅
static T_HTTP_PARSER_IMPL* ufGetParserImpl(void* a_pImpl)
{
  return static_cast<T_HTTP_PARSER_IMPL*>(a_pImpl);
}

// llhttp: 새 HTTP 메시지 시작 시 요청 컨텍스트 초기화
static int sfLlhttpOnMessageBegin(llhttp_t* a_pParser)
{
  auto* l_pImpl = static_cast<T_HTTP_PARSER_IMPL*>(a_pParser->data);
  if (l_pImpl == nullptr)
  {
    return -1;
  }
  l_pImpl->m_tReq = T_HTTP_REQ();
  l_pImpl->m_strCurHeaderField.clear();
  l_pImpl->m_bMessageComplete = false;
  return 0;
}

// llhttp: 요청 URL(path+query) 조각 누적
static int sfLlhttpOnUrl(llhttp_t* a_pParser, const char* a_pszAt, size_t a_ullLen)
{
  auto* l_pImpl = static_cast<T_HTTP_PARSER_IMPL*>(a_pParser->data);
  if (l_pImpl == nullptr)
  {
    return -1;
  }
  l_pImpl->m_tReq.m_strUrl.assign(a_pszAt, a_ullLen);
  return 0;
}

// llhttp: 헤더 필드명 조각 수신
static int sfLlhttpOnHeaderField(llhttp_t* a_pParser, const char* a_pszAt, size_t a_ullLen)
{
  auto* l_pImpl = static_cast<T_HTTP_PARSER_IMPL*>(a_pParser->data);
  if (l_pImpl == nullptr)
  {
    return -1;
  }
  l_pImpl->m_strCurHeaderField.assign(a_pszAt, a_ullLen);
  return 0;
}

// llhttp: 헤더 값 수신 후 m_oHeaders에 field→value 저장
static int sfLlhttpOnHeaderValue(llhttp_t* a_pParser, const char* a_pszAt, size_t a_ullLen)
{
  auto* l_pImpl = static_cast<T_HTTP_PARSER_IMPL*>(a_pParser->data);
  if (l_pImpl == nullptr)
  {
    return -1;
  }
  std::string l_strVal(a_pszAt, a_ullLen);
  l_pImpl->m_tReq.m_oHeaders[l_pImpl->m_strCurHeaderField] = std::move(l_strVal);
  return 0;
}

// llhttp: 요청 본문 조각을 m_strBody에 append
static int sfLlhttpOnBody(llhttp_t* a_pParser, const char* a_pszAt, size_t a_ullLen)
{
  auto* l_pImpl = static_cast<T_HTTP_PARSER_IMPL*>(a_pParser->data);
  if (l_pImpl == nullptr)
  {
    return -1;
  }
  l_pImpl->m_tReq.m_strBody.append(a_pszAt, a_ullLen);
  return 0;
}

// llhttp: 메시지 완료 시 method 설정 후 m_cbfOnRequest 호출
static int sfLlhttpOnMessageComplete(llhttp_t* a_pParser)
{
  auto* l_pImpl = static_cast<T_HTTP_PARSER_IMPL*>(a_pParser->data);
  if (l_pImpl == nullptr)
  {
    return -1;
  }
  l_pImpl->m_tReq.m_strMethod = llhttp_method_name(
    static_cast<llhttp_method_t>(llhttp_get_method(a_pParser)));
  l_pImpl->m_bMessageComplete = true;
  if (l_pImpl->m_cbfOnRequest)
  {
    l_pImpl->m_cbfOnRequest(l_pImpl->m_tSock, l_pImpl->m_tReq);
  }
  return 0;
}

CHttpParser::CHttpParser()
  : m_pImpl(nullptr)
{
}

CHttpParser::~CHttpParser()
{
  T_HTTP_PARSER_IMPL* l_pImpl = ufGetParserImpl(m_pImpl);
  if (l_pImpl != nullptr)
  {
    if (l_pImpl->m_pParser != nullptr)
    {
      delete l_pImpl->m_pParser;
      l_pImpl->m_pParser = nullptr;
    }
    delete l_pImpl;
    m_pImpl = nullptr;
  }
}

bool CHttpParser::Init()
{
  T_HTTP_PARSER_IMPL* l_pImpl = ufGetParserImpl(m_pImpl);
  if (l_pImpl == nullptr)
  {
    l_pImpl = new T_HTTP_PARSER_IMPL();
    m_pImpl = l_pImpl;
  }
  if (l_pImpl->m_pParser != nullptr)
  {
    delete l_pImpl->m_pParser;
    l_pImpl->m_pParser = nullptr;
  }
  llhttp_settings_init(&l_pImpl->m_tSettings);
  l_pImpl->m_tSettings.on_message_begin = sfLlhttpOnMessageBegin;
  l_pImpl->m_tSettings.on_url = sfLlhttpOnUrl;
  l_pImpl->m_tSettings.on_header_field = sfLlhttpOnHeaderField;
  l_pImpl->m_tSettings.on_header_value = sfLlhttpOnHeaderValue;
  l_pImpl->m_tSettings.on_body = sfLlhttpOnBody;
  l_pImpl->m_tSettings.on_message_complete = sfLlhttpOnMessageComplete;

  l_pImpl->m_pParser = new llhttp_t();
  if (l_pImpl->m_pParser == nullptr)
  {
    return false;
  }
  llhttp_init(l_pImpl->m_pParser, HTTP_REQUEST, &l_pImpl->m_tSettings);
  l_pImpl->m_pParser->data = l_pImpl;
  return true;
}

void CHttpParser::Reset()
{
  T_HTTP_PARSER_IMPL* l_pImpl = ufGetParserImpl(m_pImpl);
  if ((l_pImpl != nullptr) && (l_pImpl->m_pParser != nullptr))
  {
    llhttp_reset(l_pImpl->m_pParser);
    l_pImpl->m_pParser->data = l_pImpl;
  }
  if (l_pImpl != nullptr)
  {
    l_pImpl->m_tReq = T_HTTP_REQ();
    l_pImpl->m_strCurHeaderField.clear();
    l_pImpl->m_bMessageComplete = false;
  }
}

bool CHttpParser::ConsumeMessageComplete()
{
  T_HTTP_PARSER_IMPL* l_pImpl = ufGetParserImpl(m_pImpl);
  if ((l_pImpl == nullptr) || !l_pImpl->m_bMessageComplete)
  {
    return false;
  }
  l_pImpl->m_bMessageComplete = false;
  Reset();
  return true;
}

void CHttpParser::SetOnRequest(CBFP_HTTP_REQUEST a_cbf)
{
  T_HTTP_PARSER_IMPL* l_pImpl = ufGetParserImpl(m_pImpl);
  if (l_pImpl != nullptr)
  {
    l_pImpl->m_cbfOnRequest = std::move(a_cbf);
  }
}

void CHttpParser::SetSocket(socket_t a_tSock)
{
  T_HTTP_PARSER_IMPL* l_pImpl = ufGetParserImpl(m_pImpl);
  if (l_pImpl != nullptr)
  {
    l_pImpl->m_tSock = a_tSock;
  }
}

bool CHttpParser::Execute(const uint8_t* a_pucData, size_t a_ullLen)
{
  T_HTTP_PARSER_IMPL* l_pImpl = ufGetParserImpl(m_pImpl);
  if ((l_pImpl == nullptr) || (l_pImpl->m_pParser == nullptr) ||
      (a_pucData == nullptr) || (a_ullLen == 0))
  {
    return false;
  }
  const llhttp_errno_t l_eErr = llhttp_execute(l_pImpl->m_pParser,
                                               reinterpret_cast<const char*>(a_pucData),
                                               a_ullLen);
  if ((l_eErr != HPE_OK) && (l_eErr != HPE_PAUSED))
  {
    printf("[ERR_] llhttp_execute 실패: %s\n",
            llhttp_errno_name(l_eErr));
    return false;
  }
  return true;
}

std::string CHttpParser::BuildResponse(int a_iStatus,
                                       const char* a_pszBody,
                                       size_t a_ullLen,
                                       const char* a_pszContentType)
{
  const char* l_cpszReason = "OK";
  if (a_iStatus == 404)
  {
    l_cpszReason = "Not Found";
  }
  else if (a_iStatus == 405)
  {
    l_cpszReason = "Method Not Allowed";
  }
  else if (a_iStatus == 500)
  {
    l_cpszReason = "Internal Server Error";
  }
  const char* l_cpszType = (a_pszContentType != nullptr) ? a_pszContentType
                                                         : "application/json";
  const size_t l_ullBodyLen = (a_pszBody != nullptr) ? a_ullLen : 0u;
  char l_szHeader[512];
  const int l_iHdrLen = snprintf(l_szHeader,
                                 sizeof(l_szHeader),
                                 "HTTP/1.1 %d %s\r\n"
                                 "Content-Type: %s\r\n"
                                 "Content-Length: %zu\r\n"
                                 "Connection: close\r\n"
                                 "\r\n",
                                 a_iStatus,
                                 l_cpszReason,
                                 l_cpszType,
                                 l_ullBodyLen);
  std::string l_strResp;
  if (l_iHdrLen > 0)
  {
    l_strResp.assign(l_szHeader, static_cast<size_t>(l_iHdrLen));
  }
  if ((a_pszBody != nullptr) && (l_ullBodyLen > 0))
  {
    l_strResp.append(a_pszBody, l_ullBodyLen);
  }
  return l_strResp;
}

// =============================================================================
// CHttpRouter
// =============================================================================
void CHttpRouter::AddRoute(const char* a_pszMethod,
                           const char* a_pszPath,
                           CBFP_ROUTE_HANDLER a_cbfHandler)
{
  T_ROUTE_KEY l_tKey;
  l_tKey.m_strMethod = a_pszMethod;
  l_tKey.m_strPath = a_pszPath;
  m_oRoutes[l_tKey] = std::move(a_cbfHandler);
}

void CHttpRouter::SetStatsProducer(CShmStatsProducer* a_pProducer)
{
  m_pStatsProducer = a_pProducer;
}

bool CHttpRouter::Dispatch(socket_t a_tSock,
                           const T_HTTP_REQ& a_rReq,
                           CHybridTls& a_rTls,
                           size_t a_ullConnCount)
{
  if (m_pStatsProducer != nullptr)
  {
    m_pStatsProducer->OnConnect();
    m_pStatsProducer->AddRequest();
    m_pStatsProducer->AddRxBytes(static_cast<uint64_t>(a_rReq.m_strBody.size()));
  }

  T_ROUTE_KEY l_tKey;
  l_tKey.m_strMethod = a_rReq.m_strMethod;
  l_tKey.m_strPath = a_rReq.m_strUrl;

  const auto l_oIt = m_oRoutes.find(l_tKey);
  if (l_oIt == m_oRoutes.end())
  {
    const std::string l_strBody = "{\"error\":\"not found\"}";
    const std::string l_strResp = CHttpParser::BuildResponse(
      404, l_strBody.c_str(), l_strBody.size());
    a_rTls.Write(a_tSock,
                 reinterpret_cast<const uint8_t*>(l_strResp.data()),
                 l_strResp.size());
    if (m_pStatsProducer != nullptr)
    {
      m_pStatsProducer->AddTxBytes(static_cast<uint64_t>(l_strResp.size()));
      m_pStatsProducer->OnDisconnect();
    }
    (void)a_ullConnCount;
    return false;
  }
  int l_iStatus = 200;
  std::string l_strBody;
  std::string l_strContentType;
  if (!l_oIt->second(a_tSock, a_rReq, l_iStatus, l_strBody, l_strContentType))
  {
    l_iStatus = 500;
    l_strBody = "{\"error\":\"handler failed\"}";
    l_strContentType.clear();
  }
  const char* l_cpszType = l_strContentType.empty() ? "application/json"
                                                    : l_strContentType.c_str();
  const std::string l_strResp = CHttpParser::BuildResponse(
    l_iStatus, l_strBody.c_str(), l_strBody.size(), l_cpszType);
  a_rTls.Write(a_tSock,
               reinterpret_cast<const uint8_t*>(l_strResp.data()),
               l_strResp.size());
  if (m_pStatsProducer != nullptr)
  {
    m_pStatsProducer->AddTxBytes(static_cast<uint64_t>(l_strResp.size()));
    m_pStatsProducer->OnDisconnect();
  }
  (void)a_ullConnCount;
  return true;
}

// =============================================================================
// CPqcHttpService
// =============================================================================
CPqcHttpService::CPqcHttpService()
  : m_bRunStatsLoop(false)
{
}

CPqcHttpService::~CPqcHttpService()
{
  Shutdown();
}

bool CPqcHttpService::Init()
{
  if (!m_oShmStatsProducer.Init())
  {
    printf("[ERR_] CPqcHttpService::Init CShmStatsProducer 실패\n");
    return false;
  }
  m_bRunStatsLoop.store(true);
  m_oStatsThread = std::thread(&CPqcHttpService::updateStatsLoop, this);
  return true;
}

void CPqcHttpService::Shutdown()
{
  m_bRunStatsLoop.store(false);
  if (m_oStatsThread.joinable())
  {
    m_oStatsThread.join();
  }
  m_oShmStatsProducer.Shutdown();
}

CShmStatsProducer* CPqcHttpService::GetStatsProducer()
{
  return &m_oShmStatsProducer;
}

void CPqcHttpService::updateStatsLoop()
{
  while (m_bRunStatsLoop.load())
  {
    m_oShmStatsProducer.Update();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
// -----------------------------------------------------------------------------
