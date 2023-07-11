// Copyright 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "device.h"
#include "context.h"
#include "rt_filter.h"
#include "rtlightmap_filter.h"

OIDN_NAMESPACE_BEGIN

  thread_local Device::ErrorState Device::globalError;

  int PhysicalDevice::getInt(const std::string& name) const
  {
    if (name == "type")
      return static_cast<int>(type);
    else if (name == "uuidSupported")
      return uuidSupported;
    else if (name == "luidSupported")
      return luidSupported;
    else if (name == "nodeMask")
    {
      if (!luidSupported)
        throw Exception(Error::InvalidArgument,
                        "physical device node mask unavailable, check luidSupported first");
      return nodeMask;
    }
    else if (name == "pciAddressSupported")
      return pciAddressSupported;
    else if (name == "pciDomain")
    {
      if (!pciAddressSupported)
        throw Exception(Error::InvalidArgument,
                        "physical device PCI domain number unavailable, check pciAddressSupported first");
      return pciDomain;
    }
    else if (name == "pciBus")
    {
      if (!pciAddressSupported)
        throw Exception(Error::InvalidArgument,
                        "physical device PCI bus number unavailable, check pciAddressSupported first");
      return pciBus;
    }
    else if (name == "pciDevice")
    {
      if (!pciAddressSupported)
        throw Exception(Error::InvalidArgument,
                        "physical device PCI device number unavailable, check pciAddressSupported first");
      return pciDevice;
    }
    else if (name == "pciFunction")
    {
      if (!pciAddressSupported)
        throw Exception(Error::InvalidArgument,
                        "physical device PCI function number unavailable, check pciAddressSupported first");
      return pciFunction;
    }
    else
      throw Exception(Error::InvalidArgument, "unknown physical device parameter or type mismatch: '" + name + "'");
  }

  const char* PhysicalDevice::getString(const std::string& name) const
  {
    if (name == "name")
      return this->name.c_str();
    else
      throw Exception(Error::InvalidArgument, "unknown physical device parameter or type mismatch: '" + name + "'");
  }

  Data PhysicalDevice::getData(const std::string& name) const
  {
    if (name == "uuid")
    {
      if (!uuidSupported)
        throw Exception(Error::InvalidArgument, "physical device UUID unavailable, check uuidSupported first");
      return {uuid.bytes, sizeof(uuid.bytes)};
    }
    else if (name == "luid")
    {
      if (!luidSupported)
        throw Exception(Error::InvalidArgument, "physical device LUID unavailable, check luidSupported first");
      return {luid.bytes, sizeof(luid.bytes)};
    }
    else
      throw Exception(Error::InvalidArgument, "unknown physical device parameter or type mismatch: '" + name + "'");
  }

  Device::Device()
  {
    // Get default values from environment variables
    if (getEnvVar("OIDN_VERBOSE", verbose))
      error.setVerbose(verbose);
  }

  void Device::setError(Device* device, Error code, const std::string& message)
  {
    // Update the stored error only if the previous error was queried
    if (device)
    {
      ErrorState& curError = device->error.get();

      if (curError.code == Error::None)
      {
        curError.code = code;
        curError.message = message;
      }

      // Print the error message in verbose mode
      device->printError(message);

      // Call the error callback function
      ErrorFunction errorFunc;
      void* errorUserPtr;

      {
        std::lock_guard<std::mutex> lock(device->mutex);
        errorFunc = device->errorFunc;
        errorUserPtr = device->errorUserPtr;
      }

      if (errorFunc)
        errorFunc(errorUserPtr, code, (code == Error::None) ? nullptr : message.c_str());
    }
    else
    {
      if (globalError.code == Error::None)
      {
        globalError.code = code;
        globalError.message = message;
      }

      // Print the error message in verbose mode
      Context::get().printError(message);
    }
  }

  Error Device::getError(Device* device, const char** outMessage)
  {
    // Return and clear the stored error code, but keep the error message so pointers to it will
    // remain valid until the next getError call
    if (device)
    {
      ErrorState& curError = device->error.get();
      const Error code = curError.code;
      if (outMessage)
        *outMessage = (code == Error::None) ? nullptr : curError.message.c_str();
      curError.code = Error::None;
      return code;
    }
    else
    {
      const Error code = globalError.code;
      if (outMessage)
        *outMessage = (code == Error::None) ? nullptr : globalError.message.c_str();
      globalError.code = Error::None;
      return code;
    }
  }

  void Device::setErrorFunction(ErrorFunction func, void* userPtr)
  {
    errorFunc = func;
    errorUserPtr = userPtr;
  }

  int Device::getInt(const std::string& name)
  {
    if (name == "type")
      return static_cast<int>(getType());
    else if (name == "version")
      return OIDN_VERSION;
    else if (name == "versionMajor")
      return OIDN_VERSION_MAJOR;
    else if (name == "versionMinor")
      return OIDN_VERSION_MINOR;
    else if (name == "versionPatch")
      return OIDN_VERSION_PATCH;
    else if (name == "verbose")
      return verbose;
    else if (name == "systemMemorySupported")
      return systemMemorySupported;
    else if (name == "managedMemorySupported")
      return managedMemorySupported;
    else if (name == "externalMemoryTypes")
      return static_cast<int>(externalMemoryTypes);
    else
      throw Exception(Error::InvalidArgument, "unknown device parameter or type mismatch: '" + name + "'");
  }

  void Device::setInt(const std::string& name, int value)
  {
    if (name == "verbose")
    {
      if (!isEnvVar("OIDN_VERBOSE"))
      {
        verbose = value;
        error.setVerbose(value);
      }
      else if (verbose != value)
        printWarning("OIDN_VERBOSE environment variable overrides device parameter");
    }
    else
      printWarning("unknown device parameter or type mismatch: '" + name + "'");

    dirty = true;
  }

  void Device::commit()
  {
    if (isCommitted())
      throw Exception(Error::InvalidOperation, "device can be committed only once");

    if (isVerbose())
    {
      std::cout << std::endl;
      std::cout << "Intel(R) Open Image Denoise " << OIDN_VERSION_STRING << std::endl;
      std::cout << "  Compiler  : " << getCompilerName() << std::endl;
      std::cout << "  Build     : " << getBuildName() << std::endl;
      std::cout << "  OS        : " << getOSName() << std::endl;
    }

    init();

    if (isVerbose())
      std::cout << std::endl;

    dirty = false;
    committed = true;
  }

  void Device::checkCommitted()
  {
    if (dirty)
      throw Exception(Error::InvalidOperation, "changes to the device are not committed");
  }

  Ref<Filter> Device::newFilter(const std::string& type)
  {
    if (isVerbose(2))
      std::cout << "Filter: " << type << std::endl;

    Ref<Filter> filter;

    if (type == "RT")
      filter = makeRef<RTFilter>(this);
    else if (type == "RTLightmap")
      filter = makeRef<RTLightmapFilter>(this);
    else
      throw Exception(Error::InvalidArgument, "unknown filter type: '" + type + "'");

    return filter;
  }

OIDN_NAMESPACE_END
