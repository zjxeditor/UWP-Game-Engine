#pragma once

#include <DirectXMath.h>
#include <ppltasks.h>
#include "Common/ShaderMgr.h"
#include "Common/TextureMgr.h"
#include "Common/GameTimer.h"
#include "Common/ConstantBuffer.h"
#include "Common/DeviceResources.h"

// Billboard using alpha to coverage technique. Trees' position data
// is directly set in the world coordinates.
namespace DXFramework
{
	enum class BillTreeRenderOption
	{
		Light3,
		Light3TexClip,
		Light3TexClipFog
	};
	
	class BillboardTrees
	{
	public:
		BillboardTrees(const std::shared_ptr<DX::DeviceResources>& deviceResources,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB);

		// Custom data provide
		void Initialize(const std::vector<DX::PointSize>& Data, const std::vector<std::wstring>& treeFileNames);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Render();

	public:
		// Configure functions
		void SetRenderOption(BillTreeRenderOption r) { m_renderOptions = r; }
		void SetAlphaToCoverage(bool a) { m_alphaToCoverage = a; }
		std::wstring GetTextureArraySignature() { return m_textureArraySignature; };

	private:
		void BuildTreeSpritesBuffer();

	private:
		struct BillTreeSettingsCB
		{
			DX::Material Mat;
			int TypeNum;
		};

	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;

		// Direct3D data resources 
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_treeSpriteVB;
		DX::ConstantBuffer<BillTreeSettingsCB> m_treeSettingsCB;	// Own specific constant buffer

		//Shaders
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_treeInputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_treeVS;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_treeGS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_treeLight3PS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_treeLight3TexClipPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_treeLight3TexClipFogPS;

		// SRV
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_treeTextureMapArraySRV;

		// Custom data
		DX::Material m_treeMat;
		UINT m_treeTypeNum;
		UINT m_treeCount;

		std::vector<DX::PointSize> m_positionData;
		std::vector<std::wstring> m_treeFileNames;
		const std::wstring m_signatureBase = L"BillTreeTextureArray";
		std::wstring m_textureArraySignature;
		static int m_signatureIndex;
		BillTreeRenderOption m_renderOptions;
		bool m_alphaToCoverage;

		bool m_initialized;
		bool m_loadingComplete;
	};
}