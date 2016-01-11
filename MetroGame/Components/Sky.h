#pragma once

#include <DirectXMath.h>
#include <ppltasks.h>
#include "Common/ShaderMgr.h"
#include "Common/TextureMgr.h"
#include "Common/GameTimer.h"
#include "Common/ConstantBuffer.h"
#include "Common/DeviceResources.h"
#include "Common/Camera.h"

// Render an ellipse sky with cube mapping technique.

namespace DXFramework
{
	class Sky
	{
	public:
		Sky(const std::shared_ptr<DX::DeviceResources>& deviceResources,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB,
			const std::shared_ptr<DX::Camera>& camera);
		
		void Initialize(const std::wstring& texFileName, float skySphereRadius);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Update();
		void Render();

	private:
		void BuildSkyGeometryBuffers();

	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>> m_perObjectCB;
		std::shared_ptr<DX::Camera> m_camera;

		// Direct3D data resources 
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_skyVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_skyIB;

		//Shaders
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_skyInputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_skyVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_skyPS;

		// SRV
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_skyCubeMapSRV;
		
		// Custom data
		UINT m_indexCount;
		std::wstring m_texFileName;
		float m_sphereRadius;
		DirectX::XMFLOAT4X4 m_skyWorld;

		bool m_initialized;
		bool m_loadingComplete;
	};
}


