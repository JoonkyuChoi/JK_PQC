/*----------------------------------------------------------------------------+-
shm_stats.h
-+----------------------------------------------------------------------------+-
Description : "jk-https-server 실시간 통계정보" 공유메모리 클래스 정의
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
-+----------------------------------------------------------------------------*/
#pragma once

#include <atomic>
#include <chrono>
#include <stdint.h>
// -----------------------------------------------------------------------------
// [DEFINES]
// -----------------------------------------------------------------------------
// 공유 메모리 이름 (전역 네임스페이스)
constexpr wchar_t SHM_NAME[]  = L"Global\\JK_PQC_STATS";
constexpr wchar_t SHM_MUTEX[] = L"Global\\JK_PQC_STATS_MUTEX";
// -----------------------------------------------------------------------------
// [STRUCTS]
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct T_JKPQC_STATS
{
  uint32_t  m_uiVersion;              // 구조체 버전 (현재 1)
  uint32_t  m_uiCurrentConnections;   // 현재 접속자 수
  uint32_t  m_uiMaxConnections;       // 최대 동시 접속자 수
  uint64_t  m_ullTotalBytes4RX;       // 수신 데이터 총량 (bytes)
  uint64_t  m_ullTotalBytes4TX;       // 전송 데이터 총량 (bytes)
  uint64_t  m_ullTotalRequests;       // 총 요청 처리 수
  uint64_t  m_ullUptimeSeconds;       // 서버 가동 시간 (초)
  uint64_t  m_ullLastUpdated;         // 마지막 갱신 시각 (Unix timestamp)
};
#pragma pack(pop)
/*----------------------------------------------------------------------------+-

-+----------------------------------------------------------------------------+-
- 목적
  
- 주의 사항
  > 
-+----------------------------------------------------------------------------*/
class CShmStatsProducer
{
  // -------------------------------------
public:
  CShmStatsProducer();
  virtual ~CShmStatsProducer();

  bool Init();                          // 공유 메모리 + Mutex 생성
  void Shutdown();                      // 핸들 해제

  void OnConnect();                     // 접속 시 호출
  void OnDisconnect();                  // 접속 해제 시 호출
  void AddRxBytes(uint64_t a_ullBytes); // 수신 바이트 누적
  void AddTxBytes(uint64_t a_ullBytes); // 전송 바이트 누적
  void AddRequest();                    // 요청 처리 수 누적
  void Update();                        // uptime + last_updated 갱신 후 공유 메모리에 기록

  T_JKPQC_STATS GetSnapshot() const;    // 내부 카운터 스냅샷을 반환

  // -------------------------------------
protected:
  
  // -------------------------------------
private:
  void _writeStats(const T_JKPQC_STATS& a_rtStats); // Mutex 획득 후 공유 메모리에 기록

private:
  void* m_pMapFile;        // 파일맵 핸들 (HANDLE)
  void* m_pMutex;          // 뮤텍스 핸들 (HANDLE)
  T_JKPQC_STATS* m_ptStats; // 공유 메모리 뷰 포인터

  std::atomic<uint32_t> m_uiCurrentConnections;
  std::atomic<uint32_t> m_uiMaxConnections;
  std::atomic<uint64_t> m_ullTotalRxBytes;
  std::atomic<uint64_t> m_ullTotalTxBytes;
  std::atomic<uint64_t> m_ullTotalRequests;
  std::chrono::steady_clock::time_point m_tStartTime;
  bool m_bInitialized;

  // -------------------------------------
};
// -----------------------------------------------------------------------------
