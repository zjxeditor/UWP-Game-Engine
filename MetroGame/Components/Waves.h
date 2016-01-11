#pragma once

#include <DirectXMath.h>
#include <ppltasks.h>
#include "Common/ShaderMgr.h"
#include "Common/TextureMgr.h"
#include "Common/GameTimer.h"
#include "Common/ConstantBuffer.h"
#include "Common/DeviceResources.h"

// Waves simulation without computer shaders.

namespace DXFramework
{
	enum class WavesRenderOption
	{
		Light3,
		Light3Tex,
		Light3TexFog
	};

	class Waves
	{
	public:
		Waves(const std::shared_ptr<DX::DeviceResources>& deviceResources,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB);
		~Waves();

		void Initialize(UINT m, UINT n, float dx, float dt, float speed, float damping, std::wstring texName);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Update(DX::GameTimer const& timer);
		void Render();

	public:
		// Configure functions
		void SetWorld(const DirectX::XMFLOAT4X4& world) { m_wavesWorld = world; }
		void SetMat(const DX::Material& mat) { m_wavesMat = mat; }
		void SetTexTransform(const DirectX::XMFLOAT4X4& trans) { m_wavesTexTransform = trans; }
		void SetRenderOption(WavesRenderOption r) { m_renderOptions = r; }
		void UpdateTextureSRV(ID3D11ShaderResourceView* srv) { m_wavesMapSRV = srv; }
		float GetWidth()const { return m_numCols*m_spatialStep; }
		float GetDepth()const { return m_numRows*m_spatialStep; }

	private:
		void Update(float dt);
		void Disturb(UINT i, UINT j, float magnitude);
		void BuildWaveGeometryBuffers();

	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>> m_perObjectCB;

		// Direct3D data resources 
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_wavesVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_wavesIB;

		//Shaders
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_wavesInputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_wavesVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_wavesLight3PS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_wavesLight3TexPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_wavesLight3TexFogPS;

		// SRV 
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_wavesMapSRV;

		// Custom data
		UINT m_numRows;
		UINT m_numCols;

		UINT m_vertexCount;
		UINT m_triangleCount;

		std::wstring m_textureFileName;
		DX::Material m_wavesMat;
		DirectX::XMFLOAT4X4 m_wavesTexTransform;
		DirectX::XMFLOAT4X4 m_wavesWorld;
		WavesRenderOption m_renderOptions;

		float m_k[3];		// Simulation constants we can pre-compute.
		float m_timeStep;
		float m_spatialStep;

		DirectX::XMFLOAT3* m_prevSolution;
		DirectX::XMFLOAT3* m_currSolution;
		DirectX::XMFLOAT3* m_normals;

		// Warning! Using smart pointer will cause a worse performance.
		//std::unique_ptr<DirectX::XMFLOAT3[]> m_prevSolution;
		//std::unique_ptr<DirectX::XMFLOAT3[]> m_currSolution;
		//std::unique_ptr<DirectX::XMFLOAT3[]> m_normals;

		bool m_initialized;
		bool m_loadingComplete;
	};
}

