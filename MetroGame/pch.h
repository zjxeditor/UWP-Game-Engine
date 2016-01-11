#pragma once

#define XMASSERT(Expression) ((VOID)0)
#define _DECLSPEC_ALIGN_16_   __declspec(align(16))

//#define USE_WAIT_SWAP_CHAIN  

#include <wrl.h>
#include <wrl/client.h>
#include <dxgi1_4.h>
#include <d3d11_3.h>
#include <d2d1_3.h>
#include <d2d1effects_2.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <memory>
#include <agile.h>
#include <concrt.h>
#include <cassert>
#include "TaskExtensions.h"