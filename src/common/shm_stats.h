/*----------------------------------------------------------------------------+-
shm_stats.h
-+----------------------------------------------------------------------------+-
Description : "jk-https-server / jk-pqc-server" 실시간 통계 공유메모리 클래스 정의
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
-+----------------------------------------------------------------------------*/
#pragma once

#include <atomic>
#include <chrono>
#include <stdint.h>
// -----------------------------------------------------------------------------
// [DEFINES] - Windows Global 네임스페이스 공유 객체 이름
// -----------------------------------------------------------------------------
constexpr wchar_t SHM_NAME[]  = L"Global\\JK_PQC_STATS";       // FileMapping 객체 이름
constexpr wchar_t SHM_MUTEX[] = L"Global\\JK_PQC_STATS_MUTEX"; // 통계 갱신 동기화 Mutex
// -----------------------------------------------------------------------------
// [STRUCTS] - 대시보드(jk-https-dashboard)와 공유하는 통계 레이아웃
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct T_JKPQC_STATS
{
  uint32_t  m_uiVersion;              // 구조체 스키마 버전 (현재 1)
  uint32_t  m_uiCurrentConnections;   // 현재 동시 접속 수
  uint32_t  m_uiMaxConnections;       // 기록된 최대 동시 접속 수
  uint64_t  m_ullTotalBytes4RX;       // 누적 수신 바이트
  uint64_t  m_ullTotalBytes4TX;       // 누적 송신 바이트
  uint64_t  m_ullTotalRequests;       // 누적 HTTP 요청 처리 수
  uint64_t  m_ullUptimeSeconds;       // 서버 프로세스 가동 시간 (초)
  uint64_t  m_ullLastUpdated;         // 마지막 갱신 Unix epoch (초)
};
#pragma pack(pop)
// -----------------------------------------------------------------------------
// CShmStatsProducer : HTTPS/PQC 서버 → 공유메모리 통계 기록
// -----------------------------------------------------------------------------
class CShmStatsProducer
{
  // -------------------------------------
public:
  CShmStatsProducer();      // 생성자 - 카운터·핸들 초기화
  virtual ~CShmStatsProducer(); // 소멸자 - Shutdown()

  bool Init();                          // FileMapping·Mutex 생성 및 초기 스냅샷 기록
  void Shutdown();                      // MapView·Mutex·FileMapping 핸들 해제

  void OnConnect();                     // 접속 +1, 최대 접속 수 갱신
  void OnDisconnect();                  // 접속 -1 (0 미만 방지)
  void AddRxBytes(uint64_t a_ullBytes); // 수신 바이트 누적
  void AddTxBytes(uint64_t a_ullBytes); // 송신 바이트 누적
  void AddRequest();                    // 요청 처리 카운트 +1
  void Update();                        // uptime·last_updated 계산 후 SHM에 기록

  T_JKPQC_STATS GetSnapshot() const;    // 프로세스 내부 atomic 카운터 스냅샷

  // -------------------------------------
protected:
  // -------------------------------------
private:
  void _writeStats(const T_JKPQC_STATS& a_rtStats); // Mutex 잠금 후 SHM memcpy

  void*          m_pMapFile;  // CreateFileMappingW 핸들 (HANDLE)
  void*          m_pMutex;    // CreateMutexW(SHM_MUTEX) 핸들 (HANDLE)
  T_JKPQC_STATS* m_ptStats;   // MapViewOfFile 쓰기 가능 뷰

  std::atomic<uint32_t> m_uiCurrentConnections; // 실시간 접속 수
  std::atomic<uint32_t> m_uiMaxConnections;     // 피크 접속 수
  std::atomic<uint64_t> m_ullTotalRxBytes;      // 누적 RX (프로세스 로컬)
  std::atomic<uint64_t> m_ullTotalTxBytes;      // 누적 TX (프로세스 로컬)
  std::atomic<uint64_t> m_ullTotalRequests;     // 누적 요청 수 (프로세스 로컬)
  std::chrono::steady_clock::time_point m_tStartTime; // Init() 시점 (uptime 계산)
  bool m_bInitialized;  // Init() 성공·SHM 사용 가능 여부
  // -------------------------------------
};
// -----------------------------------------------------------------------------
