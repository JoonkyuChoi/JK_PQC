/*----------------------------------------------------------------------------+-
jk-pqc-server-routes.cpp
-+----------------------------------------------------------------------------+-
Description : jk-pqc-server HTTP 라우트 핸들러 구현
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/05/25] 최초 작성 (jk-pqc-server.cpp에서 라우트 함수 분리)
-+----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <iomanip>
#include <sstream>
#include <string>

#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <json.hpp>

#include "jk-pqc-server.h"
#include "pqc_utils.h"
#include "root_page.h"

// =============================================================================
// 헤더 헬퍼
// =============================================================================
std::string ufGetHeaderCi(const T_HTTP_REQ& a_rReq, const char* a_pszName)
{
  if (a_pszName == nullptr)
  {
    return {};
  }
  for (const auto& l_oPair : a_rReq.m_oHeaders)
  {
#ifdef _WIN32
    if (_stricmp(l_oPair.first.c_str(), a_pszName) == 0)
#else
    if (strcasecmp(l_oPair.first.c_str(), a_pszName) == 0)
#endif
    {
      return l_oPair.second;
    }
  }
  return {};
}

// =============================================================================
// HTTP 라우트 핸들러
// =============================================================================
bool smfRouteRoot(socket_t a_tSock,
                  const T_HTTP_REQ& a_rReq,
                  int& a_iStatus,
                  std::string& a_rstrBody,
                  std::string& a_rstrContentType)
{
  if (g_tCtx.m_pTls != nullptr)
  {
    SSL* l_pSsl = g_tCtx.m_pTls->GetSsl(a_tSock);
    if (l_pSsl != nullptr)
    {
      gfPrintInfo4SSL(l_pSsl);
    }
  }
  const std::string l_strUA     = ufGetHeaderCi(a_rReq, "User-Agent");
  const std::string l_strAccept = ufGetHeaderCi(a_rReq, "Accept");
  const bool l_bIsBrowser =
    (l_strUA.find("Mozilla") != std::string::npos) ||
    (l_strAccept.find("text/html") != std::string::npos);

  a_iStatus = 200;
  if (l_bIsBrowser)
  {
    a_rstrBody = D_ROOT_PAGE_HTML;
    a_rstrContentType = "text/html";
  }
  else
  {
    a_rstrBody = "Hello from PQC (HTTPS) Server";
    a_rstrContentType = "text/plain";
  }
  return true;
}

bool smfRouteHealth(socket_t a_tSock,
                    const T_HTTP_REQ& a_rReq,
                    int& a_iStatus,
                    std::string& a_rstrBody,
                    std::string& a_rstrContentType)
{
  (void)a_tSock;
  (void)a_rReq;
  (void)a_rstrContentType;
  a_iStatus = 200;
  a_rstrBody = std::string("{\"status\":\"ok\",\"version\":\"") + D_PQCHTTPS_VERSION + "\"}";
  return true;
}

bool smfRouteInfo(socket_t a_tSock,
                  const T_HTTP_REQ& a_rReq,
                  int& a_iStatus,
                  std::string& a_rstrBody,
                  std::string& a_rstrContentType)
{
  (void)a_rReq;
  (void)a_rstrContentType;
  a_iStatus = 200;
  std::string l_strCn = "N/A";
  if (g_tCtx.m_pTls != nullptr)
  {
    X509* l_pPeer = g_tCtx.m_pTls->GetPeerCert(a_tSock);
    if (l_pPeer != nullptr)
    {
      if (g_tCtx.m_pAuth != nullptr)
      {
        g_tCtx.m_pAuth->GetSubjectCN(l_pPeer, l_strCn);
      }
      X509_free(l_pPeer);
    }
    SSL* l_pSsl = g_tCtx.m_pTls->GetSsl(a_tSock);
    if (l_pSsl != nullptr)
    {
      gfPrintInfo4SSL(l_pSsl);
    }
  }
  char l_szBuf[512];
  snprintf(l_szBuf,
           sizeof(l_szBuf),
           "{\"server\":\"jk-pqc-server\",\"version\":\"%s\","
           "\"port\":%u,\"client_cn\":\"%s\"}",
           D_PQCHTTPS_VERSION,
           static_cast<unsigned>(g_tCtx.m_usPort),
           l_strCn.c_str());
  a_rstrBody = l_szBuf;
  return true;
}

bool smfRouteEcho(socket_t a_tSock,
                  const T_HTTP_REQ& a_rReq,
                  int& a_iStatus,
                  std::string& a_rstrBody,
                  std::string& a_rstrContentType)
{
  (void)a_tSock;
  (void)a_rstrContentType;
  a_iStatus = 200;
  a_rstrBody = a_rReq.m_strBody.empty() ? "{}" : a_rReq.m_strBody;
  return true;
}

bool smfRouteApiStatus(socket_t a_tSock,
                       const T_HTTP_REQ& a_rReq,
                       int& a_iStatus,
                       std::string& a_rstrBody,
                       std::string& a_rstrContentType)
{
  (void)a_rReq;
  (void)a_rstrContentType;
  if (g_tCtx.m_pTls == nullptr)
  {
    return false;
  }

  std::string l_strCipher = "N/A";
  SSL*        l_pSsl      = g_tCtx.m_pTls->GetSsl(a_tSock);
  if (l_pSsl != nullptr)
  {
    const SSL_CIPHER* l_pCipher = SSL_get_current_cipher(l_pSsl);
    if (l_pCipher != nullptr)
    {
      l_strCipher = SSL_CIPHER_get_name(l_pCipher);
    }
  }

  nlohmann::json  l_oJson;
  l_oJson["status"    ] = "ok";
  l_oJson["server"    ] = "JK-PQC Hybrid mTLS Server";
  l_oJson["kem"       ] = g_tCtx.m_pTls->GetKemGroups();
  l_oJson["signature" ] = "ML-DSA-65";
  l_oJson["cipher"    ] = l_strCipher;
  a_iStatus   = 200;
  a_rstrBody  = l_oJson.dump();

  return true;
}

bool smfRouteApiSession(socket_t a_tSock,
                        const T_HTTP_REQ& a_rReq,
                        int& a_iStatus,
                        std::string& a_rstrBody,
                        std::string& a_rstrContentType)
{
  (void)a_rReq;
  (void)a_rstrContentType;
  if (g_tCtx.m_pTls == nullptr)
  {
    return false;
  }
  
  std::string l_strKemGroup   = "N/A";
  std::string l_strCipherSuite= "N/A";
  std::string l_strTlsVersion = "N/A";
  SSL*        l_pSsl          = g_tCtx.m_pTls->GetSsl(a_tSock);

  if (l_pSsl != nullptr)
  {
    const char* l_cpszGroupName = SSL_get0_group_name(l_pSsl);
    if ((l_cpszGroupName != nullptr) && (l_cpszGroupName[0] != '\0'))
    {
      l_strKemGroup = l_cpszGroupName;
    }
    const SSL_CIPHER* l_pCipher = SSL_get_current_cipher(l_pSsl);
    if (l_pCipher != nullptr)
    {
      l_strCipherSuite = SSL_CIPHER_get_name(l_pCipher);
    }
    const char* l_cpszVersion = SSL_get_version(l_pSsl);
    if (l_cpszVersion != nullptr)
    {
      l_strTlsVersion = l_cpszVersion;
    }
  }
  
  nlohmann::json l_oJson;
  l_oJson["kem_group"   ] = l_strKemGroup;
  l_oJson["cipher_suite"] = l_strCipherSuite;
  l_oJson["tls_version" ] = l_strTlsVersion;
  a_iStatus   = 200;
  a_rstrBody  = l_oJson.dump();

  return true;
}

bool smfRouteApiClientInfo(socket_t a_tSock,
                           const T_HTTP_REQ& a_rReq,
                           int& a_iStatus,
                           std::string& a_rstrBody,
                           std::string& a_rstrContentType)
{
  (void)a_rReq;
  (void)a_rstrContentType;
  if (g_tCtx.m_pTls == nullptr)
  {
    return false;
  }
  std::string l_strSubject  = "N/A";
  std::string l_strNotBefore= "N/A";
  std::string l_strNotAfter = "N/A";
  SSL* l_pSsl = g_tCtx.m_pTls->GetSsl(a_tSock);
  if (l_pSsl != nullptr)
  {
    X509* l_pPeerCert = SSL_get_peer_certificate(l_pSsl);
    if (l_pPeerCert != nullptr)
    {
      X509_NAME* l_pSubjectName = X509_get_subject_name(l_pPeerCert);
      if (l_pSubjectName != nullptr)
      {
        char l_caSubjectBuffer[512] = { 0 };
        X509_NAME_oneline(l_pSubjectName, l_caSubjectBuffer,
                          static_cast<int>(sizeof(l_caSubjectBuffer)));
        l_strSubject = l_caSubjectBuffer;
      }
      const ASN1_TIME* l_pNotBefore = X509_get0_notBefore(l_pPeerCert);
      const ASN1_TIME* l_pNotAfter = X509_get0_notAfter(l_pPeerCert);
      struct tm l_tTimeData = { 0 };
      if ((l_pNotBefore != nullptr) && (ASN1_TIME_to_tm(l_pNotBefore, &l_tTimeData) == 1))
      {
        std::ostringstream l_oSs;
        l_oSs << std::setfill('0') << std::setw(4) << (l_tTimeData.tm_year + 1900) << "-"
          << std::setw(2) << (l_tTimeData.tm_mon + 1) << "-"
          << std::setw(2) << l_tTimeData.tm_mday;
        l_strNotBefore = l_oSs.str();
      }
      if ((l_pNotAfter != nullptr) && (ASN1_TIME_to_tm(l_pNotAfter, &l_tTimeData) == 1))
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
  
  nlohmann::json l_oJson;
  l_oJson["subject"   ] = l_strSubject;
  l_oJson["not_before"] = l_strNotBefore;
  l_oJson["not_after" ] = l_strNotAfter;
  a_iStatus   = 200;
  a_rstrBody  = l_oJson.dump();

  return true;
}

bool smfRouteApiStats(socket_t a_tSock,
                      const T_HTTP_REQ& a_rReq,
                      int& a_iStatus,
                      std::string& a_rstrBody,
                      std::string& a_rstrContentType)
{
  (void)a_tSock;
  (void)a_rReq;
  (void)a_rstrContentType;
  if (g_tCtx.m_pService == nullptr)
  {
    return false;
  }
  
  const T_JKPQC_STATS l_tStats = g_tCtx.m_pService->GetStatsProducer()->GetSnapshot();
  nlohmann::json      l_oJson;
  
  l_oJson["version"             ] = l_tStats.m_uiVersion;
  l_oJson["current_connections" ] = l_tStats.m_uiCurrentConnections;
  l_oJson["max_connections"     ] = l_tStats.m_uiMaxConnections;
  l_oJson["total_rx_bytes"      ] = l_tStats.m_ullTotalBytes4RX;
  l_oJson["total_tx_bytes"      ] = l_tStats.m_ullTotalBytes4TX;
  l_oJson["total_requests"      ] = l_tStats.m_ullTotalRequests;
  l_oJson["uptime_seconds"      ] = l_tStats.m_ullUptimeSeconds;
  l_oJson["last_updated"        ] = l_tStats.m_ullLastUpdated;
  a_iStatus   = 200;
  a_rstrBody  = l_oJson.dump();

  return true;
}

bool smfRouteApiEcho(socket_t a_tSock,
                     const T_HTTP_REQ& a_rReq,
                     int& a_iStatus,
                     std::string& a_rstrBody,
                     std::string& a_rstrContentType)
{
  (void)a_tSock;
  (void)a_rstrContentType;
  
  a_iStatus   = 200;
  a_rstrBody  = a_rReq.m_strBody.empty() ? "{}" : a_rReq.m_strBody;

  return true;
}

// =============================================================================
// 라우트 등록
// =============================================================================
void sfRegisterRoutes(CHttpRouter& a_rRouter, CPqcHttpService& a_rService)
{
  a_rRouter.SetStatsProducer(a_rService.GetStatsProducer());
  a_rRouter.AddRoute("GET",  "/",                smfRouteRoot);
  a_rRouter.AddRoute("GET",  "/health",          smfRouteHealth);
  a_rRouter.AddRoute("GET",  "/info",            smfRouteInfo);
  a_rRouter.AddRoute("POST", "/echo",            smfRouteEcho);
  a_rRouter.AddRoute("GET",  "/api/status",      smfRouteApiStatus);
  a_rRouter.AddRoute("GET",  "/api/session",     smfRouteApiSession);
  a_rRouter.AddRoute("GET",  "/api/client/info", smfRouteApiClientInfo);
  a_rRouter.AddRoute("GET",  "/api/stats",       smfRouteApiStats);
  a_rRouter.AddRoute("POST", "/api/echo",        smfRouteApiEcho);
}
// -----------------------------------------------------------------------------
