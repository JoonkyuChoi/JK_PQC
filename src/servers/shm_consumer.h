/*----------------------------------------------------------------------------+-
shm_consumer.h
-+----------------------------------------------------------------------------+-
Description : jk-https-dashboard 공유메모리 통계 리더 클래스 정의
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
-+----------------------------------------------------------------------------*/
#pragma once

#include "..\common\shm_stats.h"

// -----------------------------------------------------------------------------
// CShmStatsConsumer : jk-https-server / jk-pqc-server 공유메모리 통계 Consumer
// -----------------------------------------------------------------------------
class CShmStatsConsumer
{
  // -------------------------------------
public:
  CShmStatsConsumer();          // 생성자 - 핸들·포인터 초기화
  virtual ~CShmStatsConsumer(); // 소멸자 - Shutdown() 호출

  bool Init();                              // Global\\JK_PQC_STATS 공유 메모리·Mutex 오픈
  void Shutdown();                          // 파일맵·뮤텍스·뷰 핸들 해제
  bool Read(T_JKPQC_STATS& a_rtOut);       // Mutex 획득 후 최신 통계 스냅샷 읽기
  // -------------------------------------
protected:
  // -------------------------------------
private:
  void*                m_pMapFile;      // CreateFileMapping으로 연 파일맵 핸들 (HANDLE)
  void*                m_pMutex;        // SHM_MUTEX 이름의 뮤텍스 핸들 (HANDLE)
  const T_JKPQC_STATS* m_ptStats;         // MapViewOfFile로 매핑된 통계 구조체 읽기 전용 포인터
  bool                 m_bInitialized;  // Init() 성공 여부
  // -------------------------------------
};
// -----------------------------------------------------------------------------
