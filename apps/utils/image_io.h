// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <tuple>

namespace oidn {

  struct ImageBuffer
  {
    std::vector<float> buffer;
    std::vector<float> alpha;
    int width;
    int height;
    int numChannels;

    ImageBuffer()
      : width(0),
        height(0),
        numChannels(0) {}

    ImageBuffer(int width, int height, int numChannels, bool hasAlpha = false)
      : buffer(size_t(width) * height * numChannels, 0.0f),
        width(width),
        height(height),
        numChannels(numChannels)
    {
      if (hasAlpha)
        alpha.resize(width * height, 1.0f);
    }

    operator bool() const
    {
      return data() != nullptr;
    }

    const float& operator [](size_t i) const { return buffer[i]; }
    float& operator [](size_t i) { return buffer[i]; }

    const float* data() const { return buffer.data(); }
    float* data() { return buffer.data(); }
  
    size_t size() const { return buffer.size(); }
    std::array<int, 3> dims() const { return {width, height, numChannels}; }
  };

  // Loads an image with an optionally specified number of channels (loads all
  // channels by default)
  std::shared_ptr<ImageBuffer> loadImage(const std::string& filename, int numChannels = 0);

  // Loads an image with/without sRGB to linear conversion
  std::shared_ptr<ImageBuffer> loadImage(const std::string& filename, int numChannels, bool srgb);

  // Saves an image
  void saveImage(const std::string& filename, const ImageBuffer& image);

  // Saves an image with/without linear to sRGB conversion
  void saveImage(const std::string& filename, const ImageBuffer& image, bool srgb);

  // Compares an image to a reference image and returns the number of errors
  // and the maximum error value
  std::tuple<size_t, float> compareImage(const ImageBuffer& image,
                                         const ImageBuffer& ref,
                                         float threshold);

} // namespace oidn
