#pragma once

#include <iostream>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

namespace Hermes {
namespace Utils {

inline void PinThreadToCore(int coreId) {
#ifdef _WIN32
  HANDLE thread = GetCurrentThread();
  DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << coreId);
  if (SetThreadAffinityMask(thread, mask) == 0) {
    std::cerr << "[Hermes] Failed to set thread affinity to Core " << coreId
              << "\n";
  } else {
    std::cout << "[Hermes] Thread pinned to Core " << coreId << "\n";
  }
#else
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(coreId, &cpuset);

  pthread_t thread = pthread_self();
  if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
    std::cerr << "[Hermes] Failed to set thread affinity to Core " << coreId
              << "\n";
  } else {
    std::cout << "[Hermes] Thread pinned to Core " << coreId << "\n";
  }
#endif
}

} // namespace Utils
} // namespace Hermes
