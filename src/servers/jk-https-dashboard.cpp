#pragma execution_character_set("utf-8")
/*----------------------------------------------------------------------------+-
jk-https-dashboard.cpp
-+----------------------------------------------------------------------------+-
Description : PQC(TLS) HTTPS 대시보드 서버 콘솔 프로그램
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
-+----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <winsock2.h>
// [JKC:20260428-1034] OPENSSL_Applink 문제 해결 (DLL 방식 OpenSSL 사용시에만 필요)
// Notes: 반드시 다른 OpenSSL 헤더보다 우선하여 include
#include <openssl/applink.c>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <json.hpp>

#include "httplib.h"
#include "pqc_utils.h"
#include "shm_consumer.h"

/*----------------------------------------------------------------------------+-
[18081] PQC(TLS) HTTPS 대시보드 서버
-+----------------------------------------------------------------------------+-
- 목적
  jk-https-server 공유메모리 통계를 브라우저에서 확인할 수 있는 대시보드 서버를 제공한다.
- 구현
  TLS(브라우저 접속) + 공유메모리 Consumer + /dashboard 렌더링을 수행한다.
-+----------------------------------------------------------------------------*/
int main(int a_iArgc, char** a_ppszArgv)
{
  // 콘솔창 인코딩 UTF8 적용
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  const int         l_iServerPort = (a_iArgc > 1) ? atoi(a_ppszArgv[1]) : 18081;
  const std::string l_strCertPath = (a_iArgc > 2) ? a_ppszArgv[2] : "certs/dashboard/dashboard-cert.pem";
  const std::string l_strKeyPath  = (a_iArgc > 3) ? a_ppszArgv[3] : "certs/dashboard/dashboard-key.pem";

  WSADATA l_tWsaData = { 0 };
  if (WSAStartup(MAKEWORD(2, 2), &l_tWsaData) != 0)
  {
    fprintf(stderr, "[ERR_] WSAStartup 실패\n");
    return 1;
  }

  if (!gfLoadProvider4OQS(NULL))
  {
    WSACleanup();
    return 1;
  }

  CShmStatsConsumer l_oConsumer;
  (void)l_oConsumer.Init(); // 서버 미실행 상태를 허용하므로 실패해도 진행한다.

  httplib::SSLServer l_oServer(l_strCertPath.c_str(), l_strKeyPath.c_str());
  if (!l_oServer.is_valid())
  {
    fprintf(stderr, "[ERR_] httplib::SSLServer 초기화 실패\n");
    ERR_print_errors_fp(stderr);
    l_oConsumer.Shutdown();
    WSACleanup();
    return 1;
  }

  SSL_CTX* l_pSslCtx = static_cast<SSL_CTX*>(l_oServer.tls_context());
  if (l_pSslCtx == NULL)
  {
    fprintf(stderr, "[ERR_] SSL_CTX 획득 실패\n");
    l_oConsumer.Shutdown();
    WSACleanup();
    return 1;
  }

  SSL_CTX_set_verify(l_pSslCtx, SSL_VERIFY_NONE, NULL);
  // [JKC:20260514-0710] Hybrid KEM 그룹 설정
  if (SSL_CTX_set1_groups_list(l_pSslCtx, "X25519MLKEM768:SecP256r1MLKEM768:X25519") != 1)
  {
    fprintf(stderr, "[ERR_] SSL_CTX_set1_groups_list 실패\n");
    ERR_print_errors_fp(stderr);
    l_oConsumer.Shutdown();
    WSACleanup();
    return 1;
  }

  l_oServer.Get("/", [](const httplib::Request&, httplib::Response& a_rRes)
  {
    a_rRes.status = 302;
    a_rRes.set_header("Location", "/dashboard");
  });

  l_oServer.Get("/dashboard", [](const httplib::Request&, httplib::Response& a_rRes)
  {
    const char* l_cpszHtml =
      "<!doctype html><html><head><meta charset='utf-8'><title>JK PQC Dashboard</title>"
      "<style>"
      "body{background:#0f172a;color:#e2e8f0;font-family:Segoe UI,Arial,sans-serif;margin:0;padding:24px;}"
      ".wrap{max-width:980px;margin:0 auto;}h1{margin:0 0 16px 0;font-size:28px;}"
      ".grid{display:grid;grid-template-columns:repeat(2,minmax(260px,1fr));gap:12px;}"
      ".card{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:16px;}"
      ".k{font-size:13px;color:#94a3b8;margin-bottom:8px}.v{font-size:26px;font-weight:700;transition:all .25s ease;}"
      ".online{color:#22c55e}.offline{color:#ef4444}"
      "@media (max-width:700px){.grid{grid-template-columns:1fr;}}"
      "</style></head><body><div class='wrap'><h1>JK-PQC HTTPS Dashboard</h1><div class='grid'>"
      "<div class='card'><div class='k'>서버 상태</div><div id='server_online' class='v'>-</div></div>"
      "<div class='card'><div class='k'>현재 접속자 수</div><div id='current_connections' class='v'>0</div></div>"
      "<div class='card'><div class='k'>최대 동시 접속자 수</div><div id='max_connections' class='v'>0</div></div>"
      "<div class='card'><div class='k'>수신 데이터 총량</div><div id='total_rx_bytes' class='v'>0 KB</div></div>"
      "<div class='card'><div class='k'>전송 데이터 총량</div><div id='total_tx_bytes' class='v'>0 KB</div></div>"
      "<div class='card'><div class='k'>총 요청 처리 수</div><div id='total_requests' class='v'>0</div></div>"
      "<div class='card'><div class='k'>서버 가동 시간</div><div id='uptime_seconds' class='v'>00:00:00</div></div>"
      "<div class='card'><div class='k'>마지막 갱신 시각</div><div id='last_updated' class='v'>N/A</div></div>"
      "</div></div><script>"
      "async function u(){"
      "try{"
      "const r=await fetch('/api/stats',{cache:'no-store'});const j=await r.json();"
      "const s=document.getElementById('server_online');"
      "if(j.server_online){s.textContent='\\uD83D\\uDFE2 Online';s.className='v online';}"
      "else{s.textContent='\\uD83D\\uDD34 Offline';s.className='v offline';}"
      "const fb=(b)=>{const kb=b/1024;const mb=kb/1024;return mb>=1?mb.toFixed(2)+' MB':kb.toFixed(2)+' KB';};"
      "const fu=(sec)=>{const h=Math.floor(sec/3600);const m=Math.floor((sec%3600)/60);const s=sec%60;"
      "return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');};"
      "const fd=(ts)=>{if(!ts){return 'N/A';}const d=new Date(ts*1000);const p=(n)=>String(n).padStart(2,'0');"
      "return d.getFullYear()+'-'+p(d.getMonth()+1)+'-'+p(d.getDate())+' '+p(d.getHours())+':'+p(d.getMinutes())+':'+p(d.getSeconds());};"
      "document.getElementById('current_connections').textContent=j.current_connections;"
      "document.getElementById('max_connections').textContent=j.max_connections;"
      "document.getElementById('total_rx_bytes').textContent=fb(j.total_rx_bytes);"
      "document.getElementById('total_tx_bytes').textContent=fb(j.total_tx_bytes);"
      "document.getElementById('total_requests').textContent=j.total_requests;"
      "document.getElementById('uptime_seconds').textContent=fu(j.uptime_seconds);"
      "document.getElementById('last_updated').textContent=fd(j.last_updated);"
      "}catch(e){const s=document.getElementById('server_online');s.textContent='\\uD83D\\uDD34 Offline';s.className='v offline';}"
      "}"
      "setInterval(u,2000);u();"
      "</script></body></html>";
    a_rRes.set_content(l_cpszHtml, "text/html; charset=utf-8");
  });

  l_oServer.Get("/api/stats", [&l_oConsumer](const httplib::Request&, httplib::Response& a_rRes)
  {
    T_JKPQC_STATS l_tStats = {};
    bool l_bOnline = l_oConsumer.Read(l_tStats);

    if (!l_bOnline)
    {
      l_oConsumer.Shutdown();
      (void)l_oConsumer.Init();
    }

    nlohmann::json l_oJson;
    l_oJson["current_connections"] = l_bOnline ? l_tStats.m_uiCurrentConnections : 0;
    l_oJson["max_connections"] = l_bOnline ? l_tStats.m_uiMaxConnections : 0;
    l_oJson["total_rx_bytes"] = l_bOnline ? l_tStats.m_ullTotalBytes4RX : 0;
    l_oJson["total_tx_bytes"] = l_bOnline ? l_tStats.m_ullTotalBytes4TX : 0;
    l_oJson["total_requests"] = l_bOnline ? l_tStats.m_ullTotalRequests : 0;
    l_oJson["uptime_seconds"] = l_bOnline ? l_tStats.m_ullUptimeSeconds : 0;
    l_oJson["last_updated"] = l_bOnline ? l_tStats.m_ullLastUpdated : 0;
    l_oJson["server_online"] = l_bOnline;
    a_rRes.set_content(l_oJson.dump(), "application/json; charset=utf-8");
  });

  printf("대시보드 서버 대기 중: https://0.0.0.0:%d\n", l_iServerPort);
  if (!l_oServer.listen("0.0.0.0", l_iServerPort))
  {
    fprintf(stderr, "[ERR_] HTTPS 대시보드 서버 listen 실패\n");
    ERR_print_errors_fp(stderr);
    l_oConsumer.Shutdown();
    WSACleanup();
    return 1;
  }

  l_oConsumer.Shutdown();
  WSACleanup();
  return 0;
}
// -----------------------------------------------------------------------------
