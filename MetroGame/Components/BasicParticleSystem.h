#pragma once

#include <DirectXMath.h>
#include <ppltasks.h>
#include "Common/ShaderMgr.h"
#include "Common/TextureMgr.h"
#include "Common/GameTimer.h"
#include "Common/ConstantBuffer.h"
#include "Common/DeviceResources.h"


namespace DXFramework
{
	struct BasicParticleSystemInitInfo
	{
		BasicParticleSystemInitInfo() : MaxParticles(100), RandomTexSRV(nullptr)
		{
			AccelW = DirectX::XMFLOAT3(0.0f, -9.8f, 0.0f);
			EmitPosW = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			EmitDirW = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		}

		std::wstring DrawVSFileName;
		std::wstring SOGSFileName;
		std::wstring DrawGSFileName;
		std::wstring DrawPSFileName;
		std::vector<std::wstring> TexFileNames;
		UINT MaxParticles;
		DirectX::XMFLOAT3 AccelW;
		DirectX::XMFLOAT3 EmitPosW;
		DirectX::XMFLOAT3 EmitDirW;
		ID3D11ShaderResourceView* RandomTexSRV;
	};

	class BasicParticleSystem
	{	
	public:
		BasicParticleSystem(const std::shared_ptr<DX::DeviceResources>& deviceResources,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB);

		void Initialize(const BasicParticleSystemInitInfo& initInfo);
		void Update(float dt);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Render();

		void Reset();

	public:
		// Configure functions
		void SetEmitPos(const DirectX::XMFLOAT3& emitPosW) { m_settingsCB.Data.EmitPosW = emitPosW; m_needUpdateCB = true; }
		void SetEmitDir(const DirectX::XMFLOAT3& emitDirW) { m_settingsCB.Data.EmitDirW = emitDirW; m_needUpdateCB = true; }
		void SetAccel(const DirectX::XMFLOAT3& accel) { m_settingsCB.Data.AccelW = accel; m_needUpdateCB = true; }

		float GetAge()const { return m_age; }
		std::wstring GetTextureArraySignature() { return m_textureArraySignature; };
		ID3D11ShaderResourceView* GetRandomTexSRV() { return m_randomTexSRV.Get(); }

	private:
		void BuildVB();

	private:
		struct ParticleSettingsCB
		{
			DirectX::XMFLOAT3 EmitPosW;
			float pad;
			DirectX::XMFLOAT3 EmitDirW;
			UINT TexNum;
			DirectX::XMFLOAT3 AccelW;
		};
		
	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;

		// Direct3D data resources 
		DX::ConstantBuffer<ParticleSettingsCB> m_settingsCB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_initVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_drawVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_streamOutVB;

		//Shaders
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_soVS;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_drawVS;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_soGS;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_drawGS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_drawPS;
		
		// SRV
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texArraySRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_randomTexSRV;

		// Custom data
		BasicParticleSystemInitInfo m_initInfo;

		bool m_firstRun;
		bool m_needUpdateCB;
		float m_age;
		const std::wstring m_signatureBase = L"BasicParticleSystemTexArray";
		std::wstring m_textureArraySignature;
		static int m_signatureIndex;

		bool m_initialized;
		bool m_loadingComplete;
	};
}


