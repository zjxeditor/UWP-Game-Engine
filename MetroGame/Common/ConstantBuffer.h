#pragma once

#include "DirectXHelper.h"
#include "LightHelper.h"

//  brief Wrapper class for cbuffers that handles creation and updating
//  for a fixed type specified by the template parameter T.
namespace DX
{
	// Constant buffers shared by multi renderers
	struct BasicPerFrameCB
	{
		DirectX::XMFLOAT4X4 View;
		DirectX::XMFLOAT4X4 InvView;
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ViewProj;
		DirectX::XMFLOAT4X4 LightProj;

		DX::DirectionalLight DirLights[3];
		DirectX::XMFLOAT3 EyePosW;
		float Pad0;

		DirectX::XMFLOAT4 FogColor;
		float  FogStart;
		float  FogRange;

		float GameTime;
		float ElapseTime;
	};

	struct BasicPerObjectCB
	{
		DirectX::XMFLOAT4X4 World;
		DirectX::XMFLOAT4X4 WorldInvTranspose;
		DirectX::XMFLOAT4X4 TexTransform;
		DX::Material Mat;
	};

	struct BasicTessSettings
	{
		float HeightScale;
		float MaxTessDistance;
		float MinTessDistance;
		float MinTessFactor;
		float MaxTessFactor;
	};

	struct BasicSsaoSettings
	{
		DirectX::XMFLOAT4 OffsetVectors[14];
		DirectX::XMFLOAT4 FrustumCorners[4];

		// Coordinates given in view space.
		float    OcclusionRadius;
		float    OcclusionFadeStart;
		float    OcclusionFadeEnd;
		float    SurfaceEpsilon;
	};

	struct BasicTextureSettings
	{
		float TexelWidth;
		float TexelHeight;
	};

	struct WorldMatrix
	{
		DirectX::XMFLOAT4X4 World;
	};

	struct SkinnedTransforms
	{
		DirectX::XMFLOAT4X4 BoneTransforms[96];
	};


	template<typename T>
	class ConstantBuffer
	{
	public:
		ConstantBuffer() :
			m_initialized(false)
		{
		}

		// Public structure instance mirroring the data stored in the constant buffer.
		T Data;

		// Returns the d3d11 buffer this class wraps.  
		ID3D11Buffer* GetBuffer() const
		{
			return m_buffer.Get();
		}

		bool GetInitalizeState() { return m_initialized; }

		// Initializes the constant buffer.
		void Initialize(ID3D11Device* device)
		{
			HRESULT hr = S_OK;

			// Make constant buffer multiple of 16 bytes.
			D3D11_BUFFER_DESC desc;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;
			desc.ByteWidth = static_cast<UINT>(sizeof(T) + (16 - (sizeof(T) % 16)));
			desc.StructureByteStride = 0;

			DX::ThrowIfFailed(device->CreateBuffer(&desc, 0, m_buffer.GetAddressOf()));
			m_initialized = true;
		}

		// Copies the system memory constant buffer data to the GPU constant buffer.
		// This call should be made as infrequently as possible.
		void ApplyChanges(ID3D11DeviceContext* dc)
		{
			_ASSERT(m_initialized);

			D3D11_MAPPED_SUBRESOURCE mappedResource;
			dc->Map(m_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			CopyMemory(mappedResource.pData, &Data, sizeof(T));
			dc->Unmap(m_buffer.Get(), 0);
		}

		// Reset the constant buffer
		void Reset()
		{
			m_buffer.Reset();
			m_initialized = false;
		}

	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
		bool m_initialized;
	};
}
