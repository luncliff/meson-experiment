#pragma once
#if defined(_WIN32)
#include <winrt/windows.foundation.h>

#include <d3d11_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#endif

#if !defined(_INTERFACE_)
#define _INTERFACE_
#endif

namespace experiment {

_INTERFACE_ uint32_t get_build_version();

_INTERFACE_ bool check_vulkan_available() noexcept;
_INTERFACE_ bool check_vulkan_runtime(uint32_t &api_version, uint32_t &runtime_version) noexcept;

} // namespace experiment
