/*----------------------------------------------------------------------------+-
shm_stats.cpp
-+----------------------------------------------------------------------------+-
Description : "jk-https-server 실시간 통계정보" 공유메모리 클래스 구현
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
-+----------------------------------------------------------------------------*/
#include "shm_stats.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

// -----------------------------------------------------------------------------
// CShmStatsProducer 클래스 구현
// -----------------------------------------------------------------------------
CShmStatsProducer::CShmStatsProducer()
  : m_pMapFile(NULL)
  , m_pMutex(NULL)
  , m_ptStats(NULL)
  , m_uiCurrentConnections(0)
  , m_uiMaxConnections(0)
  , m_ullTotalRxBytes(0)
  , m_ullTotalTxBytes(0)
  , m_ullTotalRequests(0)
  , m_tStartTime(std::chrono::steady_clock::now())
  , m_bInitialized(false)
{
}

CShmStatsProducer::~CShmStatsProducer()
{
  Shutdown();
}

bool CShmStatsProducer::Init()
{
#ifndef _WIN32
  fprintf(stderr, "[ERR_] CShmStatsProducer::Init 미지원 플랫폼\n");
  return false;
#else
  m_pMutex = CreateMutexW(NULL, FALSE, SHM_MUTEX);
  if (m_pMutex == NULL)
  {
    fprintf(stderr, "[ERR_] CreateMutexW 실패 (err=%lu)\n", GetLastError());
    return false;
  }
  m_pMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, static_cast<DWORD>(sizeof(T_JKPQC_STATS)), SHM_NAME);
  if (m_pMapFile == NULL)
  {
    fprintf(stderr, "[ERR_] CreateFileMappingW 실패 (err=%lu)\n", GetLastError());
    Shutdown();
    return false;
  }
  m_ptStats = static_cast<T_JKPQC_STATS*>(MapViewOfFile(static_cast<HANDLE>(m_pMapFile), FILE_MAP_ALL_ACCESS, 0, 0, sizeof(T_JKPQC_STATS)));
  if (m_ptStats == NULL)
  {
    fprintf(stderr, "[ERR_] MapViewOfFile 실패 (err=%lu)\n", GetLastError());
    Shutdown();
    return false;
  }
  m_tStartTime = std::chrono::steady_clock::now();
  m_bInitialized = true;
  Update();
  return true;
#endif
}

void CShmStatsProducer::Shutdown()
{
#ifdef _WIN32
  if (m_ptStats != NULL)
  {
    UnmapViewOfFile(m_ptStats);
    m_ptStats = NULL;
  }
  if (m_pMapFile != NULL)
  {
    CloseHandle(static_cast<HANDLE>(m_pMapFile));
    m_pMapFile = NULL;
  }
  if (m_pMutex != NULL)
  {
    CloseHandle(static_cast<HANDLE>(m_pMutex));
    m_pMutex = NULL;
  }
#endif
  m_bInitialized = false;
}

void CShmStatsProducer::OnConnect()
{
  const uint32_t l_uiCurrent = m_uiCurrentConnections.fetch_add(1) + 1;
  uint32_t l_uiExpected = m_uiMaxConnections.load();
  while ((l_uiCurrent > l_uiExpected) && !m_uiMaxConnections.compare_exchange_weak(l_uiExpected, l_uiCurrent))
  {
  }
}

void CShmStatsProducer::OnDisconnect()
{
  uint32_t l_uiCurrent = m_uiCurrentConnections.load();
  while (l_uiCurrent > 0)
  {
    if (m_uiCurrentConnections.compare_exchange_weak(l_uiCurrent, l_uiCurrent - 1))
    {
      break;
    }
  }
}

void CShmStatsProducer::AddRxBytes(uint64_t a_ullBytes)
{
  m_ullTotalRxBytes.fetch_add(a_ullBytes);
}

void CShmStatsProducer::AddTxBytes(uint64_t a_ullBytes)
{
  m_ullTotalTxBytes.fetch_add(a_ullBytes);
}

void CShmStatsProducer::AddRequest()
{
  m_ullTotalRequests.fetch_add(1);
}

void CShmStatsProducer::Update()
{
  T_JKPQC_STATS l_tSnapshot = GetSnapshot();
  _writeStats(l_tSnapshot);
}

T_JKPQC_STATS CShmStatsProducer::GetSnapshot() const
{
  T_JKPQC_STATS l_tSnapshot = {};
  l_tSnapshot.m_uiVersion = 1;
  l_tSnapshot.m_uiCurrentConnections = m_uiCurrentConnections.load();
  l_tSnapshot.m_uiMaxConnections = m_uiMaxConnections.load();
  l_tSnapshot.m_ullTotalBytes4RX = m_ullTotalRxBytes.load();
  l_tSnapshot.m_ullTotalBytes4TX = m_ullTotalTxBytes.load();
  l_tSnapshot.m_ullTotalRequests = m_ullTotalRequests.load();
  l_tSnapshot.m_ullUptimeSeconds = static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_tStartTime).count()
  );
  l_tSnapshot.m_ullLastUpdated = static_cast<uint64_t>(time(NULL));
  return l_tSnapshot;
}

void CShmStatsProducer::_writeStats(const T_JKPQC_STATS& a_rtStats)
{
#ifndef _WIN32
  (void)a_rtStats;
  return;
#else
  if ((!m_bInitialized) || (m_pMutex == NULL) || (m_ptStats == NULL))
  {
    return;
  }
  const DWORD l_dwWait = WaitForSingleObject(static_cast<HANDLE>(m_pMutex), 500);
  if ((l_dwWait != WAIT_OBJECT_0) && (l_dwWait != WAIT_ABANDONED))
  {
    fprintf(stderr, "[ERR_] WaitForSingleObject 실패/타임아웃 (err=%lu)\n", GetLastError());
    return;
  }
  memcpy(m_ptStats, &a_rtStats, sizeof(T_JKPQC_STATS));
  if (!ReleaseMutex(static_cast<HANDLE>(m_pMutex)))
  {
    fprintf(stderr, "[ERR_] ReleaseMutex 실패 (err=%lu)\n", GetLastError());
  }
#endif
}
// -----------------------------------------------------------------------------
