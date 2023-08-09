// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "core/thread.h"

#if defined(__clang__) && !defined(_LIBCPP_VERSION) && !defined(TBB_USE_GLIBCXX_VERSION)
  // TBB does not always detect the version of libstdc++ correctly when using
  // Clang, so we have to set it manually. This is required for some TBB
  // features (e.g. TBB_USE_CAPTURED_EXCEPTION=0).
  #if defined(_GLIBCXX_RELEASE)
    // This macro is available only in GCC 7.1 and later
    #define TBB_USE_GLIBCXX_VERSION (_GLIBCXX_RELEASE * 10000)
  #else
    // Try to detect some older GCC versions
    #if __has_include(<cuchar>)
      #define TBB_USE_GLIBCXX_VERSION 60000
    #elif __has_include(<codecvt>)
      #define TBB_USE_GLIBCXX_VERSION 50000
    #elif __has_include(<ext/cmath>)
      #define TBB_USE_GLIBCXX_VERSION 40800
    #endif
  #endif
#endif

#define TBB_USE_CAPTURED_EXCEPTION 0
#define TBB_PREVIEW_LOCAL_OBSERVER 1
#define TBB_PREVIEW_TASK_ARENA_CONSTRAINTS_EXTENSION 1

#include "tbb/task_scheduler_observer.h"
#include "tbb/task_arena.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_reduce.h"
#include "tbb/blocked_range.h"
#include "tbb/blocked_range2d.h"
#include "tbb/blocked_range3d.h"

OIDN_NAMESPACE_BEGIN

  // -----------------------------------------------------------------------------------------------
  // PinningObserver
  // -----------------------------------------------------------------------------------------------

  class PinningObserver : public tbb::task_scheduler_observer
  {
  public:
    explicit PinningObserver(const std::shared_ptr<ThreadAffinity>& affinity);
    PinningObserver(const std::shared_ptr<ThreadAffinity>& affinity, tbb::task_arena& arena);
    ~PinningObserver();

    void on_scheduler_entry(bool isWorker) override;
    void on_scheduler_exit(bool isWorker) override;

  private:
    std::shared_ptr<ThreadAffinity> affinity;
  };

  // -----------------------------------------------------------------------------------------------
  // parallel_nd
  // -----------------------------------------------------------------------------------------------

  template<typename T0, typename F>
  OIDN_INLINE void parallel_nd(const T0& D0, const F& f)
  {
    tbb::parallel_for(tbb::blocked_range<T0>(0, D0), [&](const tbb::blocked_range<T0>& r)
    {
      for (T0 i = r.begin(); i != r.end(); ++i)
        f(i);
    });
  }

  template<typename T0, typename T1, typename F>
  OIDN_INLINE void parallel_nd(const T0& D0, const T1& D1, const F& f)
  {
    tbb::parallel_for(tbb::blocked_range2d<T0, T1>(0, D0, 0, D1), [&](const tbb::blocked_range2d<T0, T1>& r)
    {
      for (T0 i = r.rows().begin(); i != r.rows().end(); ++i)
      {
        for (T1 j = r.cols().begin(); j != r.cols().end(); ++j)
          f(i, j);
      }
    });
  }

  template<typename T0, typename T1, typename T2, typename F>
  OIDN_INLINE void parallel_nd(const T0& D0, const T1& D1, const T2& D2, const F& f)
  {
    tbb::parallel_for(tbb::blocked_range3d<T0, T1, T2>(0, D0, 0, D1, 0, D2), [&](const tbb::blocked_range3d<T0, T1, T2>& r)
    {
      for (T0 i = r.pages().begin(); i != r.pages().end(); ++i)
      {
        for (T1 j = r.rows().begin(); j != r.rows().end(); ++j)
        {
          for (T2 k = r.cols().begin(); k != r.cols().end(); ++k)
            f(i, j, k);
        }
      }
    });
  }

OIDN_NAMESPACE_END
