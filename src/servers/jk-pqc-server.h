/*----------------------------------------------------------------------------+-
jk-pqc-server.h
-+----------------------------------------------------------------------------+-
Description : jk-pqc-server 서버 컨텍스트·라우트 함수 선언
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/05/25] 최초 작성 (jk-pqc-server.cpp에서 라우트 함수 분리)
-+----------------------------------------------------------------------------*/
#ifndef D_JK_PQC_SERVER_H
#define D_JK_PQC_SERVER_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "pqchttps.h"

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

// jk-pqc-server.cpp에서 정의되는 전역 컨텍스트 (라우트 함수가 참조)
extern T_SERVER_CTX g_tCtx;

// =============================================================================
// 라우트 헬퍼·핸들러·등록 함수 선언
// =============================================================================
// HTTP 요청 헤더 맵에서 필드명 대소문자 무시 조회
std::string ufGetHeaderCi(const T_HTTP_REQ& a_rReq, const char* a_pszName);

// GET /  : 브라우저용 HTML 또는 plain text 응답
bool smfRouteRoot(socket_t a_tSock,
                  const T_HTTP_REQ& a_rReq,
                  int& a_iStatus,
                  std::string& a_rstrBody,
                  std::string& a_rstrContentType);

// GET /health  : 상태·버전 JSON
bool smfRouteHealth(socket_t a_tSock,
                    const T_HTTP_REQ& a_rReq,
                    int& a_iStatus,
                    std::string& a_rstrBody,
                    std::string& a_rstrContentType);

// GET /info  : 서버·클라이언트 CN 정보 JSON
bool smfRouteInfo(socket_t a_tSock,
                  const T_HTTP_REQ& a_rReq,
                  int& a_iStatus,
                  std::string& a_rstrBody,
                  std::string& a_rstrContentType);

// POST /echo  : 요청 바디 그대로 응답
bool smfRouteEcho(socket_t a_tSock,
                  const T_HTTP_REQ& a_rReq,
                  int& a_iStatus,
                  std::string& a_rstrBody,
                  std::string& a_rstrContentType);

// GET /api/status  : KEM·서명·Cipher Suite JSON
bool smfRouteApiStatus(socket_t a_tSock,
                       const T_HTTP_REQ& a_rReq,
                       int& a_iStatus,
                       std::string& a_rstrBody,
                       std::string& a_rstrContentType);

// GET /api/session  : 현재 세션 KEM·Cipher·TLS 버전 JSON
bool smfRouteApiSession(socket_t a_tSock,
                        const T_HTTP_REQ& a_rReq,
                        int& a_iStatus,
                        std::string& a_rstrBody,
                        std::string& a_rstrContentType);

// GET /api/client/info  : 클라이언트 인증서 Subject·유효기간 JSON
bool smfRouteApiClientInfo(socket_t a_tSock,
                           const T_HTTP_REQ& a_rReq,
                           int& a_iStatus,
                           std::string& a_rstrBody,
                           std::string& a_rstrContentType);

// GET /api/stats  : 공유메모리 통계 스냅샷 JSON
bool smfRouteApiStats(socket_t a_tSock,
                      const T_HTTP_REQ& a_rReq,
                      int& a_iStatus,
                      std::string& a_rstrBody,
                      std::string& a_rstrContentType);

// POST /api/echo  : 요청 바디 그대로 응답
bool smfRouteApiEcho(socket_t a_tSock,
                     const T_HTTP_REQ& a_rReq,
                     int& a_iStatus,
                     std::string& a_rstrBody,
                     std::string& a_rstrContentType);

// CHttpRouter에 위 라우트들을 일괄 등록
void sfRegisterRoutes(CHttpRouter& a_rRouter, CPqcHttpService& a_rService);
// -----------------------------------------------------------------------------
#endif // D_JK_PQC_SERVER_H
