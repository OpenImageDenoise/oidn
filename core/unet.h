// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "filter.h"
#include "network.h"
#include "color.h"

namespace oidn {

  // ---------------------------------------------------------------------------
  // UNetFilter: U-Net based denoising filter
  // ---------------------------------------------------------------------------

  class UNetFilter : public Filter
  {
  protected:
    // Network constants
    static constexpr int alignment       = 16;  // required spatial alignment in pixels (padding may be necessary)
    static constexpr int receptiveField  = 174; // receptive field in pixels
    static constexpr int overlap         = round_up(receptiveField / 2, alignment); // required spatial overlap between tiles in pixels

    // Estimated memory usage
    static constexpr int estimatedBytesBase     = 16*1024*1024; // conservative base memory usage
  #if defined(OIDN_DNNL)
    static constexpr int estimatedBytesPerPixel = 882;
  #else
    static constexpr int estimatedBytesPerPixel = 854;
  #endif

    // Images
    Image color;
    Image albedo;
    Image normal;
    Image output;
    Image outputTemp; // required for in-place tiled filtering

    // Options
    bool hdr = false;
    bool srgb = false;
    bool directional = false;
    float inputScale = std::numeric_limits<float>::quiet_NaN();
#if defined(OIDN_DNNL)
    int maxMemoryMB = 6000; // approximate maximum memory usage in MBs
#else
    int maxMemoryMB = 2000; // BNNS like memory reuse for best performance
#endif
    // Image dimensions
    int H = 0;            // image height
    int W = 0;            // image width
    int tileH = 0;        // tile height
    int tileW = 0;        // tile width
    int tileCountH = 1;   // number of tiles in H dimension
    int tileCountW = 1;   // number of tiles in W dimension
    bool inplace = false; // indicates whether input and output buffers overlap

    // Network
    Ref<Network> net;
    Ref<Node> inputReorder;
    Ref<Node> outputReorder;

    // Weights
    struct
    {
      Data hdr;
      Data hdr_alb;
      Data hdr_alb_nrm;
      Data ldr;
      Data ldr_alb;
      Data ldr_alb_nrm;
      Data dir;
    } builtinWeights;
    Data userWeights;

    explicit UNetFilter(const Ref<Device>& device);
    virtual Ref<TransferFunction> makeTransferFunc() = 0;

  public:
    void setData(const std::string& name, const Data& data) override;
    void set1f(const std::string& name, float value) override;
    float get1f(const std::string& name) override;

    void commit() override;
    void execute() override;

  private:
    void computeTileSize();
    Ref<Network> buildNet();
    bool isCommitted() const { return bool(net); }
  };

  // ---------------------------------------------------------------------------
  // RTFilter: Generic ray tracing denoiser
  // ---------------------------------------------------------------------------

  class RTFilter : public UNetFilter
  {
  public:
    explicit RTFilter(const Ref<Device>& device);

    void setImage(const std::string& name, const Image& data) override;
    void set1i(const std::string& name, int value) override;
    int get1i(const std::string& name) override;
  
  protected:
    Ref<TransferFunction> makeTransferFunc() override;
  };

  // ---------------------------------------------------------------------------
  // RTLightmapFilter: Ray traced lightmap denoiser
  // ---------------------------------------------------------------------------

  class RTLightmapFilter : public UNetFilter
  {
  public:
    explicit RTLightmapFilter(const Ref<Device>& device);

    void setImage(const std::string& name, const Image& data) override;
    void set1i(const std::string& name, int value) override;
    int get1i(const std::string& name) override;

  protected:
    Ref<TransferFunction> makeTransferFunc() override;
  };

} // namespace oidn
