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


// Manage basic objects which takes "DX::Basic32" as the input data structure.
// These basic objects share the same shaders.
// It owns multi vertex buffer and index buffer. You can also pack different objects' data into one buffer.
// This class has a two-layer data access control method.

namespace DXFramework
{
	// Represent for one specific element
	struct BasicElementUnit
	{
		BasicElementUnit() : Count(0), Start(0), Base(0), VCount(0),
			TextureStepRate(1), MaterialStepRate(1), 
			TextureTransformStepRate(1), NorTextureStepRate(1)
		{}

		UINT Count;		// Index count
		UINT Start;		// Index start
		UINT Base;		// Only vertex base 
		UINT VCount;	// Only vertex count

		std::vector<DirectX::XMFLOAT4X4> Worlds;
		std::vector<std::wstring> TextureFileNames;
		UINT TextureStepRate;
		std::vector<DX::Material> Material;
		UINT MaterialStepRate;
		std::vector<DirectX::XMFLOAT4X4> TextureTransform;
		UINT TextureTransformStepRate;
		std::vector<std::wstring> NorTextureFileNames;
		UINT NorTextureStepRate;
	};
	// Manage one vertex buffer and the accordingly index buffer
	struct BasicObjectData
	{
		BasicObjectData() : UseIndex(true), UseEx(false) {}

		bool UseEx;
		bool UseIndex;
		std::vector<DX::Basic32> VertexData;
		std::vector<DX::PosNormalTexTan> VertexDataEx;
		std::vector<UINT> IndexData;
		std::vector<BasicElementUnit> Units;
	};

	struct BasicFeatureConfigure
	{
		bool TextureEnable;
		bool ClipEnable;
		bool NormalEnable;
		bool ShadowEnable;
		bool SsaoEnable;
		int LightCount;
		bool ReflectEnable;
		bool FogEnable;
		bool TessEnable;
		bool Enhance;
		DX::BasicTessSettings TessDesc;
		std::wstring ReflectFileName;
	};

	// Own several BasicElementData
	class BasicObject
	{
	public:
		BasicObject(
			const std::shared_ptr<DX::DeviceResources>& deviceResources,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
			const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB);

		void Initialize(BasicObjectData* data, BasicFeatureConfigure feature);
		concurrency::task<void> CreateDeviceDependentResourcesAsync();
		void ReleaseDeviceDependentResources();
		void Render(bool recover = false);
		void DepthRender(bool recover = false);
		void NorDepRender(bool recover = false);

	public:
		// Config functions
		concurrency::task<void> UpdateFeatureAsync(BasicFeatureConfigure feature);

		void UpdateReflectMapSRV(ID3D11ShaderResourceView* srv);
		void UpdateShadowMapSRV(ID3D11ShaderResourceView* srv);
		void UpdateSsaoMapSRV(ID3D11ShaderResourceView* srv);
		void UpdateDiffuseMapSRV(int i, ID3D11ShaderResourceView* srv);
		void UpdateNormalMapSRV(int i, ID3D11ShaderResourceView* srv);

		void SetWorld(int i, int j, const DirectX::XMFLOAT4X4& world) { m_object->Units[i].Worlds[j] = world; }
		void SetMaterial(int i, int j, const DX::Material& mat) { m_object->Units[i].Material[j] = mat; }
		void SetTexTranform(int i, int j, const DirectX::XMFLOAT4X4& transform) { m_object->Units[i].TextureTransform[j] = transform; }

		DirectX::XMFLOAT4X4 GetWorld(int i, int j) { return m_object->Units[i].Worlds[j]; }
		DirectX::BoundingBox GetOrgBoundingBox(int i) { return m_boundingBox[i]; }
		DirectX::BoundingSphere GetOrgBoundingSphere(int i) { return m_boundingSphere[i]; }
		DirectX::BoundingBox GetTransBoundingBox(int i, int j = 0);
		DirectX::BoundingSphere GetTransBoundingSphere(int i, int j = 0);

	private:
		concurrency::task<void> BuildDataAsync();
		concurrency::task<void> LoadFeatureAsync(const BasicFeatureConfigure& feature);

	private:
		// Cached pointer to shared resources
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>> m_perObjectCB;
		
		// Direct3D data resources 
		std::unique_ptr<BasicObjectData> m_object;
		BasicFeatureConfigure m_feature;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_objectVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_objectIB;
		DX::ConstantBuffer<DX::BasicTessSettings> m_tessSettingsCB;

		// Shaders
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_basicVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_basicPS;
		Microsoft::WRL::ComPtr<ID3D11HullShader> m_basicHS;
		Microsoft::WRL::ComPtr<ID3D11DomainShader> m_basicDS;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_depthVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_depthPS;
		Microsoft::WRL::ComPtr<ID3D11HullShader> m_depthHS;
		Microsoft::WRL::ComPtr<ID3D11DomainShader> m_depthDS;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_norDepVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_norDepPS;
		Microsoft::WRL::ComPtr<ID3D11HullShader> m_norDepHS;
		Microsoft::WRL::ComPtr<ID3D11DomainShader> m_norDepDS;

		// SRV
		std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_diffuseMapSRV;
		std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_norMapSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_reflectMapSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_depthMapSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ssaoMapSRV;

		// Custom data
		bool m_texture;
		bool m_normal;

		std::vector<DirectX::BoundingBox> m_boundingBox;
		std::vector<DirectX::BoundingSphere> m_boundingSphere;

		bool m_initialized;
		bool m_loadingComplete;
	};
}