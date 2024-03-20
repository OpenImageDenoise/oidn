// Copyright 2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "module.h"
#include "context.h"

#if !defined(_WIN32)
  #include <dlfcn.h>
  #include <fstream>
#endif

OIDN_NAMESPACE_BEGIN

  ModuleLoader::ModuleLoader()
  {
    // Get the path of the current module
    const Path path = getModulePath();

    // Remove the filename from the path
    const size_t lastPathSep = path.find_last_of(pathSeps);
    if (lastPathSep == Path::npos)
      throw std::runtime_error("could not get absolute path of module directory");
    modulePathPrefix = path.substr(0, lastPathSep + 1);
  }

  bool ModuleLoader::load(const std::string& name)
  {
    if (modules.find(name) != modules.end())
      return true; // module already loaded

    // Get the path of the module to load
    std::string filename = "OpenImageDenoise_" + name;
  #if defined(_WIN32)
    filename += ".dll";
  #else
    const std::string versionStr = "." + toString(OIDN_VERSION_MAJOR) +
                                   "." + toString(OIDN_VERSION_MINOR) +
                                   "." + toString(OIDN_VERSION_PATCH);
  #if defined(__APPLE__)
    filename = "lib" + filename + versionStr + ".dylib";
  #else
    filename = "lib" + filename + ".so" + versionStr;
  #endif
  #endif

    const Path path = modulePathPrefix + Path(filename.begin(), filename.end());

    // Load the module
  #if defined(_WIN32)
    // Prevent the system from displaying a message box when the module fails to load
    UINT prevErrorMode = GetErrorMode();
    SetErrorMode(prevErrorMode | SEM_FAILCRITICALERRORS);
    void* module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
    SetErrorMode(prevErrorMode);
  #else
    void* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  #endif
    if (module == nullptr)
      return false;

    // Get the address of the module init function
    const std::string initSymbol =
      (OIDN_TO_STRING(OIDN_NAMESPACE_C) "_init_module_") + name + ("_v" OIDN_TO_STRING(OIDN_VERSION));
    void* initAddress = getSymbolAddress(module, initSymbol);
    if (initAddress == nullptr)
    {
      Context::get().printWarning("invalid module: '" + filename + "'");
      closeModule(module);
      return false;
    }

    // Call the module init function
    auto initFunc = reinterpret_cast<void (*)()>(initAddress);
    initFunc();

    // The module has been loaded successfully.
    // We won't unload the module manually to avoid issues due to the undefined module unloading
    // and static object destruction order at process exit. This intentional "leak" is fine
    // because the modules are owned by the context which is static, and the modules will be
    // unloaded at process exit anyway. Thus we don't need the module handle anymore.
    modules.insert(name);

    Context::get().printDebug("Loaded module: '" + filename + "'");
    return true;
  }

  void* ModuleLoader::getSymbolAddress(void* module, const std::string& name)
  {
  #if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name.c_str()));
  #else
    return dlsym(module, name.c_str());
  #endif
  }

  void ModuleLoader::closeModule(void* module)
  {
  #if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(module));
  #else
    dlclose(module);
  #endif
  }

  ModuleLoader::Path ModuleLoader::getModulePath(void* address)
  {
    if (address == nullptr)
      address = reinterpret_cast<void*>(&getModulePath); // any other function in this module would work

  #if defined(_WIN32)

    // Get the handle of the module which contains the address
    HMODULE module;
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExA(flags, reinterpret_cast<LPCSTR>(address), &module))
      throw std::runtime_error("GetModuleHandleExA failed");

    // Get the path of the module
    // Since we don't know the length of the path, we use a buffer of increasing size
    DWORD pathSize = MAX_PATH + 1;
    for (; ;)
    {
      std::vector<wchar_t> path(pathSize);
      DWORD result = GetModuleFileNameW(module, path.data(), pathSize);
      if (result == 0)
        throw std::runtime_error("GetModuleFileNameW failed");
      else if (result < pathSize)
        return path.data();
      else
        pathSize *= 2;
    }

  #else

    // dladdr should return an absolute path on Linux except for the main executable
    // On macOS it should always return an absolute path
    Dl_info info;
    if (dladdr(address, &info))
    {
      // Check whether the path is absolute
      if (info.dli_fname && info.dli_fname[0] == '/')
        return info.dli_fname;
    }

  #if defined(__APPLE__)
    // This shouldn't happen
    throw std::runtime_error("failed to get absolute path with dladdr");
  #else
    // We failed to get an absolute path, so we try to parse /proc/self/maps
    std::ifstream file("/proc/self/maps");
    if (!file)
      throw std::runtime_error("could not open /proc/self/maps");

    // Parse the lines
    for (std::string lineStr; std::getline(file, lineStr); )
    {
      std::istringstream line(lineStr);

      // Parse the address range
      uintptr_t addressBegin, addressEnd;
      line >> std::hex;
      line >> addressBegin;
      if (line.get() != '-')
        continue; // parse error
      line >> addressEnd;
      if (!isspace(line.peek()) || !line)
        continue; // parse error

      // Check whether the address is in this range
      if (reinterpret_cast<uintptr_t>(address) <  addressBegin ||
          reinterpret_cast<uintptr_t>(address) >= addressEnd)
        continue;

      // Skip the permissions, offset, device, inode
      std::string str;
      for (int i = 0; i < 4; ++i)
        line >> str;

      // Parse the path
      line >> std::ws;
      if (!std::getline(line, str))
        continue; // no path or parse error

      // Check whether the path is absolute
      if (str[0] == '/')
        return str;
    }

    throw std::runtime_error("could not find address in /proc/self/maps");
  #endif

  #endif
  }


OIDN_NAMESPACE_END