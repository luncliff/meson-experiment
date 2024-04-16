#pragma once
#include <winrt/windows.foundation.h>

#include <d3d11_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>

#if !defined(_INTERFACE_)
#define _INTERFACE_
#endif
namespace experiment {

_INTERFACE_ uint32_t get_build_version();

}
