#include "pch.h"
#include "MeshObject.h"
#include <algorithm>
#include <vector>
#include "Common/DirectXHelper.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"

using namespace DXFramework;
using namespace Microsoft::WRL;
using namespace DirectX;

using namespace DX;

bool MeshObject::m_resetFlag = false;
DX::ConstantBuffer<DX::SkinnedTransforms> MeshObject::m_skinnedCB;

MeshObject::~MeshObject()
{
	if (!m_resetFlag)
	{
		m_resetFlag = true;
		m_skinnedCB.Reset();
	}
}


MeshObject::MeshObject(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB)
	: m_loadingComplete(false), m_initialized(false),
	m_deviceResources(deviceResources), m_perFrameCB(perFrameCB), m_perObjectCB(perObjectCB)
{
}

void MeshObject::Initialize(MeshObjectData* data, const MeshFeatureConfigure& feature, std::wstring textureDir /* = L"MediaMeshesTextures" */, bool generateMips /* = false */)
{
	m_object = std::unique_ptr<MeshObjectData>(data);
	m_feature = feature;

#ifdef _DEBUG
	// Data check
	if ((m_object->Skinned && m_object->VertexDataSkinned.size() == 0))
		throw ref new Platform::InvalidArgumentException("Lack necessary vertex input data!");
	if (m_object->Worlds.size() == 0)
		throw ref new Platform::InvalidArgumentException("Lack necessary mesh object data!");
	if (m_object->Skinned)
	{
		if (m_object->Worlds.size() != m_object->ClipNames.size())
			throw ref new Platform::InvalidArgumentException("Worlds and clip do not match!");
	}
#endif

	if (m_object->Skinned)
	{
		BoundingBox::CreateFromPoints(m_boundingBox, m_object->VertexDataSkinned.size(), &m_object->VertexDataSkinned[0].Pos, sizeof(PosNormalTexTanSkinned));
		BoundingSphere::CreateFromBoundingBox(m_boundingSphere, m_boundingBox);
	}
	else
	{
		BoundingBox::CreateFromPoints(m_boundingBox, m_object->VertexData.size(), &m_object->VertexData[0].Pos, sizeof(PosNormalTexTan));
		BoundingSphere::CreateFromBoundingBox(m_boundingSphere, m_boundingBox);
	}

	for (UINT i = 0; i < m_object->Material.size(); ++i)
	{
		auto& diffuse = m_object->Material[i].DiffuseMap;
		auto& normal = m_object->Material[i].NormalMap;
		if (diffuse != L"" && diffuse != L"Null")
			diffuse = textureDir + diffuse;
		if(normal != L"" && normal != L"Null")
			normal = textureDir + normal;
	}

	if (m_object->Skinned)
	{
		m_timePositions.resize(m_object->Worlds.size());
		m_timePositions.assign(m_timePositions.size(), -1.0f);
		m_finalTransforms.resize(m_object->Worlds.size());
		for (UINT i = 0; i < m_object->Worlds.size(); ++i)
		{
			m_finalTransforms[i].resize(m_object->SkinInfo.GetBoneCount());
			m_object->SkinInfo.GetFinalTransforms(m_object->ClipNames[i], 0.0f, m_finalTransforms[i]);
		}
	}
	
	m_generateMips = generateMips;
	m_initialized = true;
}

concurrency::task<void> MeshObject::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The mesh objects haven't been initialized!");
		return concurrency::task_from_result();
	}

	// Initialize constant buffer
	if(!m_skinnedCB.GetInitalizeState())
		m_skinnedCB.Initialize(m_deviceResources->GetD3DDevice());

	return BuildDataAsync()
		.then([=]()
	{
		m_loadingComplete = true;
	});
}

void MeshObject::Update(float dt)
{
	if (!m_initialized)
	{
		OutputDebugString(L"The objects haven't been initialized!");
		return;
	}

	if (!m_object->Skinned)
		return;

	for (UINT i = 0; i < m_finalTransforms.size(); ++i)
	{
		if (m_timePositions[i] < 0)
			continue;
		m_timePositions[i] += dt;
		// Loop animation
		if (m_timePositions[i] > m_object->SkinInfo.GetClipEndTime(m_object->ClipNames[i]))
			if (m_feature.Loop)
				m_timePositions[i] = 0.0f;
			else
				m_timePositions[i] = -1.0f;
		m_object->SkinInfo.GetFinalTransforms(m_object->ClipNames[i], m_timePositions[i], m_finalTransforms[i]);
	}	
}

void MeshObject::Render(bool recover /* = false */)
{
	if (!m_loadingComplete)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();
	// Set IA stage.
	UINT stride = m_object->Skinned ? sizeof(PosNormalTexTanSkinned) : sizeof(PosNormalTexTan);
	UINT offset = 0;
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_objectVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_objectIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	ID3D11Buffer* cbuffers0[2] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer() };
	ID3D11Buffer* cbuffers1[1] = { m_skinnedCB.GetBuffer() };
	ID3D11SamplerState* samplers[2] = { renderStateMgr->LinearSam(), renderStateMgr->ShadowSam() };
	ID3D11ShaderResourceView* srvs[3] = { m_depthMapSRV.Get(), m_ssaoMapSRV.Get(), m_reflectMapSRV.Get() };

	// Set constant buffers
	context->VSSetConstantBuffers(0, 2, cbuffers0);
	if (m_object->Skinned)
		context->VSSetConstantBuffers(3, 1, cbuffers1);
	context->PSSetConstantBuffers(0, 2, cbuffers0);

	// Set srvs & samplers
	context->PSSetShaderResources(2, 3, srvs);
	context->PSSetSamplers(0, 2, samplers);

	for (UINT i = 0; i < m_object->Worlds.size(); ++i)
	{
		// Update constant buffers
		XMMATRIX world = XMLoadFloat4x4(&m_object->Worlds[i]);
		XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
		XMStoreFloat4x4(&m_perObjectCB->Data.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&m_perObjectCB->Data.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
		XMStoreFloat4x4(&m_perObjectCB->Data.TexTransform, XMMatrixIdentity());
		if (m_object->Skinned)
		{
			for (UINT j = 0; j < m_object->SkinInfo.GetBoneCount(); ++j)
				m_skinnedCB.Data.BoneTransforms[j] = m_finalTransforms[i][j];
			m_skinnedCB.ApplyChanges(context);
		}

		// Iterate over each subSet. Each subSet is corespondent to one material of the same index.
		for (UINT j = 0; j < m_object->Subsets.size(); ++j)
		{
			Subset& item = m_object->Subsets[j];
			UINT index = item.MtlIndex;
			X3dMaterial& material = m_object->Material[index];
			// Set material
			m_perObjectCB->Data.Mat = material.Mat;
			m_perObjectCB->ApplyChanges(context);
			// Bind srv
			ID3D11ShaderResourceView* srvs[2] = { m_diffuseMapSRV[index].Get(), m_norMapSRV[index].Get() };
			context->PSSetShaderResources(0, 2, srvs);
			// Bind shaders
			if (material.Effect != EffectType::Normal)
			{
				if (ShaderChangement::VS != m_meshVS.Get())
				{
					context->VSSetShader(m_meshVS.Get(), nullptr, 0);
					ShaderChangement::VS = m_meshVS.Get();
				}
			}
			else
			{
				if (ShaderChangement::VS != m_meshVSNormal.Get())
				{
					context->VSSetShader(m_meshVSNormal.Get(), nullptr, 0);
					ShaderChangement::VS = m_meshVSNormal.Get();
				}
			}
			if (ShaderChangement::PS != m_meshPS[index].Get())
			{
				context->PSSetShader(m_meshPS[index].Get(), nullptr, 0);
				ShaderChangement::PS = m_meshPS[index].Get();
			}

			if (m_feature.AlphaClip)
			{
				if (ShaderChangement::RSS != renderStateMgr->NoCullRS())
				{
					context->RSSetState(renderStateMgr->NoCullRS());
					ShaderChangement::RSS = renderStateMgr->NoCullRS();
				}
			}
			else if(ShaderChangement::RSS != nullptr)
			{
				context->RSSetState(nullptr);
				ShaderChangement::RSS = nullptr;
			}

			context->DrawIndexed(item.IndexCount, item.IndexStart, item.VertexBase);
		}
	}

	// Recovery
	if (recover)
	{
		ID3D11ShaderResourceView* nullSRV[3] = { nullptr, nullptr, nullptr };
		context->PSSetShaderResources(2, 3, nullSRV);

		if (ShaderChangement::RSS != nullptr)
		{
			context->RSSetState(nullptr);
			ShaderChangement::RSS = nullptr;
		}
	}
}

void MeshObject::DepthRender(bool recover /* = false */)
{
	if (!m_loadingComplete || !m_feature.Shadow)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();
	// Set IA stage
	UINT stride = m_object->Skinned ? sizeof(PosNormalTexTanSkinned) : sizeof(PosNormalTexTan);
	UINT offset = 0;
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_objectVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_objectIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	ID3D11Buffer* cbuffers0[2] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer() };
	ID3D11Buffer* cbuffers1[1] = { m_skinnedCB.GetBuffer() };
	ID3D11SamplerState* samplers[1] = { renderStateMgr->LinearSam() };

	// Set constant buffers
	context->VSSetConstantBuffers(0, 2, cbuffers0);
	if (m_object->Skinned)
		context->VSSetConstantBuffers(3, 1, cbuffers1);
	context->PSSetConstantBuffers(0, 2, cbuffers0);

	// Set srvs & samplers
	context->PSSetSamplers(0, 1, samplers);

	// vs
	if (m_object->Skinned)
	{
		if (ShaderChangement::VS != m_depthVSSkinned.Get())
		{
			context->VSSetShader(m_depthVSSkinned.Get(), nullptr, 0);
			ShaderChangement::VS = m_depthVSSkinned.Get();
		}
	}
	else
	{
		if (ShaderChangement::VS != m_depthVS.Get())
		{
			context->VSSetShader(m_depthVS.Get(), nullptr, 0);
			ShaderChangement::VS = m_depthVS.Get();
		}
	}

	for (UINT i = 0; i < m_object->Worlds.size(); ++i)
	{
		// Set world
		XMMATRIX world = XMLoadFloat4x4(&m_object->Worlds[i]);
		XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
		XMStoreFloat4x4(&m_perObjectCB->Data.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&m_perObjectCB->Data.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
		XMStoreFloat4x4(&m_perObjectCB->Data.TexTransform, XMMatrixIdentity());
		m_perObjectCB->ApplyChanges(context);
		if (m_object->Skinned)
		{
			for (UINT j = 0; j < m_object->SkinInfo.GetBoneCount(); ++j)
				m_skinnedCB.Data.BoneTransforms[j] = m_finalTransforms[i][j];
			m_skinnedCB.ApplyChanges(context);
		}

		// Iterate over each subSet. Each subSet is corespondent to one material of the same index.
		for (UINT j = 0; j < m_object->Subsets.size(); ++j)
		{
			Subset& item = m_object->Subsets[j];
			UINT index = item.MtlIndex;
			X3dMaterial& material = m_object->Material[index];

			if (m_feature.AlphaClip)
			{
				context->PSSetShaderResources(0, 1, m_diffuseMapSRV[i].GetAddressOf());
				if (ShaderChangement::PS != m_depthPSClip.Get())
				{
					context->PSSetShader(m_depthPSClip.Get(), nullptr, 0);
					ShaderChangement::PS = m_depthPSClip.Get();
				}
				if (ShaderChangement::RSS != renderStateMgr->DepthBiasNoCullRS())
				{
					context->RSSetState(renderStateMgr->DepthBiasNoCullRS());
					ShaderChangement::RSS = renderStateMgr->DepthBiasNoCullRS();
				}
			}
			else
			{
				context->PSSetShader(nullptr, nullptr, 0);
				ShaderChangement::PS = nullptr;
				if (ShaderChangement::RSS != renderStateMgr->DepthBiasRS())
				{
					context->RSSetState(renderStateMgr->DepthBiasRS());
					ShaderChangement::RSS = renderStateMgr->DepthBiasRS();
				}
			}

			context->DrawIndexed(item.IndexCount, item.IndexStart, item.VertexBase);
		}
	}

	// Recovery
	if (recover)
	{
		context->RSSetState(nullptr);
	}
}

void MeshObject::NorDepRender(bool recover /* = false */)
{
	if (!m_loadingComplete || !m_feature.Ssao)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();
	// Set IA stage.
	UINT stride = m_object->Skinned ? sizeof(PosNormalTexTanSkinned) : sizeof(PosNormalTexTan);
	UINT offset = 0;
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_objectVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_objectIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	ID3D11Buffer* cbuffers0[2] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer() };
	ID3D11Buffer* cbuffers1[1] = { m_skinnedCB.GetBuffer() };
	ID3D11SamplerState* samplers[1] = { renderStateMgr->LinearSam() };

	// Set constant buffers
	context->VSSetConstantBuffers(0, 2, cbuffers0);
	if (m_object->Skinned)
		context->VSSetConstantBuffers(3, 1, cbuffers1);
	context->PSSetConstantBuffers(0, 2, cbuffers0);

	// Set srvs & samplers
	context->PSSetSamplers(0, 1, samplers);

	// vs
	if (m_object->Skinned)
	{
		if (ShaderChangement::VS != m_norDepVSSkinned.Get())
		{
			context->VSSetShader(m_norDepVSSkinned.Get(), nullptr, 0);
			ShaderChangement::VS = m_norDepVSSkinned.Get();
		}
	}
	else
	{
		if (ShaderChangement::VS != m_norDepVS.Get())
		{
			context->VSSetShader(m_norDepVS.Get(), nullptr, 0);
			ShaderChangement::VS = m_norDepVS.Get();
		}
	}

	for (UINT i = 0; i < m_object->Worlds.size(); ++i)
	{
		// Set world
		XMMATRIX world = XMLoadFloat4x4(&m_object->Worlds[i]);
		XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
		XMStoreFloat4x4(&m_perObjectCB->Data.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&m_perObjectCB->Data.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
		XMStoreFloat4x4(&m_perObjectCB->Data.TexTransform, XMMatrixIdentity());
		m_perObjectCB->ApplyChanges(context);
		if (m_object->Skinned)
		{
			for (UINT j = 0; j < m_object->SkinInfo.GetBoneCount(); ++j)
				m_skinnedCB.Data.BoneTransforms[j] = m_finalTransforms[i][j];
			m_skinnedCB.ApplyChanges(context);
		}

		// Iterate over each subSet. Each subSet is corespondent to one material of the same index.
		for (UINT j = 0; j < m_object->Subsets.size(); ++j)
		{
			Subset& item = m_object->Subsets[j];
			UINT index = item.MtlIndex;
			X3dMaterial& material = m_object->Material[index];

			if (m_feature.AlphaClip)
			{
				context->PSSetShaderResources(0, 1, m_diffuseMapSRV[index].GetAddressOf());
				if (ShaderChangement::PS != m_norDepPSClip.Get())
				{
					context->PSSetShader(m_norDepPSClip.Get(), nullptr, 0);
					ShaderChangement::PS = m_norDepPSClip.Get();
				}
				if (ShaderChangement::RSS != renderStateMgr->NoCullRS())
				{
					context->RSSetState(renderStateMgr->NoCullRS());
					ShaderChangement::RSS = renderStateMgr->NoCullRS();
				}
			}
			else 
			{
				if (ShaderChangement::PS != m_norDepPS.Get())
				{
					context->PSSetShader(m_norDepPS.Get(), nullptr, 0);
					ShaderChangement::PS = m_norDepPS.Get();
				}
				if (ShaderChangement::RSS != nullptr)
				{
					context->RSSetState(nullptr);
					ShaderChangement::RSS = nullptr;
				}
			}

			context->DrawIndexed(item.IndexCount, item.IndexStart, item.VertexBase);
		}
	}

	if (recover && ShaderChangement::RSS != nullptr)
	{
		context->RSSetState(nullptr);
		ShaderChangement::RSS = nullptr;
	}
}

void MeshObject::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	// Direct3D data resources 
	m_objectVB.Reset();
	m_objectIB.Reset();
	m_skinnedCB.Reset();

	// Shaders
	m_inputLayout.Reset();
	m_meshVS.Reset();
	m_meshVSNormal.Reset();
	m_meshPS.clear();
	m_depthVS.Reset();
	m_depthPSClip.Reset();
	m_norDepVS.Reset();
	m_norDepPS.Reset();
	m_norDepPSClip.Reset();

	// SRV
	m_diffuseMapSRV.clear();
	m_norMapSRV.clear();
	m_reflectMapSRV.Reset();
	m_depthMapSRV.Reset();
	m_ssaoMapSRV.Reset();
}

concurrency::task<void> MeshObject::BuildDataAsync()
{
	assert(IsMainThread());

	auto shaderMgr = ShaderMgr::Instance();
	auto textureMgr = TextureMgr::Instance();

	std::vector<concurrency::task<void>> CreateTasks;
	std::vector<std::wstring> fileCache;
	std::map<UINT, std::wstring> psAd;
	std::map<UINT, std::wstring> diffuseAd;
	std::map<UINT, std::wstring> normalAd;
	bool cacheFlag;

	// VS
	std::wstring shaderName = L"BasicVS";
	InputLayoutType inputLayoutType = m_object->Skinned ? InputLayoutType::PosNormalTexTanSkinned : InputLayoutType::PosNormalTexTan;
	shaderName += L'0';
	shaderName += L'0';
	shaderName += m_feature.Shadow ? L'1' : L'0';
	shaderName += m_feature.Ssao ? L'1' : L'0';
	shaderName += m_object->Skinned ? L'1' : L'0';
	shaderName += L".cso";
	CreateTasks.push_back(shaderMgr->GetVSAsync(shaderName, InputLayoutType::None)
		.then([=](ID3D11VertexShader* vs) { m_meshVS = vs; }));
	shaderName[7] = L'1';
	CreateTasks.push_back(shaderMgr->GetVSAsync(shaderName, inputLayoutType)
		.then([=](ID3D11VertexShader* vs)
	{ 
		m_meshVSNormal = vs; 
		m_inputLayout = shaderMgr->GetInputLayout(inputLayoutType);
	}));
	// ps & srv
	m_meshPS.resize(m_object->Material.size());
	m_diffuseMapSRV.resize(m_object->Material.size());
	m_norMapSRV.resize(m_object->Material.size());
	for (UINT i = 0; i < m_object->Material.size(); ++i)
	{
		auto& material = m_object->Material[i];
		shaderName = L"BasicPS";
		shaderName += material.Effect == EffectType::NoTexture ? L'0' : L'1';
		shaderName += m_feature.AlphaClip ? L'1' : L'0';
		shaderName += material.Effect == EffectType::Normal ? L'1' : L'0';
		shaderName += m_feature.Shadow ? L'1' : L'0';
		shaderName += m_feature.Ssao ? L'1' : L'0';
		shaderName += (L'0' + m_feature.LightCount);
		shaderName += m_feature.Reflect ? L'1' : L'0';
		shaderName += m_feature.Fog ? L'1' : L'0';
		shaderName += L".cso";

		cacheFlag = false;
		for (auto& file : fileCache)
			if (file == shaderName)
			{
				cacheFlag = true;
				psAd[i] = shaderName;
				break;
			}
		if (!cacheFlag)
		{
			CreateTasks.push_back(shaderMgr->GetPSAsync(shaderName)
				.then([=](ID3D11PixelShader* ps) { m_meshPS[i] = ps; }));
			fileCache.push_back(shaderName);
		}
			
		if (material.DiffuseMap == L"" || material.DiffuseMap == L"Null")
			m_diffuseMapSRV.push_back(nullptr);
		else
		{
			cacheFlag = false;
			for (auto& file : fileCache)
				if (file == material.DiffuseMap)
				{
					cacheFlag = true;
					diffuseAd[i] = material.DiffuseMap;
					break;
				}
			if(!cacheFlag)
			{
				CreateTasks.push_back(textureMgr->GetTextureAsync(material.DiffuseMap)
					.then([=](ID3D11ShaderResourceView* srv)
				{
					if (m_generateMips)
						m_deviceResources->GetD3DDeviceContext()->GenerateMips(srv);
					m_diffuseMapSRV[i] = srv; 
				}));
				fileCache.push_back(material.DiffuseMap);
			}
		}
		if (material.NormalMap == L"" || material.NormalMap == L"Null")
			m_norMapSRV.push_back(nullptr);
		else
		{
			cacheFlag = false;
			for (auto& file : fileCache)
				if (file == material.NormalMap)
				{
					cacheFlag = true;
					normalAd[i] = material.NormalMap;
					break;
				}
			if (!cacheFlag)
			{
				CreateTasks.push_back(textureMgr->GetTextureAsync(material.NormalMap)
					.then([=](ID3D11ShaderResourceView* srv) 
				{
					if (m_generateMips)
						m_deviceResources->GetD3DDeviceContext()->GenerateMips(srv);
					m_norMapSRV[i] = srv; 
				}));
				fileCache.push_back(material.NormalMap);
			}
		}
	}

	// Shadow -- get depth
	if (m_feature.Shadow)
	{
		if (m_object->Skinned)
		{
			CreateTasks.push_back(shaderMgr->GetVSAsync(L"GetDepthVSSkinned.cso", InputLayoutType::None)
				.then([=](ID3D11VertexShader* vs) { m_depthVSSkinned = vs; }));
		}
		CreateTasks.push_back(shaderMgr->GetVSAsync(L"GetDepthVS.cso", InputLayoutType::None)
			.then([=](ID3D11VertexShader* vs) { m_depthVS = vs; }));
		CreateTasks.push_back(shaderMgr->GetPSAsync(L"GetDepthPSClip.cso")
			.then([=](ID3D11PixelShader* ps) { m_depthPSClip = ps; }));
	}
	// Ssao -- get normal and depth
	if (m_feature.Ssao)
	{
		if (m_object->Skinned)
		{
			CreateTasks.push_back(shaderMgr->GetVSAsync(L"GetNorDepVSSkinned.cso", InputLayoutType::None)
				.then([=](ID3D11VertexShader* vs) { m_norDepVSSkinned = vs; }));
		}
		CreateTasks.push_back(shaderMgr->GetVSAsync(L"GetNorDepVS.cso", InputLayoutType::None)
			.then([=](ID3D11VertexShader* vs) { m_norDepVS = vs; }));
		CreateTasks.push_back(shaderMgr->GetPSAsync(L"GetNorDepPS.cso")
			.then([=](ID3D11PixelShader* ps) { m_norDepPS = ps; }));
		CreateTasks.push_back(shaderMgr->GetPSAsync(L"GetNorDepPSClip.cso")
			.then([=](ID3D11PixelShader* ps) { m_norDepPSClip = ps; }));
	}
	// Reflect map SRV
	if (m_feature.ReflectFileName != L"")
	{
		CreateTasks.push_back(textureMgr->GetTextureAsync(m_feature.ReflectFileName)
			.then([=](ID3D11ShaderResourceView* srv) { m_reflectMapSRV = srv; }));
	}

	return concurrency::when_all(CreateTasks.begin(), CreateTasks.end())
		.then([=]()
	{
		// Create cached files
		for (auto& item : psAd)
			m_meshPS[item.first] = shaderMgr->GetPS(item.second);
		for (auto& item : diffuseAd)
			m_diffuseMapSRV[item.first] = textureMgr->GetTexture(item.second);
		for (auto& item : normalAd)
			m_norMapSRV[item.first] = textureMgr->GetTexture(item.second);

		// Create VB and IB
		D3D11_BUFFER_DESC vbd;
		D3D11_SUBRESOURCE_DATA vinitData;
		vbd.Usage = D3D11_USAGE_IMMUTABLE;
		if (m_object->Skinned)
		{
			vbd.ByteWidth = sizeof(PosNormalTexTanSkinned) * m_object->VertexDataSkinned.size();
			vinitData.pSysMem = &m_object->VertexDataSkinned[0];
		}
		else
		{
			vbd.ByteWidth = sizeof(PosNormalTexTan) * m_object->VertexData.size();
			vinitData.pSysMem = &m_object->VertexData[0];
		}
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = 0;
		vbd.MiscFlags = 0;
		ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, &vinitData, m_objectVB.GetAddressOf()));

		D3D11_BUFFER_DESC ibd;
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.ByteWidth = sizeof(UINT) * m_object->IndexData.size();
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.CPUAccessFlags = 0;
		ibd.MiscFlags = 0;
		D3D11_SUBRESOURCE_DATA iinitData;
		iinitData.pSysMem = &m_object->IndexData[0];
		ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&ibd, &iinitData, m_objectIB.GetAddressOf()));
	});
}

void MeshObject::UpdateReflectMapSRV(ID3D11ShaderResourceView* srv)
{
	if (!m_feature.Reflect) return;
	m_reflectMapSRV = srv;
}

void MeshObject::UpdateShadowMapSRV(ID3D11ShaderResourceView* srv)
{
	if (!m_feature.Shadow) return;
	m_depthMapSRV = srv;
}

void MeshObject::UpdateSsaoMapSRV(ID3D11ShaderResourceView* srv)
{
	if (!m_feature.Ssao) return;
	m_ssaoMapSRV = srv;
}

void MeshObject::UpdateDiffuseMapSRV(int i, ID3D11ShaderResourceView* srv)
{
	m_diffuseMapSRV[i] = srv;
}

void MeshObject::UpdateNormalMapSRV(int i, ID3D11ShaderResourceView* srv)
{
	m_norMapSRV[i] = srv;
}

BoundingBox MeshObject::GetTransBoundingBox(int i)
{
	BoundingBox res;
	m_boundingBox.Transform(res, XMLoadFloat4x4(&m_object->Worlds[i]));
	return res;
}

BoundingSphere MeshObject::GetTransBoundingSphere(int i)
{
	BoundingSphere res;
	m_boundingSphere.Transform(res, XMLoadFloat4x4(&m_object->Worlds[i]));
	return res;
}