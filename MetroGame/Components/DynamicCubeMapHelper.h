#pragma once

#include <DirectXMath.h>
#include <ppltasks.h>
#include <functional>
#include "Common/ShaderMgr.h"
#include "Common/TextureMgr.h"
#include "Common/GameTimer.h"
#include "Common/ConstantBuffer.h"
#include "Common/DeviceResources.h"
#include "Common/Camera.h"

// Helper that draws the dynamic cube map

namespace DXFramework
{
	class DynamicCubeMapHelper
	{
	public:
		DynamicCubeMapHelper(const std::shared_ptr<DX::DeviceResources>& deviceResources,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
			const std::shared_ptr<DX::Camera>& camera);
		void Initialize(const DirectX::XMFLOAT3 center, int cubeMapSize = 256);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();

		void Render(const std::function<void()>& DrawMap);

	public:
		// Final dynamic built cube-map
		ID3D11ShaderResourceView* GetDynamicCubeMapSRV() { return m_cubeMapSRV.Get(); }

	private:
		void BuildCubeFaceCamera();
		void BuildCubeMapViews();

	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;
		std::shared_ptr<DX::Camera> m_camera;

		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_cubeMapDSV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_cubeMapSRV;
		std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, 6> m_cubeMapRTV;
		
		D3D11_VIEWPORT m_cubeMapViewport;
		DirectX::XMFLOAT3 m_center;
		int m_cubeMapSize;
		std::array<DX::Camera, 6> m_cubeMapCamera;

		bool m_initialized;
		bool m_loadingComplete;
	};
}


