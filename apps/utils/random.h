// Copyright 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/platform.h"

OIDN_NAMESPACE_BEGIN

  // Simple and very fast LCG random number generator
  class Random
  {
  private:
    uint32_t state;

  public:
    oidn_inline Random(uint32_t seed = 1) : state(seed) {}

    oidn_inline void reset(uint32_t seed = 1)
    {
      state = (seed * 8191) ^ 140167;
    }

    oidn_inline void next()
    {
      const uint32_t multiplier = 1664525;
      const uint32_t increment  = 1013904223;
      state = multiplier * state + increment;
    }

    oidn_inline uint32_t getUInt()
    {
      next();
      return state;
    }

    oidn_inline int getInt()
    {
      next();
      return state;
    }

    oidn_inline float getFloat()
    {
      next();
      return float(state) * 2.3283064365386962890625e-10f; // x / 2^32
    }
  };

OIDN_NAMESPACE_END