/*----------------------------------------------------------------------------+-
shm_consumer.cpp
-+----------------------------------------------------------------------------+-
Description : jk-https-dashboard 공유메모리 통계 리더 클래스 구현
Copyright   : 2026~ by Joonkyu Choi, All rights reserved.

변경 이력   :
  [2026/04/28] 최초 작성
-+----------------------------------------------------------------------------*/
#include "shm_consumer.h"

#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

// -----------------------------------------------------------------------------
// CShmStatsConsumer 클래스 구현
// -----------------------------------------------------------------------------
CShmStatsConsumer::CShmStatsConsumer()
  : m_pMapFile(NULL)
  , m_pMutex(NULL)
  , m_ptStats(NULL)
  , m_bInitialized(false)
{
}

CShmStatsConsumer::~CShmStatsConsumer()
{
  Shutdown();
}

bool CShmStatsConsumer::Init()
{
#ifndef _WIN32
  fprintf(stderr, "[ERR_] CShmStatsConsumer::Init 미지원 플랫폼\n");
  return false;
#else
  m_pMutex = OpenMutexW(SYNCHRONIZE, FALSE, SHM_MUTEX);
  if (m_pMutex == NULL)
  {
    fprintf(stderr, "[ERR_] OpenMutexW 실패 (err=%lu)\n", GetLastError());
    return false;
  }
  m_pMapFile = OpenFileMappingW(FILE_MAP_READ, FALSE, SHM_NAME);
  if (m_pMapFile == NULL)
  {
    fprintf(stderr, "[ERR_] OpenFileMappingW 실패 (err=%lu)\n", GetLastError());
    Shutdown();
    return false;
  }
  m_ptStats = static_cast<const T_JKPQC_STATS*>(MapViewOfFile(static_cast<HANDLE>(m_pMapFile), FILE_MAP_READ, 0, 0, sizeof(T_JKPQC_STATS)));
  if (m_ptStats == NULL)
  {
    fprintf(stderr, "[ERR_] MapViewOfFile 실패 (err=%lu)\n", GetLastError());
    Shutdown();
    return false;
  }
  m_bInitialized = true;
  return true;
#endif
}

void CShmStatsConsumer::Shutdown()
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

bool CShmStatsConsumer::Read(T_JKPQC_STATS& a_rtOut)
{
#ifndef _WIN32
  (void)a_rtOut;
  return false;
#else
  if ((!m_bInitialized) || (m_pMutex == NULL) || (m_ptStats == NULL))
  {
    return false;
  }
  const DWORD l_dwWait = WaitForSingleObject(static_cast<HANDLE>(m_pMutex), 500);
  if ((l_dwWait != WAIT_OBJECT_0) && (l_dwWait != WAIT_ABANDONED))
  {
    fprintf(stderr, "[ERR_] WaitForSingleObject 실패/타임아웃 (err=%lu)\n", GetLastError());
    return false;
  }
  memcpy(&a_rtOut, m_ptStats, sizeof(T_JKPQC_STATS));
  if (!ReleaseMutex(static_cast<HANDLE>(m_pMutex)))
  {
    fprintf(stderr, "[ERR_] ReleaseMutex 실패 (err=%lu)\n", GetLastError());
    return false;
  }
  return (a_rtOut.m_uiVersion == 1);
#endif
}
// -----------------------------------------------------------------------------
