// Copyright 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "common/common.h"
#include "utils/image_buffer.h"
#include "utils/random.h"
#include <cassert>
#include <cmath>
#include <limits>

#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_FAST_COMPILE

// The following are required for the correct functioning of USM!
#define CATCH_CONFIG_NO_POSIX_SIGNALS
#define CATCH_CONFIG_NO_WINDOWS_SEH

#include "catch.hpp"

OIDN_NAMESPACE_USING

DeviceType deviceType = DeviceType::Default;
PhysicalDeviceRef physicalDevice;

DeviceRef makeDevice()
{
  DeviceRef device = physicalDevice ? physicalDevice.newDevice() : newDevice(deviceType);
  REQUIRE(bool(device));
  REQUIRE(getError() == Error::None);
  return device;
}

DeviceRef makeAndCommitDevice()
{
  DeviceRef device = makeDevice();
  device.commit();
  REQUIRE(device.getError() == Error::None);
  return device;
}

// -------------------------------------------------------------------------------------------------

TEST_CASE("physical device", "[physical_device]")
{
  const int numDevices = getNumPhysicalDevices();
  REQUIRE(getError() == Error::None);
  REQUIRE(numDevices >= 0);

  SECTION("enumeration")
  {
    for (int i = 0; i < numDevices; ++i)
    {
      PhysicalDeviceRef physicalDevice(i);

      const DeviceType type = physicalDevice.get<DeviceType>("type");
      REQUIRE(getError() == Error::None);
      REQUIRE(type != DeviceType::Default);

      // Try invalid parameter name
      physicalDevice.get<int>("qwertyuiop");
      REQUIRE(getError() == Error::InvalidArgument);
      physicalDevice.get<int>(nullptr);
      REQUIRE(getError() == Error::InvalidArgument);

      const std::string name = physicalDevice.get<std::string>("name");
      REQUIRE(getError() == Error::None);
      REQUIRE(!name.empty());

      const bool uuidSupported = physicalDevice.get<bool>("uuidSupported");
      REQUIRE(getError() == Error::None);
      physicalDevice.get<OIDN_NAMESPACE::UUID>("uuid");
      if (uuidSupported)
        REQUIRE(getError() == Error::None);
      else
        REQUIRE(getError() == Error::InvalidArgument);

      const bool luidSupported = physicalDevice.get<bool>("luidSupported");
      REQUIRE(getError() == Error::None);

      physicalDevice.get<OIDN_NAMESPACE::LUID>("luid");
      if (luidSupported)
        REQUIRE(getError() == Error::None);
      else
        REQUIRE(getError() == Error::InvalidArgument);

      const uint32_t nodeMask = physicalDevice.get<uint32_t>("nodeMask");
      if (luidSupported)
      {
        REQUIRE(getError() == Error::None);
        REQUIRE(nodeMask != 0);
      }
      else
        REQUIRE(getError() == Error::InvalidArgument);

      const bool pciAddressSupported = physicalDevice.get<bool>("pciAddressSupported");
      REQUIRE(getError() == Error::None);
      physicalDevice.get<int>("pciDomain");
      physicalDevice.get<int>("pciBus");
      physicalDevice.get<int>("pciDevice");
      physicalDevice.get<int>("pciFunction");
      if (pciAddressSupported)
        REQUIRE(getError() == Error::None);
      else
        REQUIRE(getError() == Error::InvalidArgument);

      DeviceRef device = physicalDevice.newDevice();
      REQUIRE(device.getError() == Error::None);
      device.commit();
      REQUIRE(device.getError() == Error::None);
    }
  }

  SECTION("invalid ID")
  {
    PhysicalDeviceRef physicalDevice(-1);
    physicalDevice.get<DeviceType>("type");
    REQUIRE(getError() == Error::InvalidArgument);

    physicalDevice = PhysicalDeviceRef(numDevices);
    physicalDevice.get<DeviceType>("type");
    REQUIRE(getError() == Error::InvalidArgument);
  }
}

// -------------------------------------------------------------------------------------------------

TEST_CASE("buffer creation", "[buffer]")
{
  DeviceRef device = makeAndCommitDevice();

  SECTION("default buffer")
  {
    BufferRef buffer = device.newBuffer(1234567);
    REQUIRE(device.getError() == Error::None);
  }

  SECTION("device buffer")
  {
    BufferRef buffer = device.newBuffer(1234567, Storage::Device);
    REQUIRE(device.getError() == Error::None);
  }

  SECTION("managed buffer")
  {
    const bool managedMemorySupported = device.get<bool>("managedMemorySupported");
    REQUIRE(device.getError() == Error::None);

    if (managedMemorySupported)
    {
      BufferRef buffer = device.newBuffer(1234567, Storage::Managed);
      REQUIRE(device.getError() == Error::None);
    }
  }

  SECTION("zero-sized default buffer")
  {
    BufferRef buffer = device.newBuffer(0);
    REQUIRE(device.getError() == Error::None);
  }

  SECTION("zero-sized device buffer")
  {
    BufferRef buffer = device.newBuffer(0, Storage::Device);
    REQUIRE(device.getError() == Error::None);
  }

  SECTION("out-of-memory default buffer")
  {
    BufferRef buffer = device.newBuffer(INTPTR_MAX);
    REQUIRE(device.getError() == Error::OutOfMemory);
  }

  SECTION("out-of-memory device buffer")
  {
    BufferRef buffer = device.newBuffer(INTPTR_MAX, Storage::Device);
    REQUIRE(device.getError() == Error::OutOfMemory);
  }

  SECTION("invalid buffer storage")
  {
    BufferRef buffer = device.newBuffer(10000, static_cast<Storage>(-42));
    REQUIRE(device.getError() == Error::InvalidArgument);
  }
}

TEST_CASE("buffer read/write", "[buffer_rw]")
{
  DeviceRef device = makeAndCommitDevice();

  const int N = 128*1024;
  const size_t bufferSize = N * sizeof(int);
  BufferRef buffer = device.newBuffer(bufferSize, Storage::Device);
  REQUIRE(device.getError() == Error::None);

  std::vector<int> src(N);
  std::vector<int> dst(N);
  for (int i = 0; i < N; ++i)
  {
    src[i] = i+1;
    dst[i] = 0;
  }

  SECTION("sync")
  {
    // Try writing to the buffer from nullptr
    buffer.write(0, bufferSize, nullptr);
    REQUIRE(device.getError() == Error::InvalidArgument);
    // Try writing with buffer overflow
    buffer.write(0, bufferSize+256, src.data());
    REQUIRE(device.getError() == Error::InvalidArgument);
    buffer.write(256, bufferSize, src.data());
    REQUIRE(device.getError() == Error::InvalidArgument);

    // Write to the buffer
    buffer.write(0, bufferSize, src.data());
    REQUIRE(device.getError() == Error::None);

    // Try reading from the buffer to nullptr
    buffer.read(0, bufferSize, nullptr);
    REQUIRE(device.getError() == Error::InvalidArgument);
    // Try reading with buffer overflow
    buffer.read(0, bufferSize+256, dst.data());
    REQUIRE(device.getError() == Error::InvalidArgument);
    buffer.read(256, bufferSize, dst.data());
    REQUIRE(device.getError() == Error::InvalidArgument);

    // Read from the buffer
    buffer.read(0, bufferSize, dst.data());
    REQUIRE(device.getError() == Error::None);

    // Verify the data
    REQUIRE(memcmp(src.data(), dst.data(), bufferSize) == 0);
  }

  SECTION("async")
  {
    // Try writing to the buffer from nullptr
    buffer.writeAsync(0, bufferSize, nullptr);
    REQUIRE(device.getError() == Error::InvalidArgument);
    // Try writing with buffer overflow
    buffer.writeAsync(0, bufferSize+256, src.data());
    REQUIRE(device.getError() == Error::InvalidArgument);
    buffer.writeAsync(256, bufferSize, src.data());
    REQUIRE(device.getError() == Error::InvalidArgument);

    // Write to the buffer
    buffer.writeAsync(0, bufferSize, src.data());
    REQUIRE(device.getError() == Error::None);

    // Try reading from the buffer to nullptr
    buffer.readAsync(0, bufferSize, nullptr);
    REQUIRE(device.getError() == Error::InvalidArgument);
    // Try reading with buffer overflow
    buffer.readAsync(0, bufferSize+256, dst.data());
    REQUIRE(device.getError() == Error::InvalidArgument);
    buffer.readAsync(256, bufferSize, dst.data());
    REQUIRE(device.getError() == Error::InvalidArgument);

    // Read from the buffer
    buffer.readAsync(0, bufferSize, dst.data());
    REQUIRE(device.getError() == Error::None);

    device.sync();
    REQUIRE(device.getError() == Error::None);

    // Verify the data
    REQUIRE(memcmp(src.data(), dst.data(), bufferSize) == 0);
  }
}

// -------------------------------------------------------------------------------------------------

#if defined(OIDN_FILTER_RT)

void setFilterImage(FilterRef& filter, const char* name, const std::shared_ptr<ImageBuffer>& image,
                    bool useBuffer = true)
{
  if (useBuffer)
    filter.setImage(name, image->getBuffer(), image->getFormat(), image->getW(), image->getH());
  else
    filter.setImage(name, image->getData(), image->getFormat(), image->getW(), image->getH());
}

std::shared_ptr<ImageBuffer> makeImage(DeviceRef& device, int W, int H, int C = 3,
                                       DataType dataType = DataType::Float32)
{
  return std::make_shared<ImageBuffer>(device, W, H, C, dataType);
}

std::shared_ptr<ImageBuffer> makeConstImage(DeviceRef& device, int W, int H, int C = 3,
                                            DataType dataType = DataType::Float32,
                                            float value = 0.5f)
{
  auto image = std::make_shared<ImageBuffer>(device, W, H, C, dataType);
  for (size_t i = 0; i < image->getSize(); ++i)
    image->set(i, value);
  return image;
}

bool isBetween(const std::shared_ptr<ImageBuffer>& image, float a, float b)
{
  for (size_t i = 0; i < image->getSize(); ++i)
  {
    const float x = image->get(i);
    if (!std::isfinite(x) || x < a || x > b)
      return false;
  }
  return true;
}

// -------------------------------------------------------------------------------------------------

TEST_CASE("single filter", "[single_filter]")
{
  const char* filterType = "RT";
  const int W = 257;
  const int H = 89;

  // Try creating a filter from a nullptr device
  DeviceRef nullDevice;
  FilterRef nullFilter = nullDevice.newFilter(filterType);
  REQUIRE(!nullFilter);
  REQUIRE(getError() == Error::InvalidArgument);

  // Create the device
  DeviceRef device = makeDevice();

  // Try creating a filter without committing the device
  FilterRef filter = device.newFilter(filterType);
  REQUIRE(!filter);
  REQUIRE(device.getError() == Error::InvalidOperation);

  // Commit the device
  device.commit();
  REQUIRE(device.getError() == Error::None);

  // Try creating an invalid filter type
  filter = device.newFilter("*");
  REQUIRE(!filter);
  REQUIRE(device.getError() == Error::InvalidArgument);
  filter = device.newFilter(nullptr);
  REQUIRE(!filter);
  REQUIRE(device.getError() == Error::InvalidArgument);

  // Create the filter
  filter = device.newFilter(filterType);
  REQUIRE(bool(filter));
  REQUIRE(device.getError() == Error::None);

  // Try executing the filter without committing it
  filter.execute();
  REQUIRE(device.getError() == Error::InvalidOperation);

  // Try committing the filter without setting any images
  filter.commit();
  REQUIRE(device.getError() == Error::InvalidOperation);

  // Set the output image
  auto image = makeConstImage(device, W, H);
  setFilterImage(filter, "output", image);
  REQUIRE(device.getError() == Error::None);

  // Try committing without setting all required images
  filter.commit();
  REQUIRE(device.getError() == Error::InvalidOperation);

  // Try setting an image with nullptr buffer
  auto nullImage = std::make_shared<ImageBuffer>();
  setFilterImage(filter, "color", nullImage);
  REQUIRE(device.getError() == Error::InvalidArgument);

  // Try setting an image with invalid name
  setFilterImage(filter, nullptr, image);
  REQUIRE(device.getError() == Error::InvalidArgument);

  // Try setting an image with invalid format
  filter.setImage("color", image->getBuffer(), static_cast<Format>(-1), W, H);
  REQUIRE(device.getError() == Error::InvalidArgument);

  // Try setting an image with buffer overflow
  filter.setImage("color", image->getBuffer(), image->getFormat(), W+1, H);
  REQUIRE(device.getError() == Error::InvalidArgument);
  filter.setImage("color", image->getBuffer(), image->getFormat(), W, H, 1);
  REQUIRE(device.getError() == Error::InvalidArgument);
  filter.setImage("color", image->getBuffer(), image->getFormat(), W, H, 0, 100);
  REQUIRE(device.getError() == Error::InvalidArgument);
  filter.setImage("color", image->getBuffer(), image->getFormat(), W, H, 0, 0, W*100);
  REQUIRE(device.getError() == Error::InvalidArgument);

  // Set the input image
  setFilterImage(filter, "color", image);
  REQUIRE(device.getError() == Error::None);

  // Commit the filter
  filter.commit();
  REQUIRE(device.getError() == Error::None);

  SECTION("single filter: 1 frame")
  {
    filter.execute();
    REQUIRE(device.getError() == Error::None);
  }

  SECTION("single filter: 3 frames")
  {
    for (int i = 0; i < 3; ++i)
    {
      filter.execute();
      REQUIRE(device.getError() == Error::None);
    }
  }

  // Release the device manually to test destroying it when some other object that holds the last
  // reference to it gets destroyed
  device.release();
}

// -------------------------------------------------------------------------------------------------

void multiFilter1PerDeviceTest(DeviceRef& device, const std::vector<int>& sizes)
{
  for (size_t i = 0; i < sizes.size(); ++i)
  {
    FilterRef filter = device.newFilter("RT");
    REQUIRE(bool(filter));

    auto image = makeConstImage(device, sizes[i], sizes[i]);
    setFilterImage(filter, "color",  image);
    setFilterImage(filter, "output", image);

    filter.commit();
    REQUIRE(device.getError() == Error::None);

    filter.execute();
    REQUIRE(device.getError() == Error::None);
  }
}

void multiFilterNPerDeviceTest(DeviceRef& device, const std::vector<int>& sizes)
{
  std::vector<FilterRef> filters;
  std::vector<std::shared_ptr<ImageBuffer>> images;

  for (size_t i = 0; i < sizes.size(); ++i)
  {
    filters.push_back(device.newFilter("RT"));
    REQUIRE(bool(filters[i]));

    images.push_back(makeConstImage(device, sizes[i], sizes[i]));
    setFilterImage(filters[i], "color",  images[i]);
    setFilterImage(filters[i], "output", images[i]);

    filters[i].commit();
    REQUIRE(device.getError() == Error::None);
  }

  for (size_t i = 0; i < filters.size(); ++i)
  {
    filters[i].execute();
    REQUIRE(device.getError() == Error::None);
  }
}

TEST_CASE("multiple filters", "[multi_filter]")
{
  DeviceRef device = makeAndCommitDevice();

  SECTION("1 filter / device: small -> large -> medium")
  {
    multiFilter1PerDeviceTest(device, {257, 3001, 1024});
  }

  SECTION("3 filters / device: small")
  {
    multiFilterNPerDeviceTest(device, {256, 256, 256});
  }

  SECTION("2 filters / device: small -> large")
  {
    multiFilterNPerDeviceTest(device, {256, 3001});
  }

  SECTION("3 filters / device: large -> small -> medium")
  {
    multiFilterNPerDeviceTest(device, {3001, 257, 1024});
  }
}

// -------------------------------------------------------------------------------------------------

TEST_CASE("multiple devices", "[multi_device]")
{
  const std::vector<int> sizes = {111, 256, 80};

  std::vector<DeviceRef> devices;
  std::vector<FilterRef> filters;
  std::vector<std::shared_ptr<ImageBuffer>> images;

  for (size_t i = 0; i < sizes.size(); ++i)
  {
    devices.push_back(makeAndCommitDevice());

    filters.push_back(devices[i].newFilter("RT"));
    REQUIRE(bool(filters[i]));

    images.push_back(makeConstImage(devices[i], sizes[i], sizes[i]));
    setFilterImage(filters[i], "color",  images[i]);
    setFilterImage(filters[i], "output", images[i]);

    filters[i].commit();
    REQUIRE(devices[i].getError() == Error::None);
  }

  for (size_t i = 0; i < devices.size(); ++i)
  {
    filters[i].execute();
    REQUIRE(devices[i].getError() == Error::None);
  }
}

// -------------------------------------------------------------------------------------------------

TEST_CASE("shared image", "[shared_image]")
{
  const int W = 198;
  const int H = 300;

  DeviceRef device = makeAndCommitDevice();

  SECTION("buffer allocator")
  {
    FilterRef filter = device.newFilter("RT");
    REQUIRE(bool(filter));

    auto color  = makeConstImage(device, W, H);
    auto output = makeImage(device, W, H);
    setFilterImage(filter, "color",  color,  false);
    setFilterImage(filter, "output", output, false);

    filter.commit();
    REQUIRE(device.getError() == Error::None);

    filter.execute();
    REQUIRE(device.getError() == Error::None);
  }

  SECTION("system allocator")
  {
    FilterRef filter = device.newFilter("RT");
    REQUIRE(bool(filter));

    const bool systemMemorySupported = device.get<bool>("systemMemorySupported");
    REQUIRE(device.getError() == Error::None);

    std::vector<int16_t> color (W * H * 3, 0);
    std::vector<int16_t> output(W * H * 3);

    filter.setImage("color",  color.data(),  Format::Half3, W, H);
    filter.setImage("output", output.data(), Format::Half3, W, H);

    if (systemMemorySupported)
    {
      REQUIRE(device.getError() == Error::None);
      filter.commit();
      REQUIRE(device.getError() == Error::None);
      filter.execute();
      REQUIRE(device.getError() == Error::None);
    }
    else
    {
      REQUIRE(device.getError() == Error::InvalidArgument);
    }
  }
}

// -------------------------------------------------------------------------------------------------

TEST_CASE("filter update", "[filter_update]")
{
  const int W = 211;
  const int H = 599;

  DeviceRef device = makeAndCommitDevice();

  FilterRef filter = device.newFilter("RT");
  REQUIRE(bool(filter));

  filter.set("quality", Quality::High);

  auto color  = makeConstImage(device, W, H);
  auto albedo = makeConstImage(device, W, H);
  auto output = makeConstImage(device, W, H);
  setFilterImage(filter, "color",  color);
  setFilterImage(filter, "albedo", albedo);
  setFilterImage(filter, "output", output);

  filter.set("hdr", true);

  filter.commit();
  REQUIRE(device.getError() == Error::None);

  filter.execute();
  REQUIRE(device.getError() == Error::None);

  SECTION("no updates")
  {
    // No changes
  }

  SECTION("same image size")
  {
    color = makeConstImage(device, W, H);
    setFilterImage(filter, "color", color);
  }

  SECTION("different image size")
  {
    color  = makeConstImage(device, W*2, H*2);
    albedo = makeConstImage(device, W*2, H*2);
    output = makeConstImage(device, W*2, H*2);
    setFilterImage(filter, "color",  color);
    setFilterImage(filter, "albedo", albedo);
    setFilterImage(filter, "output", output);
  }

  SECTION("different image format")
  {
    albedo = makeConstImage(device, W, H, 3, DataType::Float16);
    setFilterImage(filter, "albedo", albedo);
  }

  SECTION("unset image")
  {
    filter.unsetImage("albedo");
  }

  SECTION("unset image by setting to nullptr")
  {
    filter.setImage("albedo", nullptr, Format::Float3, 0, 0);
  }

  SECTION("different mode")
  {
    filter.set("hdr", false);
  }

  SECTION("different quality")
  {
    filter.set("quality", Quality::Balanced);
  }

  filter.commit();
  REQUIRE(device.getError() == Error::None);

  filter.execute();
  REQUIRE(device.getError() == Error::None);
}

// -------------------------------------------------------------------------------------------------

TEST_CASE("async filter", "[async_filter]")
{
  const int W = 799;
  const int H = 601;

  DeviceRef device = makeAndCommitDevice();

  FilterRef filter = device.newFilter("RT");
  REQUIRE(bool(filter));

  auto color  = makeConstImage(device, W, H);
  auto albedo = makeConstImage(device, W, H);
  auto output = makeImage(device, W, H);

  setFilterImage(filter, "color",  color);
  setFilterImage(filter, "output", output);
  filter.set("hdr", true);

  filter.commit();
  REQUIRE(device.getError() == Error::None);

  for (int i = 0; i < 3; ++i)
    filter.executeAsync();

  setFilterImage(filter, "albedo", albedo);
  filter.set("hdr", false);

  filter.commit();
  REQUIRE(device.getError() == Error::None);

  for (int i = 0; i < 2; ++i)
    filter.executeAsync();

  device.sync();
  REQUIRE(device.getError() == Error::None);

  filter.executeAsync();
  filter.release();

  REQUIRE(device.getError() == Error::None);
}

// -------------------------------------------------------------------------------------------------

void imageSizeTest(DeviceRef& device, int W, int H, bool execute = true)
{
  FilterRef filter = device.newFilter("RT");
  REQUIRE(bool(filter));

  auto color  = makeConstImage(device, W, H);
  auto output = makeImage(device, W, H);
  setFilterImage(filter, "color",  color);
  setFilterImage(filter, "output", output);

  filter.commit();
  REQUIRE(device.getError() == Error::None);

  if (execute)
  {
    filter.execute();
    REQUIRE(device.getError() == Error::None);
  }
}

TEST_CASE("image size", "[size]")
{
  DeviceRef device = makeAndCommitDevice();

  SECTION("image size: 0x0")
  {
    imageSizeTest(device, 0, 0);
  }

  SECTION("image size: [0,1]x[0,1]")
  {
    imageSizeTest(device, 0, 1);
    imageSizeTest(device, 1, 0);
  }

  SECTION("image size: [1,2]x[1,2]")
  {
    imageSizeTest(device, 1, 1);
    imageSizeTest(device, 1, 2);
    imageSizeTest(device, 2, 1);
    imageSizeTest(device, 2, 2);
  }

  SECTION("image size: 8192x4320")
  {
    imageSizeTest(device, 8192, 4320, false);
  }
}

// -------------------------------------------------------------------------------------------------

void sanitizationTest(DeviceRef& device, bool hdr, float value)
{
  const int W = 191;
  const int H = 347;

  FilterRef filter = device.newFilter("RT");
  REQUIRE(bool(filter));

  auto input  = makeConstImage(device, W, H, 3, DataType::Float32, value);
  auto output = makeImage(device, W, H, 3, DataType::Float32);
  setFilterImage(filter, "color",  input);
  setFilterImage(filter, "albedo", input);
  setFilterImage(filter, "normal", input);
  setFilterImage(filter, "output", output);
  filter.set("hdr", hdr);

  filter.commit();
  REQUIRE(device.getError() == Error::None);

  filter.execute();
  REQUIRE(device.getError() == Error::None);

  if (hdr)
    REQUIRE(isBetween(output, 0.f, std::numeric_limits<float>::max()));
  else
    REQUIRE(isBetween(output, 0.f, 1.f));
}

TEST_CASE("image sanitization", "[sanitization]")
{
  DeviceRef device = makeAndCommitDevice();

  SECTION("image sanitization: HDR")
  {
    sanitizationTest(device, true,  std::numeric_limits<float>::quiet_NaN());
    sanitizationTest(device, true,  std::numeric_limits<float>::infinity());
    sanitizationTest(device, true, -std::numeric_limits<float>::infinity());
    sanitizationTest(device, true, -100.f);
  }

  SECTION("image sanitization: LDR")
  {
    sanitizationTest(device, false,  std::numeric_limits<float>::quiet_NaN());
    sanitizationTest(device, false,  std::numeric_limits<float>::infinity());
    sanitizationTest(device, false,  10.f);
    sanitizationTest(device, false, -2.f);
  }
}

// -------------------------------------------------------------------------------------------------

struct Progress
{
  double n;    // current progress
  double nMax; // when to cancel execution

  Progress(double nMax) : n(0), nMax(nMax) {}
};

// Progress monitor callback function
bool progressCallback(void* userPtr, double n)
{
  Progress* progress = (Progress*)userPtr;
  REQUIRE((std::isfinite(n) && n >= 0 && n <= 1)); // n must be between 0 and 1
  REQUIRE(n >= progress->n); // n must not decrease
  progress->n = n;
  return n < progress->nMax; // cancel if reached nMax
}

void progressTest(DeviceRef& device, double nMax = 1000)
{
  const int W = 1283;
  const int H = 727;

  FilterRef filter = device.newFilter("RT");
  REQUIRE(bool(filter));

  auto image = makeConstImage(device, W, H);
  setFilterImage(filter, "color",  image);
  setFilterImage(filter, "output", image); // in-place

  Progress progress(nMax);
  filter.setProgressMonitorFunction(progressCallback, &progress);

  filter.set("maxMemoryMB", 0); // make sure there will be multiple tiles

  filter.commit();
  REQUIRE(device.getError() == Error::None);

  filter.execute();

  if (nMax <= 1)
  {
    // Execution should be cancelled but it's not guaranteed
    Error error = device.getError();
    REQUIRE((error == Error::None || error == Error::Cancelled));
    // Check whether the callback has not been called after requesting cancellation
    REQUIRE(progress.n >= nMax);
  }
  else
  {
    // Execution should be finished
    REQUIRE(device.getError() == Error::None);
    REQUIRE(progress.n == 1); // progress must be 100% at the end
  }
}

TEST_CASE("progress monitor", "[progress]")
{
  DeviceRef device = makeAndCommitDevice();

  SECTION("progress monitor: complete")
  {
    progressTest(device);
  }

  SECTION("progress monitor: cancel at the middle")
  {
    progressTest(device, 0.5);
  }

  SECTION("progress monitor: cancel at the beginning")
  {
    progressTest(device, 0);
  }

  SECTION("progress monitor: cancel at the end")
  {
    progressTest(device, 1);
  }
}

// -------------------------------------------------------------------------------------------------

TEST_CASE("user weights", "[user_weights]")
{
  DeviceRef device = makeAndCommitDevice();

  FilterRef filter = device.newFilter("RT");
  REQUIRE(bool(filter));

  const int W = 23;
  const int H = 99;
  auto image = makeConstImage(device, W, H);
  setFilterImage(filter, "color",  image);
  setFilterImage(filter, "output", image); // in-place

  REQUIRE(device.getError() == Error::None);

  std::vector<uint8_t> data(900);
  Random rng;
  for (size_t i = 0; i < data.size(); ++i)
    data[i] = uint8_t(rng.getUInt());

  SECTION("invalid parameter name")
  {
    filter.setData(nullptr, data.data(), data.size());
    REQUIRE(device.getError() == Error::InvalidArgument);

    filter.commit();
    REQUIRE(device.getError() == Error::None);
  }

  SECTION("nullptr data")
  {
    filter.setData("weights", nullptr, 0);
    REQUIRE(device.getError() == Error::None);
    filter.setData("weights", nullptr, data.size());
    REQUIRE(device.getError() == Error::InvalidArgument);

    filter.commit();
    REQUIRE(device.getError() == Error::None);
  }

  SECTION("invalid data")
  {
    filter.setData("weights", data.data(), data.size());
    REQUIRE(device.getError() == Error::None);

    filter.commit();
    REQUIRE(device.getError() == Error::InvalidOperation);
  }
}

#endif // defined(OIDN_FILTER_RT)

int main(int argc, char* argv[])
{
  Catch::Session session;

  std::string deviceStr = "default";

  using namespace Catch::clara;
  auto cli
    = session.cli()
    | Opt(deviceStr, "[0-9]+|default|cpu|sycl|cuda|hip")
        ["--device"]
        ("Open Image Denoise device to use");

  session.cli(cli);

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0)
    return returnCode;

  try
  {
    if (isdigit(deviceStr[0]))
      physicalDevice = fromString<int>(deviceStr);
    else
      deviceType = fromString<DeviceType>(deviceStr);
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return session.run();
}