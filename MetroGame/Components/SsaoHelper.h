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

// Helper that draws the Ssao map for ambient enhance effect
namespace DXFramework
{
	class SsaoHelper
	{
	public:
		SsaoHelper(
			const std::shared_ptr<DX::DeviceResources>& deviceResources,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
			const std::shared_ptr<DX::Camera>& camera);
		void Initialize(UINT blurCount = 3, float occlusionRadius = 0.5f, float occlusionFadeStart = 0.2f, float occlusionFadeEnd = 2.0f, float surfaceEpsilon = 0.05f);
		void CreateWindowSizeDependentResources();
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Render(const std::function<void()>& DrawMap);

	public:
		ID3D11ShaderResourceView* GetSsaoMapSRV() { return m_ambientSRV0.Get(); }

	private:
		void BlurAmbientMap();
		void BlurAmbientMap(ID3D11ShaderResourceView* inputSRV, ID3D11RenderTargetView* outputRTV, bool horzBlur);
		void BuildFrustumFarCorners();
		void BuildFullScreenQuad();
		void BuildTextureViews();
		void BuildRandomVectorTexture();
		void BuildOffsetVectors();
		void ResetTextureViews();

	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;
		std::shared_ptr<DX::Camera> m_camera;

		// Direct3D data resources 
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_quadVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_quadIB;
		DX::ConstantBuffer<DX::BasicSsaoSettings> m_ssaoSettingsCB;
		DX::ConstantBuffer<DX::BasicTextureSettings> m_texSettingsCB;

		// Shaders
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_ssaoVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ssaoPS;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_BilateralBlurVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_BilateralBlurPSHori;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_BilateralBlurPSVert;

		// Resources
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_randomVectorSRV;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_normalDepthRTV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_normalDepthSRV;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilDSV;
		// Need two for ping-ponging during blur.
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_ambientRTV0;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ambientSRV0;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_ambientRTV1;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ambientSRV1;

		// Custom data
		D3D11_VIEWPORT m_ambientMapViewport;
		UINT m_blurCount;
		bool m_updateSsaoSettings;
		bool m_updateTexSettings;

		bool m_initialized;
		bool m_loadingComplete;
	};
}


