#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <ppltasks.h>
#include "Common/ShaderMgr.h"
#include "Common/TextureMgr.h"
#include "Common/RenderStateMgr.h"
#include "Common/GameTimer.h"
#include "Common/ConstantBuffer.h"
#include "Common/DeviceResources.h"
#include "MeshGeometry.h"


// Support "Normal", "Reflect", "NoTexture", "Texture".
namespace DXFramework
{
	struct MeshObjectData
	{
		MeshObjectData() : Skinned(false) {}

		bool Skinned;
		std::vector<DX::PosNormalTexTan> VertexData;
		std::vector<DX::PosNormalTexTanSkinned> VertexDataSkinned;
		std::vector<UINT> IndexData;
		std::vector<Subset> Subsets;
		std::vector<X3dMaterial> Material;
		SkinnedData SkinInfo;

		// Used for multi instance
		std::vector<DirectX::XMFLOAT4X4> Worlds;
		std::vector<std::wstring> ClipNames;
	};

	struct MeshFeatureConfigure
	{
		bool AlphaClip;
		bool Fog;
		bool Reflect;
		bool Shadow;
		bool Ssao;
		bool Loop;
		UINT LightCount;
		std::wstring ReflectFileName;
	};

	// Own several BasicElementData
	class MeshObject
	{
	public:
		MeshObject(
			const std::shared_ptr<DX::DeviceResources>& deviceResources,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB);
		~MeshObject();

		void Initialize(MeshObjectData* data, const MeshFeatureConfigure& feature, std::wstring textureDir = L"Media\\Meshes\\Textures\\", bool generateMips = false);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Update(float dt);
		void Render(bool recover = false);
		void DepthRender(bool recover = false);
		void NorDepRender(bool recover = false);

	public:
		void UpdateReflectMapSRV(ID3D11ShaderResourceView* srv);
		void UpdateShadowMapSRV(ID3D11ShaderResourceView* srv);
		void UpdateSsaoMapSRV(ID3D11ShaderResourceView* srv);
		void UpdateDiffuseMapSRV(int i, ID3D11ShaderResourceView* srv);
		void UpdateNormalMapSRV(int i, ID3D11ShaderResourceView* srv);

		void StartAnimation(int i) { m_timePositions[i] = 0.0f; }
		void StopAnimation(int i) { m_timePositions[i] = -1.0f; }
		void SetWorld(int i, const DirectX::XMFLOAT4X4& world) { m_object->Worlds[i] = world; }
		void SetClipName(int i, const std::wstring& clipName) { m_object->ClipNames[i] = clipName; m_timePositions[i] = -1.0f; }

		DirectX::XMFLOAT4X4 GetWorld(int i) { return m_object->Worlds[i]; }
		DirectX::BoundingBox GetOrgBoundingBox() { return m_boundingBox; }
		DirectX::BoundingSphere GetOrgBoundingSphere() { return m_boundingSphere; }
		DirectX::BoundingBox GetTransBoundingBox(int i);
		DirectX::BoundingSphere GetTransBoundingSphere(int i);

	private:
		concurrency::task<void> BuildDataAsync();

	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>> m_perObjectCB;

		// Direct3D data resources 
		std::unique_ptr<MeshObjectData> m_object;
		MeshFeatureConfigure m_feature;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_objectVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_objectIB;
		
		static bool m_resetFlag;
		static DX::ConstantBuffer<DX::SkinnedTransforms> m_skinnedCB;
		
		// Shaders
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_meshVS;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_meshVSNormal;
		std::vector<Microsoft::WRL::ComPtr<ID3D11PixelShader>>  m_meshPS;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_depthVS;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_depthVSSkinned;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_depthPSClip;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_norDepVS;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_norDepVSSkinned;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_norDepPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_norDepPSClip;

		// SRV
		std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_diffuseMapSRV;
		std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_norMapSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_reflectMapSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_depthMapSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ssaoMapSRV;

		// Custom data
		std::vector<std::vector<DirectX::XMFLOAT4X4>> m_finalTransforms;
		std::vector<float> m_timePositions;

		DirectX::BoundingBox m_boundingBox;
		DirectX::BoundingSphere m_boundingSphere;

		bool m_generateMips;
		bool m_initialized;
		bool m_loadingComplete;
	};
}