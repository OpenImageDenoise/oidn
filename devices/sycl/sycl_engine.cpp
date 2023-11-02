// Copyright 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "sycl_engine.h"
#include "sycl_conv.h"
#include "sycl_external_buffer.h"
#include "../gpu/gpu_autoexposure.h"
#include "../gpu/gpu_input_process.h"
#include "../gpu/gpu_output_process.h"
#include "../gpu/gpu_image_copy.h"

OIDN_NAMESPACE_BEGIN

  SYCLEngine::SYCLEngine(const Ref<SYCLDevice>& device,
                         const sycl::queue& syclQueue)
    : device(device.get()),
      syclQueue(syclQueue)
  {
    auto syclDevice = syclQueue.get_device();

    if (syclDevice.get_platform().get_backend() == sycl::backend::ext_oneapi_level_zero)
      zeDevice = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(syclDevice);

    maxWorkGroupSize = syclDevice.get_info<sycl::info::device::max_work_group_size>();
  }

  Ref<Buffer> SYCLEngine::newExternalBuffer(ExternalMemoryTypeFlag fdType,
                                            int fd, size_t byteSize)
  {
    return makeRef<SYCLExternalBuffer>(this, fdType, fd, byteSize);
  }

  Ref<Buffer> SYCLEngine::newExternalBuffer(ExternalMemoryTypeFlag handleType,
                                            void* handle, const void* name, size_t byteSize)
  {
    return makeRef<SYCLExternalBuffer>(this, handleType, handle, name, byteSize);
  }

  bool SYCLEngine::isConvSupported(PostOp postOp)
  {
    return postOp == PostOp::None ||
           postOp == PostOp::Pool ||
           postOp == PostOp::Upsample;
  }

  std::shared_ptr<Conv> SYCLEngine::newConv(const ConvDesc& desc)
  {
    switch (device->getArch())
    {
    case SYCLArch::XeLP:
      return xelp::newSYCLConv(this, desc);
    case SYCLArch::XeHPG:
      return xehpg::newSYCLConv(this, desc);
  #if defined(__linux__)
    case SYCLArch::XeHPC:
      return xehpc::newSYCLConv(this, desc);
  #endif
    default:
      throw std::logic_error("unsupported architecture");
    }
  }

  std::shared_ptr<Pool> SYCLEngine::newPool(const PoolDesc& desc)
  {
    throw std::logic_error("operation not implemented");
  }

  std::shared_ptr<Upsample> SYCLEngine::newUpsample(const UpsampleDesc& desc)
  {
    throw std::logic_error("operation not implemented");
  }

  std::shared_ptr<Autoexposure> SYCLEngine::newAutoexposure(const ImageDesc& srcDesc)
  {
    if (maxWorkGroupSize >= 1024)
      return std::make_shared<GPUAutoexposure<SYCLEngine, 1024>>(this, srcDesc);
    else if (maxWorkGroupSize >= 512)
      return std::make_shared<GPUAutoexposure<SYCLEngine, 512>>(this, srcDesc);
    else
      return std::make_shared<GPUAutoexposure<SYCLEngine, 256>>(this, srcDesc);
  }

  std::shared_ptr<InputProcess> SYCLEngine::newInputProcess(const InputProcessDesc& desc)
  {
    return std::make_shared<GPUInputProcess<SYCLEngine, half, TensorLayout::Chw16c, 16>>(this, desc);
  }

  std::shared_ptr<OutputProcess> SYCLEngine::newOutputProcess(const OutputProcessDesc& desc)
  {
    return std::make_shared<GPUOutputProcess<SYCLEngine, half, TensorLayout::Chw16c>>(this, desc);
  }

  std::shared_ptr<ImageCopy> SYCLEngine::newImageCopy()
  {
    return std::make_shared<GPUImageCopy<SYCLEngine>>(this);
  }

  void* SYCLEngine::usmAlloc(size_t byteSize, Storage storage)
  {
    if (byteSize == 0)
      return nullptr;

    void* ptr = nullptr;

    switch (storage)
    {
    case Storage::Host:
      ptr = sycl::aligned_alloc_host(memoryAlignment,
                                     byteSize,
                                     syclQueue.get_context());
      break;

    case Storage::Device:
      ptr = sycl::aligned_alloc_device(memoryAlignment,
                                       byteSize,
                                       syclQueue.get_device(),
                                       syclQueue.get_context());
      break;

    case Storage::Managed:
      ptr = sycl::aligned_alloc_shared(memoryAlignment,
                                       byteSize,
                                       syclQueue.get_device(),
                                       syclQueue.get_context());
      break;

    default:
      throw Exception(Error::InvalidArgument, "invalid storage mode");
    }

    if (ptr == nullptr && byteSize > 0)
      throw std::bad_alloc();

    return ptr;
  }

  void SYCLEngine::usmFree(void* ptr, Storage storage)
  {
    if (ptr != nullptr)
      sycl::free(ptr, syclQueue.get_context());
  }

  void SYCLEngine::usmCopy(void* dstPtr, const void* srcPtr, size_t byteSize)
  {
    submitUSMCopy(dstPtr, srcPtr, byteSize);
    wait();
  }

  void SYCLEngine::submitUSMCopy(void* dstPtr, const void* srcPtr, size_t byteSize)
  {
    lastEvent = syclQueue.memcpy(dstPtr, srcPtr, byteSize, getDepEvents());
  }

  void SYCLEngine::submitHostFunc(std::function<void()>&& f)
  {
    lastEvent = syclQueue.submit([&](sycl::handler& cgh) {
      cgh.depends_on(getDepEvents()),
      cgh.host_task(f);
    });
  }

  void SYCLEngine::submitBarrier()
  {
    lastEvent = syclQueue.submit([&](sycl::handler& cgh) {
      cgh.depends_on(getDepEvents()),
      //cgh.ext_oneapi_barrier(); // FIXME: hangs, workaround: SYCL_PI_LEVEL_ZERO_USE_MULTIPLE_COMMANDLIST_BARRIERS=0
      cgh.single_task([](){});    // FIXME: should switch to ext_oneapi_barrier when it gets fixed
    });
  }

  void SYCLEngine::wait()
  {
    if (lastEvent)
    {
      lastEvent.value().wait_and_throw();
      lastEvent.reset();
    }
  }

OIDN_NAMESPACE_END
