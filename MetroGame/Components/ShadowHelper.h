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

// Helper that draws the depth map for shadow effect
namespace DXFramework
{
	class ShadowHelper
	{
	public:
		ShadowHelper(
			const std::shared_ptr<DX::DeviceResources>& deviceResources, 
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
			const std::shared_ptr<DX::Camera>& camera);
		void Initialize(const DirectX::XMFLOAT3& sceneCenter, float sceneRadius, int texelSize = 2048);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Update(const DirectX::XMFLOAT3& lightDirection);
		void Render(const std::function<void()>& DrawDepth);

	public:
		ID3D11ShaderResourceView* GetDepthMapSRV() { return m_depthMapSRV.Get(); }

	private:
		void BuildDepthMapViews();

	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;
		std::shared_ptr<DX::Camera> m_camera;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_depthMapSRV;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthMapDSV;

		DirectX::XMFLOAT4X4 m_lightView;
		DirectX::XMFLOAT4X4 m_lightProj;
		DirectX::XMFLOAT3 m_sceneCenter;
		float m_sceneRadius;
		UINT m_texelSize;
		D3D11_VIEWPORT m_viewport;

		bool m_initialized;
		bool m_loadingComplete;
	};
}


