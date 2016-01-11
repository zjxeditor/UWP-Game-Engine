#pragma once

#include <DirectXMath.h>
#include <ppltasks.h>
#include "Common/ShaderMgr.h"
#include "Common/TextureMgr.h"
#include "Common/GameTimer.h"
#include "Common/ConstantBuffer.h"
#include "Common/DeviceResources.h"
#include "Common/Camera.h"

// Render an ellipse MapDisplayer with cube mapping technique.



namespace DXFramework
{
	enum class MapDisplayType
	{
		RED,
		GREEN,
		BLUE,
		ALPHA,
		ALL
	};

	class MapDisplayer
	{
	public:
		MapDisplayer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		
		void Initialize(MapDisplayType type, const DirectX::XMFLOAT4X4& world, bool unbind = true);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Render();
	public:
		void UpdateMapSRV(ID3D11ShaderResourceView* srv) { m_mapSRV = srv; }

	private:
		void BuildQuadGeometryBuffers();

	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Direct3D data resources 
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_quadVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_quadIB;
		DX::ConstantBuffer<DX::WorldMatrix> m_worldCB;

		//Shaders
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_redPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_greenPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_bluePS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_alphaPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_allPS;

		// SRV
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_mapSRV;
		
		// Custom data
		DirectX::XMFLOAT4X4 m_world;
		MapDisplayType m_type;
		bool m_unbind;

		bool m_initialized;
		bool m_loadingComplete;
	};
}


