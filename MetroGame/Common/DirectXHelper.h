#pragma once

#include <ppltasks.h>	// For create_task
#include <string>
#include <sstream>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Set a breakpoint on this line to catch Win32 API errors.
			throw Platform::Exception::CreateException(hr);
		}
	}

	// Function that reads from a binary file asynchronously.
	Concurrency::task<std::shared_ptr<std::vector<byte>>> ReadDataAsync(const std::wstring& filename);
	std::shared_ptr<std::vector<byte>> ReadData(const std::wstring& filename);

	// Merge bounding box / sphere
	DirectX::BoundingBox MergeBoundingBox(const std::vector<DirectX::BoundingBox>& boxList);
	DirectX::BoundingSphere MergeBoundingSphere(const std::vector<DirectX::BoundingSphere>& sphereList);

	// Converts a length in device-independent pixels (DIPs) to a length in physical pixels.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
	}

	// Converts a length in physical pixels to a length in device-independent pixels (DIPs).
	inline float ConvertPixelsToDips(float pixels)
	{
		static const float dipsPerInch = 96.0f;
		return pixels * dipsPerInch / Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi; // Do not round.
	}

	// Converts a length in physical pixels to a length in device-independent pixels (DIPs) using the provided DPI. This removes the need
	// to call this function from a thread associated with a CoreWindow, which is required by DisplayInformation::GetForCurrentView().
	inline float ConvertPixelsToDips(float pixels, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return pixels * dipsPerInch / dpi; // Do not round.
	}

#if defined(_DEBUG)
	// Check for SDK Layer support.
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
			nullptr,                    // Any feature level will do.
			0,
			D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
			nullptr,                    // No need to keep the D3D device reference.
			nullptr,                    // No need to know the feature level.
			nullptr                     // No need to keep the D3D device context reference.
			);

		return SUCCEEDED(hr);
	}
#endif

	// String converter
	template<typename T>
	inline std::wstring ToString(const T& s)
	{
		std::wostringstream oss;
		oss << s;

		return oss.str();
	}

	template<typename T>
	inline T FromString(const std::wstring& s)
	{
		T x;
		std::wistringstream iss(s);
		iss >> x;

		return x;
	}

	void ExtractFrustumPlanes(DirectX::XMFLOAT4 planes[6], const DirectX::XMFLOAT4X4& M);

	void CreateRandomTexture1DSRV(ID3D11Device* device, ID3D11ShaderResourceView** textureView);

	// #define XMGLOBALCONST extern CONST __declspec(selectany)
	//   1. extern so there is only one copy of the variable, and not a separate
	//      private copy in each .obj.
	//   2. __declspec(selectany) so that the compiler does not complain about
	//      multiple definitions in a .cpp file (it can pick anyone and discard 
	//      the rest because they are constant--all the same).
	
	/// Converts XMVECTOR to XMCOLOR, where XMVECTOR represents a color.
	inline DirectX::PackedVector::XMCOLOR ToXmColor(DirectX::FXMVECTOR v)
	{
		DirectX::PackedVector::XMCOLOR dest;
		DirectX::PackedVector::XMStoreColor(&dest, v);
		return dest;
	}

	/// Converts XMVECTOR to XMFLOAT4, where XMVECTOR represents a color.
	inline DirectX::XMFLOAT4 ToXmFloat4(DirectX::FXMVECTOR v)
	{
		DirectX::XMFLOAT4 dest;
		DirectX::XMStoreFloat4(&dest, v);
		return dest;
	}

	inline UINT ArgbToAbgr(UINT argb)
	{
		BYTE A = (argb >> 24) & 0xff;
		BYTE R = (argb >> 16) & 0xff;
		BYTE G = (argb >> 8) & 0xff;
		BYTE B = (argb >> 0) & 0xff;

		return (A << 24) | (B << 16) | (G << 8) | (R << 0);
	}

	// Record wire frame state.
	static bool WireFrameRender = false;
}
