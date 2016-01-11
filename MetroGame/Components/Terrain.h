#pragma once

#include <DirectXMath.h>
#include <ppltasks.h>
#include "Common/ShaderMgr.h"
#include "Common/TextureMgr.h"
#include "Common/GameTimer.h"
#include "Common/ConstantBuffer.h"
#include "Common/DeviceResources.h"

// Using height-map method to simulate a terrain. The terrain data is directly
// set in the world coordinates.

namespace DXFramework
{
	enum class TerrainRenderOption
	{
		Light3,
		Light3Tex,
		Light3TexFog
	};

	struct TerrainInitInfo
	{
		TerrainInitInfo() : HeightScale(10.0f), HeightmapWidth(0), HeightmapHeight(0),
			CellSpacing(0.5f), MaxDist(500.0f), MaxTess(6.0f), MinDist(20.0f), MinTess(0.0f)
		{
			TexScale = DirectX::XMFLOAT2(50.0f, 50.0f);
		}

		std::wstring HeightMapFilename;
		std::wstring LayerMapFilename0;
		std::wstring LayerMapFilename1;
		std::wstring LayerMapFilename2;
		std::wstring LayerMapFilename3;
		std::wstring LayerMapFilename4;
		std::wstring BlendMapFilename;
		float HeightScale;
		UINT HeightmapWidth;	// Vertices number
		UINT HeightmapHeight;	// Vertices number
		float CellSpacing;

		DirectX::XMFLOAT2 TexScale;

		float MinDist;
		float MaxDist;
		// Exponents for power of 2 tessellation.  The tessellation
		// range is [2^(gMinTess), 2^(gMaxTess)].  Since the maximum
		// tessellation is 64, this means gMaxTess can be at most 6
		// since 2^6 = 64.
		float MinTess;
		float MaxTess;
	};

	class Terrain
	{	
	public:
		Terrain(const std::shared_ptr<DX::DeviceResources>& deviceResources,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB);

		void Initialize(const TerrainInitInfo& initInfo);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Render();

	public:
		// Configure functions
		void SetMaterial(const DX::Material& mat) { m_terrainMat = mat; }
		void SetRenderOption(TerrainRenderOption r){ m_renderOptions = r; }

		float GetWidth()const{ return (m_initInfo.HeightmapWidth - 1)*m_initInfo.CellSpacing; }
		float GetDepth()const{ return (m_initInfo.HeightmapHeight - 1)*m_initInfo.CellSpacing; }
		std::wstring GetTextureArraySignature() { return m_textureArraySignature; };
		float GetHeight(float x, float z)const;
		DirectX::XMVECTOR GetNormal(float x, float z)const;
		
	private:
		void LoadHeightmap();
		void Smooth();
		bool InBounds(int i, int j);
		float Average(int i, int j);
		void CalcAllPatchBoundsY();
		void CalcPatchBoundsY(UINT i, UINT j);
		void CalcAllNormal();
		void CalcNormal(UINT i, UINT j);
		void BuildQuadPatchVB();
		void BuildQuadPatchIB();
		void BuildHeightmapSRV();

	private:
		struct TerrainSettingsCB
		{
			DirectX::XMFLOAT2 TexScale;
			// When distance is minimum, the tessellation is maximum.
			// When distance is maximum, the tessellation is minimum.
			float MinDist;
			float MaxDist;
			// Exponents for power of 2 tessellation.  The tessellation
			// range is [2^(gMinTess), 2^(gMaxTess)].  Since the maximum
			// tessellation is 64, this means gMaxTess can be at most 6
			// since 2^6 = 64.
			float MinTess;
			float MaxTess;

			float TexelCellSpaceU;
			float TexelCellSpaceV;
			float WorldCellSpace;
		};
		struct FrustumCB
		{
			DirectX::XMFLOAT4 WorldFrustumPlanes[6];
		};
		
	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>> m_perObjectCB;

		// Direct3D data resources 
		DX::ConstantBuffer<TerrainSettingsCB> m_terrainSettingsCB;
		DX::ConstantBuffer<FrustumCB> m_frustumCB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_quadPatchVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_quadPatchIB;

		//Shaders
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_terrainInputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_terrainVS;
		Microsoft::WRL::ComPtr<ID3D11HullShader> m_terrainHS;
		Microsoft::WRL::ComPtr<ID3D11DomainShader> m_terrainDS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_terrainLight3PS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_terrainLight3TexPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_terrainLight3TexFogPS;

		// SRV
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_layerMapArraySRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_blendMapSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_heightMapSRV;

		// Custom data
		// Divide heightmap into patches such that each patch has CellsPerPatch cells
		// and CellsPerPatch+1 vertices.  Use 64 so that if we tessellate all the way 
		// to 64, we use all the data from the heightmap.  
		static const int CellsPerPatch = 64;
		TerrainInitInfo m_initInfo;
		UINT m_numPatchVertices;
		UINT m_numPatchQuadFaces;
		UINT m_numPatchVertRows;
		UINT m_numPatchVertCols;
		DX::Material m_terrainMat;

		TerrainRenderOption m_renderOptions;
		std::vector<DirectX::XMFLOAT2> m_patchBoundsY;
		std::vector<DirectX::XMFLOAT3> m_normal;
		std::vector<float> m_heightmap;
		const std::wstring m_signatureBase = L"TerrainLayerTextureArray";
		std::wstring m_textureArraySignature;
		static int m_signatureIndex;

		bool m_initialized;
		bool m_loadingComplete;
	};
}


