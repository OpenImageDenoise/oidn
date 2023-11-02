// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "core/input_process.h"
#include "core/tensor_accessor.h"
#include "core/image_accessor.h"
#include "core/color.h"
#include "core/tile.h"

OIDN_NAMESPACE_BEGIN

  template<typename DstT, TensorLayout dstLayout, int dstPaddedC>
  struct GPUInputProcessKernel : WorkGroup<2>
  {
    // Source
    ImageAccessor input;  // color, albedo or normal
    ImageAccessor albedo; // auxiliary albedo
    ImageAccessor normal; // auxiliary normal

    // Destination
    TensorAccessor3D<DstT, dstLayout> dst;

    // Tile
    Tile tile;

    // Transfer function
    TransferFunction transferFunc;
    bool hdr;
    bool snorm; // signed normalized ([-1..1])

    OIDN_DEVICE_INLINE vec3f getInput(int h, int w) const
    {
      vec3f value = input.get3(h, w);

      // Scale
      value = value * transferFunc.getInputScale();

      // Sanitize
      value = math::clamp(math::nan_to_zero(value), snorm ? -1.f : 0.f, hdr ? FLT_MAX : 1.f);

      if (snorm)
      {
        // Transform to [0..1]
        value = value * 0.5f + 0.5f;
      }

      // Apply the transfer function
      value = transferFunc.forward(value);

      return value;
    }

    OIDN_DEVICE_INLINE vec3f getAlbedo(int h, int w) const
    {
      vec3f value = albedo.get3(h, w);

      // Sanitize
      value = math::clamp(math::nan_to_zero(value), 0.f, 1.f);

      return value;
    }

    OIDN_DEVICE_INLINE vec3f getNormal(int h, int w) const
    {
      vec3f value = normal.get3(h, w);

      // Sanitize
      value = math::clamp(math::nan_to_zero(value), -1.f, 1.f);

      // Transform to [0..1]
      value = value * 0.5f + 0.5f;

      return value;
    }

    OIDN_DEVICE_INLINE void operator ()(const WorkGroupItem<2>& it) const
    {
      const int hDst = it.getGlobalID<0>();
      const int wDst = it.getGlobalID<1>();

      const int h = hDst - tile.hDstBegin;
      const int w = wDst - tile.wDstBegin;

      // Gather and process the input channel values
      float values[dstPaddedC] = {}; // = 0

      if (h >= 0 && h < tile.H && w >= 0 && w < tile.W)
      {
        const int hSrc = h + tile.hSrcBegin;
        const int wSrc = w + tile.wSrcBegin;

        const vec3f inputValue = getInput(hSrc, wSrc);
        values[0] = inputValue.x;
        values[1] = inputValue.y;
        values[2] = inputValue.z;

        if (dstPaddedC >= 6 && albedo.ptr)
        {
          const vec3f albedoValue = getAlbedo(hSrc, wSrc);
          values[3] = albedoValue.x;
          values[4] = albedoValue.y;
          values[5] = albedoValue.z;

          if (dstPaddedC >= 9 && normal.ptr)
          {
            const vec3f normalValue = getNormal(hSrc, wSrc);
            values[6] = normalValue.x;
            values[7] = normalValue.y;
            values[8] = normalValue.z;
          }
        }
      }

    #if !defined(OIDN_COMPILE_HIP) || defined(__gfx1030__)
      // Transpose the values in the subgroup into coalesced blocks and store them to memory (fast)
      // All work-items in the subgroup are assumed to be in the same row
      const int subgroupLocalID = it.getSubgroupLocalID();
      const int wDstBegin = it.subgroupBroadcast(wDst, 0);
      GlobalPtr<DstT> dstPtr = &dst(0, hDst, wDstBegin);

      #if defined(OIDN_COMPILE_SYCL)
      // The subgroup size is assumed to be equal to the channel count
      constexpr int subgroupSize = dstPaddedC;

      #pragma unroll
      for (int i = 0; i < subgroupSize; ++i)
      {
        float dstBlock = 0;

        #pragma unroll
        for (int c = 0; c < min(dstPaddedC, 9); ++c) // only up to 9 non-zero channels
        {
          const auto value = it.subgroupBroadcast(values[c], i);
          dstBlock = (subgroupLocalID == c) ? value : dstBlock;
        }

        if (wDstBegin + i < dst.W)
          it.subgroupStore(dstPtr + i * dstPaddedC, DstT(dstBlock));
      }
      #else
      // The subgroup size is assumed to be divisible by the channel count
      const int subgroupSize = it.getSubgroupSize();

      for (int i = 0; i < subgroupSize; i += subgroupSize / dstPaddedC)
      {
        // We may store multiple pixels in the same block
        const int wBlock = i + subgroupLocalID / dstPaddedC;
        float dstBlock = 0;

        #pragma unroll
        for (int c = 0; c < min(dstPaddedC, 9); ++c) // only up to 9 non-zero channels
        {
          const auto value = it.subgroupShuffle(values[c], wBlock);
          dstBlock = (subgroupLocalID % dstPaddedC) == c ? value : dstBlock;
        }

        if (wDstBegin + wBlock < dst.W)
          dstPtr[i * dstPaddedC + subgroupLocalID] = DstT(dstBlock);
      }
      #endif
    #else
      // Scatter the values to memory (slow on most architectures)
      if (wDst < dst.W)
      {
        #pragma unroll
        for (int c = 0; c < dstPaddedC; ++c)
          dst(c, hDst, wDst) = values[c];
      }
    #endif
    }
  };

  template<typename EngineT, typename DstT, TensorLayout dstLayout, int tensorBlockC>
  class GPUInputProcess : public InputProcess
  {
  public:
    GPUInputProcess(const Ref<EngineT>& engine, const InputProcessDesc& desc)
      : InputProcess(engine, desc),
        engine(engine) {}

    void submit() override
    {
      if (!getMainSrc() || !dst)
        throw std::logic_error("input processing source/destination not set");
      if (tile.hSrcBegin + tile.H > getMainSrc()->getH() ||
          tile.wSrcBegin + tile.W > getMainSrc()->getW() ||
          tile.hDstBegin + tile.H > dst->getH() ||
          tile.wDstBegin + tile.W > dst->getW())
        throw std::out_of_range("input processing source/destination out of range");

      switch (dst->getC())
      {
      case  3: runImpl<3>(); break;
      case  6: runImpl<6>(); break;
      case  9: runImpl<9>(); break;
      default: throw std::logic_error("unsupported input processing source channel count");
      }
    }

  private:
    template<int dstC>
    void runImpl()
    {
      constexpr int dstPaddedC = round_up(dstC, tensorBlockC);
      if (dst->getPaddedC() != dstPaddedC)
        throw std::logic_error("unexpected input processing destination channel count");

    #if defined(OIDN_COMPILE_SYCL)
      // We request the subgroup size at compile time
      constexpr int subgroupSize = dstPaddedC;
    #else
      // We know the subgroup size only at runtime
      const int subgroupSize = engine->getSubgroupSize();
      if (subgroupSize % dstPaddedC != 0)
        throw std::logic_error("unsupported input processing destination channel count");
    #endif

      using Kernel = GPUInputProcessKernel<DstT, dstLayout, dstPaddedC>;

      Kernel kernel;
      Image nullImage;

      kernel.input  = color ? *color : (albedo ? *albedo : *normal);
      kernel.albedo = (color && albedo) ? *albedo : nullImage;
      kernel.normal = (color && normal) ? *normal : nullImage;
      kernel.dst    = *dst;
      kernel.tile   = tile;
      kernel.transferFunc = *transferFunc;
      kernel.hdr   = hdr;
      kernel.snorm = snorm;

      const WorkDim<2> numGroups{dst->getH(), ceil_div(dst->getW(), subgroupSize)};
      const WorkDim<2> groupSize{1, subgroupSize};

    #if defined(OIDN_COMPILE_SYCL)
      engine->template submitKernel<subgroupSize>(numGroups, groupSize, kernel);
    #else
      engine->submitKernel(numGroups, groupSize, kernel);
    #endif
    }

    Ref<EngineT> engine;
  };

OIDN_NAMESPACE_END
