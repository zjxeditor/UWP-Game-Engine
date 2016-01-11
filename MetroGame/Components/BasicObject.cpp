#include "pch.h"
#include "BasicObject.h"
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

BasicObject::BasicObject(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB)
	: m_loadingComplete(false), m_initialized(false),
	m_deviceResources(deviceResources), m_perFrameCB(perFrameCB), m_perObjectCB(perObjectCB)
{
}

void BasicObject::Initialize(BasicObjectData* data, BasicFeatureConfigure feature)
{
	m_object = std::unique_ptr<BasicObjectData>(data);
	m_feature = feature;

#ifdef _DEBUG
	// Data check
	if ((m_object->UseEx && m_object->VertexDataEx.size() == 0)
		|| (!m_object->UseEx && m_object->VertexData.size() == 0)
		|| (m_feature.NormalEnable && !m_object->UseEx))
		throw ref new Platform::InvalidArgumentException("Lack necessary vertex input data!");
#endif

	m_boundingBox.resize(m_object->Units.size());
	m_boundingSphere.resize(m_object->Units.size());
	if (m_object->UseEx)
	{
		for (UINT i = 0; i < m_object->Units.size(); ++i)
		{
			auto& item = m_object->Units[i];
			BoundingBox::CreateFromPoints(m_boundingBox[i], item.VCount, &m_object->VertexDataEx[item.Base].Pos, sizeof(PosNormalTexTan));
			BoundingSphere::CreateFromBoundingBox(m_boundingSphere[i], m_boundingBox[i]);
		}
	}
	else
	{
		for (UINT i = 0; i < m_object->Units.size(); ++i)
		{
			auto& item = m_object->Units[i];
			BoundingBox::CreateFromPoints(m_boundingBox[i], item.VCount, &m_object->VertexData[item.Base].Pos, sizeof(Basic32));
			BoundingSphere::CreateFromBoundingBox(m_boundingSphere[i], m_boundingBox[i]);
		}
	}

	m_texture = true;
	m_normal = true;
	auto& units = m_object->Units;
	for (size_t i = 0; i < units.size(); ++i)
	{
		UINT Len = units[i].Worlds.size();
		if (Len != units[i].Material.size() * units[i].MaterialStepRate)
			throw ref new Platform::InvalidArgumentException("unit length does bot match!");
		UINT texLen = units[i].TextureFileNames.size() * units[i].TextureStepRate;
		UINT transLen = units[i].TextureTransform.size() * units[i].TextureTransformStepRate;
		UINT norLen = units[i].NorTextureFileNames.size() * units[i].NorTextureStepRate;
		if (transLen == 0)
		{
			if (texLen != 0 || norLen != 0)
				throw ref new Platform::InvalidArgumentException("unit length does bot match!");
			m_texture = false;
			m_normal = false;
		}
		else
		{
			if (transLen != Len)
				throw ref new Platform::InvalidArgumentException("unit length does bot match!");
			if (texLen == 0)
				m_texture = false;
			else if (texLen != Len)
				throw ref new Platform::InvalidArgumentException("unit length does bot match!");
			if (norLen == 0)
				m_normal = false;
			else if (norLen != Len)
				throw ref new Platform::InvalidArgumentException("unit length does bot match!");
		}
	}

#ifdef _DEBUG
	// Feature dependency check
	if (!m_texture && m_feature.TextureEnable)
		throw ref new Platform::InvalidArgumentException("Lack necessary texture input data!");
	if (!m_normal && m_feature.NormalEnable)
		throw ref new Platform::InvalidArgumentException("Lack necessary texture input data!");
	if (!m_feature.TextureEnable && (m_feature.ClipEnable || m_feature.NormalEnable))
		throw ref new Platform::InvalidArgumentException("Alpha clip feature or normal feature is dependent on texture feature!");
	if (!m_feature.NormalEnable && m_feature.TessEnable)
		throw ref new Platform::InvalidArgumentException("Tess feature is dependent on normal feature!");
	if (!m_feature.ShadowEnable && m_feature.SsaoEnable)
		throw ref new Platform::InvalidArgumentException("Ssao feature is dependent on shadow feature!");
	if (!m_feature.TessEnable && m_feature.Enhance)
		throw ref new Platform::InvalidArgumentException("Enhance feature is dependent on tess feature!");
#endif

	m_initialized = true;
}

concurrency::task<void> BasicObject::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The objects haven't been initialized!");
		return concurrency::task_from_result();
	}

	m_tessSettingsCB.Initialize(m_deviceResources->GetD3DDevice());

	return LoadFeatureAsync(m_feature)
		.then([=]()
	{
		return BuildDataAsync();
	}, concurrency::task_continuation_context::use_current())
		.then([=]()
	{
		m_loadingComplete = true;
	});
}

void BasicObject::Render(bool recover /* = false */)
{
	if (!m_loadingComplete)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();
	// Set IA stage.
	UINT stride = m_object->UseEx ? sizeof(PosNormalTexTan) : sizeof(Basic32);
	UINT offset = 0;
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	if (m_feature.TessEnable)
	{
		if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST)
		{
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
			ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
		}
	}
	else
	{
		if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
		{
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_objectVB.GetAddressOf(), &stride, &offset);
	if (m_object->UseIndex)
	{
		context->IASetIndexBuffer(m_objectIB.Get(), DXGI_FORMAT_R32_UINT, 0);
	}

	// Bind shaders, constant buffers, srvs and samplers
	ID3D11Buffer* cbuffers[3] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer(), m_tessSettingsCB.GetBuffer() };
	ID3D11Buffer* cbuffersT[2] = { cbuffers[0], cbuffers[2] };
	ID3D11SamplerState* samplers[2] = { renderStateMgr->LinearSam(), renderStateMgr->ShadowSam() };
	ID3D11ShaderResourceView* srvs[3] = { m_depthMapSRV.Get(), m_ssaoMapSRV.Get(),  m_reflectMapSRV.Get() };
	
	// vs
	if (ShaderChangement::VS != m_basicVS.Get())
	{
		context->VSSetShader(m_basicVS.Get(), nullptr, 0);
		ShaderChangement::VS = m_basicVS.Get();
	}
	if (m_feature.TessEnable)
		context->VSSetConstantBuffers(0, 3, cbuffers);
	else
		context->VSSetConstantBuffers(0, 2, cbuffers);
	// ps
	if (ShaderChangement::PS != m_basicPS.Get())
	{
		context->PSSetShader(m_basicPS.Get(), nullptr, 0);
		ShaderChangement::PS = m_basicPS.Get();
	}
	context->PSSetConstantBuffers(0, 2, cbuffers);
	context->PSSetShaderResources(2, 3, srvs);
	context->PSSetSamplers(0, 2, samplers);

	// hs and ds
	if (m_feature.TessEnable)
	{
		if (ShaderChangement::HS != m_basicHS.Get())
		{
			context->HSSetShader(m_basicHS.Get(), nullptr, 0);
			ShaderChangement::HS = m_basicHS.Get();
		}
		if (ShaderChangement::DS != m_basicDS.Get())
		{
			context->DSSetShader(m_basicDS.Get(), nullptr, 0);
			ShaderChangement::DS = m_basicDS.Get();
		}
		context->DSSetConstantBuffers(0, 2, cbuffersT);
		context->DSSetSamplers(0, 1, samplers);
	}
	else
	{
		if (ShaderChangement::HS != nullptr)
		{
			context->HSSetShader(nullptr, nullptr, 0);
			ShaderChangement::HS = nullptr;
		}
		if (ShaderChangement::DS != nullptr)
		{
			context->DSSetShader(nullptr, nullptr, 0);
			ShaderChangement::DS =nullptr;
		}
	}

	if (m_feature.ClipEnable)
	{
		if (ShaderChangement::RSS != renderStateMgr->NoCullRS())
		{
			context->RSSetState(renderStateMgr->NoCullRS());
			ShaderChangement::RSS = renderStateMgr->NoCullRS();
		}
	}
	else if (ShaderChangement::RSS != nullptr)
	{
		context->RSSetState(nullptr);
		ShaderChangement::RSS = nullptr;
	}

	// Iterate over each unit
	int totalNum, matBase, matInc, transBase, transInc, texBase, texInc, norBase, norInc;
	texBase = norBase = 0;
	if (m_feature.TextureEnable)
	{
		texInc = transInc = 0;
		if (m_feature.NormalEnable)
			norInc = 0;
		else
			norInc = -1;
	}
	else
		texInc = norInc = transInc = -1;
	for (size_t i = 0; i < m_object->Units.size(); ++i)
	{
		BasicElementUnit& item = m_object->Units[i];

		totalNum = (int)item.Worlds.size();
		matBase = matInc = transBase = 0;
		if (texInc > 0) texInc = 0;
		if (norInc > 0) norInc = 0;
		if (transInc > 0) transInc = 0;

		for (int k = 0; k < totalNum; ++k)
		{
			// Set world
			XMMATRIX world = XMLoadFloat4x4(&item.Worlds[k]);
			XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
			XMStoreFloat4x4(&m_perObjectCB->Data.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&m_perObjectCB->Data.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
			// Set material
			if (matInc == 0)
				m_perObjectCB->Data.Mat = item.Material[matBase];
			if (++matInc >= (int)item.MaterialStepRate)
			{
				matInc = 0;
				++matBase;
			}
			// Set texture transform
			if (transInc == 0)
			{
				XMMATRIX texTransform = XMLoadFloat4x4(&item.TextureTransform[transBase]);
				XMStoreFloat4x4(&m_perObjectCB->Data.TexTransform, XMMatrixTranspose(texTransform));
			}
			if (transInc >= 0 && ++transInc >= (int)item.TextureTransformStepRate)
			{
				transInc = 0;
				++transBase;
			}
			// Set diffuse texture
			if (texInc == 0)
				context->PSSetShaderResources(0, 1, m_diffuseMapSRV[texBase].GetAddressOf());
			if (texInc >= 0 && ++texInc >= (int)item.TextureStepRate)
			{
				texInc = 0;
				++texBase;
			}
			// Set normal texture
			if (norInc == 0)
			{
				context->PSSetShaderResources(1, 1, m_norMapSRV[norBase].GetAddressOf());
				if (m_feature.TessEnable)
					context->DSSetShaderResources(0, 1, m_norMapSRV[norBase].GetAddressOf());
			}
			if (norInc >= 0 && ++norInc >= (int)item.NorTextureStepRate)
			{
				norInc = 0;
				++norBase;
			}

			// Update constant buffer
			m_perObjectCB->ApplyChanges(context);

			// Draw
			if (m_object->UseIndex)
				context->DrawIndexed(item.Count, item.Start, item.Base);
			else
				context->Draw(item.VCount, item.Base);
		}
	}

	// Recovery
	if (recover)
	{
		ID3D11ShaderResourceView* nullSRV[3] = { nullptr, nullptr, nullptr };
		context->PSSetShaderResources(2, 3, nullSRV);
		if (m_feature.ClipEnable)
		{
			context->RSSetState(nullptr);
			ShaderChangement::RSS = nullptr;
		}
		if (m_feature.TessEnable)
		{
			ShaderChangement::HS = nullptr;
			ShaderChangement::DS = nullptr;
			context->HSSetShader(nullptr, nullptr, 0);
			context->DSSetShader(nullptr, nullptr, 0);
		}
	}
}

void BasicObject::DepthRender(bool recover /* = false */)
{
	if (!m_loadingComplete || !m_feature.ShadowEnable)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();
	// Set IA stage
	UINT stride = m_object->UseEx ? sizeof(PosNormalTexTan) : sizeof(Basic32);
	UINT offset = 0;
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	if (m_feature.Enhance)
	{
		if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST)
		{
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
			ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
		}
	}
	else
	{
		if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
		{
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_objectVB.GetAddressOf(), &stride, &offset);
	if (m_object->UseIndex)
	{
		context->IASetIndexBuffer(m_objectIB.Get(), DXGI_FORMAT_R32_UINT, 0);
	}

	// Bind shaders, constant buffers, srvs and samplers
	ID3D11Buffer* cbuffers[3] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer(), m_tessSettingsCB.GetBuffer() };
	ID3D11Buffer* cbuffersT[2] = { cbuffers[0], cbuffers[2] };
	ID3D11SamplerState* samplers[1] = { renderStateMgr->LinearSam() };
	// vs
	if (ShaderChangement::VS != m_depthVS.Get())
	{
		context->VSSetShader(m_depthVS.Get(), nullptr, 0);
		ShaderChangement::VS = m_depthVS.Get();
	}
	if (m_feature.Enhance)
		context->VSSetConstantBuffers(0, 3, cbuffers);
	else
		context->VSSetConstantBuffers(0, 2, cbuffers);
	// ps
	if (m_feature.ClipEnable)
	{
		if (ShaderChangement::PS != m_depthPS.Get())
		{
			context->PSSetShader(m_depthPS.Get(), nullptr, 0);
			ShaderChangement::PS = m_depthPS.Get();
		}
		context->PSSetSamplers(0, 1, samplers);
		if (ShaderChangement::RSS != renderStateMgr->DepthBiasNoCullRS())
		{
			context->RSSetState(renderStateMgr->DepthBiasNoCullRS());
			ShaderChangement::RSS = renderStateMgr->DepthBiasNoCullRS();
		}
	}
	else
	{
		if (ShaderChangement::PS != nullptr)
		{
			context->PSSetShader(nullptr, nullptr, 0);
			ShaderChangement::PS = nullptr;
		}
		if (ShaderChangement::RSS != renderStateMgr->DepthBiasRS())
		{
			context->RSSetState(renderStateMgr->DepthBiasRS());
			ShaderChangement::RSS = renderStateMgr->DepthBiasRS();
		}
	}
	// hs and ds
	if (m_feature.Enhance)
	{
		if (ShaderChangement::HS != m_depthHS.Get())
		{
			context->HSSetShader(m_depthHS.Get(), nullptr, 0);
			ShaderChangement::HS = m_depthHS.Get();
		}
		if (ShaderChangement::DS != m_depthDS.Get())
		{
			context->DSSetShader(m_depthDS.Get(), nullptr, 0);
			ShaderChangement::DS = m_depthDS.Get();
		}
		context->DSSetConstantBuffers(0, 2, cbuffersT);
		context->DSSetSamplers(0, 1, samplers);
	}
	else
	{
		if (ShaderChangement::HS != nullptr)
		{
			context->HSSetShader(nullptr, nullptr, 0);
			ShaderChangement::HS = nullptr;
		}
		if (ShaderChangement::DS != nullptr)
		{
			context->DSSetShader(nullptr, nullptr, 0);
			ShaderChangement::DS = nullptr;
		}
	}

	// Iterate over each unit
	int totalNum, texBase, texInc, norBase, norInc;
	texBase = norBase = 0;
	if (m_feature.TextureEnable)
	{
		if (m_feature.Enhance) norInc = 0;
		else norInc = -1;
		if (m_feature.ClipEnable) texInc = 0;
		else texInc = -1;
	}
	else
		texInc = norInc = -1;
	for (size_t i = 0; i < m_object->Units.size(); ++i)
	{
		BasicElementUnit& item = m_object->Units[i];
		totalNum = (int)item.Worlds.size();
		if (texInc > 0) texInc = 0;
		if (norInc > 0) norInc = 0;

		for (int k = 0; k < totalNum; ++k)
		{
			// Set world
			XMMATRIX world = XMLoadFloat4x4(&item.Worlds[k]);
			XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
			XMStoreFloat4x4(&m_perObjectCB->Data.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&m_perObjectCB->Data.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
			// Set diffuse texture
			if (texInc == 0)
				context->PSSetShaderResources(0, 1, m_diffuseMapSRV[texBase].GetAddressOf());
			if (texInc >= 0 && ++texInc >= (int)item.TextureStepRate)
			{
				texInc = 0;
				++texBase;
			}
			// Set normal texture
			if (m_feature.Enhance && norInc == 0)
				context->DSSetShaderResources(0, 1, m_norMapSRV[norBase].GetAddressOf());
			if (norInc >= 0 && ++norInc >= (int)item.NorTextureStepRate)
			{
				norInc = 0;
				++norBase;
			}

			// Update constant buffer
			m_perObjectCB->ApplyChanges(context);

			// Draw
			if (m_object->UseIndex)
				context->DrawIndexed(item.Count, item.Start, item.Base);
			else
				context->Draw(item.VCount, item.Base);
		}
	}

	// Recovery
	if (recover)
	{
		context->RSSetState(nullptr);
		ShaderChangement::RSS = nullptr;
		if (m_feature.Enhance)
		{
			ShaderChangement::HS = nullptr;
			ShaderChangement::DS = nullptr;
			context->HSSetShader(nullptr, nullptr, 0);
			context->DSSetShader(nullptr, nullptr, 0);
		}
	}
}

void BasicObject::NorDepRender(bool recover /* = false */)
{
	if (!m_loadingComplete || !m_feature.SsaoEnable)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();
	// Set IA stage.
	UINT stride = m_object->UseEx ? sizeof(PosNormalTexTan) : sizeof(Basic32);
	UINT offset = 0;
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	if (m_feature.Enhance)
	{
		if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST)
		{
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
			ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
		}
	}
	else
	{
		if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
		{
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_objectVB.GetAddressOf(), &stride, &offset);
	if (m_object->UseIndex)
	{
		context->IASetIndexBuffer(m_objectIB.Get(), DXGI_FORMAT_R32_UINT, 0);
	}

	// Bind shaders, constant buffers, srvs and samplers
	ID3D11Buffer* cbuffers[3] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer(), m_tessSettingsCB.GetBuffer() };
	ID3D11Buffer* cbuffersT[2] = { cbuffers[0], cbuffers[2] };
	ID3D11SamplerState* samplers[1] = { renderStateMgr->LinearSam() };
	// vs
	if (ShaderChangement::VS != m_norDepVS.Get())
	{
		context->VSSetShader(m_norDepVS.Get(), nullptr, 0);
		ShaderChangement::VS = m_norDepVS.Get();
	}
	if (m_feature.Enhance)
		context->VSSetConstantBuffers(0, 3, cbuffers);
	else
		context->VSSetConstantBuffers(0, 2, cbuffers);
	// ps
	if (ShaderChangement::PS != m_norDepPS.Get())
	{
		context->PSSetShader(m_norDepPS.Get(), nullptr, 0);
		ShaderChangement::PS = m_norDepPS.Get();
	}

	if (m_feature.ClipEnable)
	{
		context->PSSetSamplers(0, 1, samplers);
		if (ShaderChangement::RSS != renderStateMgr->NoCullRS())
		{
			context->RSSetState(renderStateMgr->NoCullRS());
			ShaderChangement::RSS = renderStateMgr->NoCullRS();
		}
	}
	else if (ShaderChangement::RSS != nullptr)
	{
		context->RSSetState(nullptr);
		ShaderChangement::RSS = nullptr;
	}

	// hs and ds
	if (m_feature.Enhance)
	{
		if (ShaderChangement::HS != m_norDepHS.Get())
		{
			context->HSSetShader(m_norDepHS.Get(), nullptr, 0);
			ShaderChangement::HS = m_norDepHS.Get();
		}
		if (ShaderChangement::DS != m_norDepDS.Get())
		{
			context->DSSetShader(m_norDepDS.Get(), nullptr, 0);
			ShaderChangement::DS = m_norDepDS.Get();
		}
		context->DSSetConstantBuffers(0, 2, cbuffersT);
		context->DSSetSamplers(0, 1, samplers);
	}
	else
	{
		if (ShaderChangement::HS != nullptr)
		{
			context->HSSetShader(nullptr, nullptr, 0);
			ShaderChangement::HS = nullptr;
		}
		if (ShaderChangement::DS != nullptr)
		{
			context->DSSetShader(nullptr, nullptr, 0);
			ShaderChangement::DS = nullptr;
		}
	}

	// Iterate over each unit
	int totalNum, texBase, texInc, norBase, norInc;
	texBase = norBase = 0;
	if (m_feature.TextureEnable)
	{
		if (m_feature.Enhance) norInc = 0;
		else norInc = -1;
		if (m_feature.ClipEnable) texInc = 0;
		else texInc = -1;
	}
	else
		texInc = norInc = -1;
	for (size_t i = 0; i < m_object->Units.size(); ++i)
	{
		BasicElementUnit& item = m_object->Units[i];

		totalNum = (int)item.Worlds.size();
		if (texInc > 0) texInc = 0;
		if (norInc > 0) norInc = 0;

		for (int k = 0; k < totalNum; ++k)
		{
			// Set world
			XMMATRIX world = XMLoadFloat4x4(&item.Worlds[k]);
			XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
			XMStoreFloat4x4(&m_perObjectCB->Data.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&m_perObjectCB->Data.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
			// Set diffuse texture
			if (texInc == 0)
				context->PSSetShaderResources(0, 1, m_diffuseMapSRV[texBase].GetAddressOf());
			if (texInc >= 0 && ++texInc >= (int)item.TextureStepRate)
			{
				texInc = 0;
				++texBase;
			}
			// Set normal texture
			if (m_feature.Enhance && norInc == 0)
				context->DSSetShaderResources(0, 1, m_norMapSRV[norBase].GetAddressOf());
			if (norInc >= 0 && ++norInc >= (int)item.NorTextureStepRate)
			{
				norInc = 0;
				++norBase;
			}

			// Update constant buffer
			m_perObjectCB->ApplyChanges(context);

			// Draw
			if (m_object->UseIndex)
				context->DrawIndexed(item.Count, item.Start, item.Base);
			else
				context->Draw(item.VCount, item.Base);
		}
	}

	// Recovery
	if (recover && m_feature.Enhance)
	{
		if (m_feature.ClipEnable)
		{
			context->RSSetState(nullptr);
			ShaderChangement::RSS = nullptr;
		}
		ShaderChangement::HS = nullptr;
		ShaderChangement::DS = nullptr;
		context->HSSetShader(nullptr, nullptr, 0);
		context->DSSetShader(nullptr, nullptr, 0);
	}
}

void BasicObject::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	// Direct3D data resources 
	m_objectVB.Reset();
	m_objectIB.Reset();
	m_tessSettingsCB.Reset();

	// Shaders
	m_inputLayout.Reset();
	m_basicVS.Reset();
	m_basicPS.Reset();
	m_basicHS.Reset();
	m_basicDS.Reset();
	m_depthVS.Reset();
	m_depthPS.Reset();
	m_depthHS.Reset();
	m_depthDS.Reset();
	m_norDepVS.Reset();
	m_norDepPS.Reset();
	m_norDepHS.Reset();
	m_norDepDS.Reset();

	// SRV
	m_diffuseMapSRV.clear();
	m_norMapSRV.clear();
	m_reflectMapSRV.Reset();
	m_depthMapSRV.Reset();
	m_ssaoMapSRV.Reset();
}

concurrency::task<void> BasicObject::LoadFeatureAsync(const BasicFeatureConfigure& feature)
{
	assert(IsMainThread());

	if (feature.NormalEnable && !m_object->UseEx)
		throw ref new Platform::InvalidArgumentException("Lack necessary vertex input data!");
	if (!m_texture && m_feature.TextureEnable)
		throw ref new Platform::InvalidArgumentException("Lack necessary texture input data!");
	if (!m_normal && m_feature.NormalEnable)
		throw ref new Platform::InvalidArgumentException("Lack necessary texture input data!");

	auto shaderMgr = ShaderMgr::Instance();
	auto textureMgr = TextureMgr::Instance();

	// Cache
	std::shared_ptr<ComPtr<ID3D11InputLayout>> inputLayout = std::make_shared<ComPtr<ID3D11InputLayout>>();
	std::shared_ptr<ComPtr<ID3D11VertexShader>> basicVS = std::make_shared<ComPtr<ID3D11VertexShader>>();
	std::shared_ptr<ComPtr<ID3D11PixelShader>> basicPS = std::make_shared<ComPtr<ID3D11PixelShader>>();
	std::shared_ptr<ComPtr<ID3D11HullShader>> basicHS = std::make_shared<ComPtr<ID3D11HullShader>>();
	std::shared_ptr<ComPtr<ID3D11DomainShader>> basicDS = std::make_shared<ComPtr<ID3D11DomainShader>>();
	std::shared_ptr<ComPtr<ID3D11VertexShader>> depthVS = std::make_shared<ComPtr<ID3D11VertexShader>>();;
	std::shared_ptr<ComPtr<ID3D11PixelShader>> depthPS = std::make_shared<ComPtr<ID3D11PixelShader>>();
	std::shared_ptr<ComPtr<ID3D11HullShader>> depthHS = std::make_shared<ComPtr<ID3D11HullShader>>();
	std::shared_ptr<ComPtr<ID3D11DomainShader>> depthDS = std::make_shared<ComPtr<ID3D11DomainShader>>();
	std::shared_ptr<ComPtr<ID3D11VertexShader>> norDepVS = std::make_shared<ComPtr<ID3D11VertexShader>>();
	std::shared_ptr<ComPtr<ID3D11PixelShader>> norDepPS = std::make_shared<ComPtr<ID3D11PixelShader>>();
	std::shared_ptr<ComPtr<ID3D11HullShader>> norDepHS = std::make_shared<ComPtr<ID3D11HullShader>>();
	std::shared_ptr<ComPtr<ID3D11DomainShader>> norDepDS = std::make_shared<ComPtr<ID3D11DomainShader>>();
	std::shared_ptr<ComPtr<ID3D11ShaderResourceView>> reflectMapSRV = std::make_shared<ComPtr<ID3D11ShaderResourceView>>();

	// Load shaders
	std::vector<concurrency::task<void>> CreateTasks;
	// basicVS
	std::wstring shaderName = L"BasicVS";
	InputLayoutType inputLayoutType = feature.NormalEnable ? InputLayoutType::PosNormalTexTan : InputLayoutType::Basic32;
	shaderName += feature.NormalEnable ? L'1' : L'0';
	shaderName += feature.TessEnable ? L'1' : L'0';
	shaderName += feature.ShadowEnable ? L'1' : L'0';
	shaderName += feature.SsaoEnable ? L'1' : L'0';
	shaderName += L'0';
	shaderName += L".cso";
	CreateTasks.push_back(shaderMgr->GetVSAsync(shaderName, inputLayoutType)
		.then([=](ID3D11VertexShader* vs)
	{
		*basicVS = vs;
		*inputLayout = shaderMgr->GetInputLayout(inputLayoutType);
	}));
	// basicPS
	shaderName = L"BasicPS";
	shaderName += feature.TextureEnable ? L'1' : L'0';
	shaderName += feature.ClipEnable ? L'1' : L'0';
	shaderName += feature.NormalEnable ? L'1' : L'0';
	shaderName += feature.ShadowEnable ? L'1' : L'0';
	shaderName += feature.SsaoEnable ? L'1' : L'0';
	shaderName += (L'0' + feature.LightCount);
	shaderName += feature.ReflectEnable ? L'1' : L'0';
	shaderName += feature.FogEnable ? L'1' : L'0';
	shaderName += L".cso";
	CreateTasks.push_back(shaderMgr->GetPSAsync(shaderName)
		.then([=](ID3D11PixelShader* ps) { *basicPS = ps; }));
	// basicHS and basicDS
	if (feature.TessEnable)
	{
		shaderName = L"BasicHS.cso";
		CreateTasks.push_back(shaderMgr->GetHSAsync(shaderName)
			.then([=](ID3D11HullShader* hs) { *basicHS = hs; }));
		shaderName = L"BasicDS";
		shaderName += feature.ShadowEnable ? L'1' : L'0';
		shaderName += feature.SsaoEnable ? L'1' : L'0';
		shaderName += L".cso";
		CreateTasks.push_back(shaderMgr->GetDSAsync(shaderName)
			.then([=](ID3D11DomainShader* ds) { *basicDS = ds; }));
	}
	// Shadow -- get depth
	if (feature.ShadowEnable)
	{
		shaderName = (feature.TessEnable && feature.Enhance) ? L"GetDepthVSTess.cso" : L"GetDepthVS.cso";
		CreateTasks.push_back(shaderMgr->GetVSAsync(shaderName, InputLayoutType::None)
			.then([=](ID3D11VertexShader* vs) { *depthVS = vs; }));
		if (feature.ClipEnable)
		{
			shaderName = L"GetDepthPSClip.cso";
			CreateTasks.push_back(shaderMgr->GetPSAsync(shaderName)
				.then([=](ID3D11PixelShader* ps) { *depthPS = ps; }));
		}
		if (feature.TessEnable && feature.Enhance)
		{
			shaderName = L"GetDepthHS.cso";
			CreateTasks.push_back(shaderMgr->GetHSAsync(shaderName)
				.then([=](ID3D11HullShader* hs) { *depthHS = hs; }));
			shaderName = L"GetDepthDS.cso";
			CreateTasks.push_back(shaderMgr->GetDSAsync(shaderName)
				.then([=](ID3D11DomainShader* ds) { *depthDS = ds; }));
		}
	}
	// Ssao -- get normal and depth
	if (feature.SsaoEnable)
	{
		shaderName = (feature.TessEnable && feature.Enhance) ? L"GetNorDepVSTess.cso" : L"GetNorDepVS.cso";
		CreateTasks.push_back(shaderMgr->GetVSAsync(shaderName, InputLayoutType::None)
			.then([=](ID3D11VertexShader* vs) { *norDepVS = vs; }));
		shaderName = feature.ClipEnable ? L"GetNorDepPSClip.cso" : L"GetNorDepPS.cso";
		CreateTasks.push_back(shaderMgr->GetPSAsync(shaderName)
			.then([=](ID3D11PixelShader* ps) { *norDepPS = ps; }));
		if (feature.TessEnable && feature.Enhance)
		{
			shaderName = L"GetNorDepHS.cso";
			CreateTasks.push_back(shaderMgr->GetHSAsync(shaderName)
				.then([=](ID3D11HullShader* hs) { *norDepHS = hs; }));
			shaderName = L"GetNorDepDS.cso";
			CreateTasks.push_back(shaderMgr->GetDSAsync(shaderName)
				.then([=](ID3D11DomainShader* ds) { *norDepDS = ds; }));
		}
	}
	// Reflect map SRV
	if (feature.ReflectFileName != L"")
	{
		CreateTasks.push_back(textureMgr->GetTextureAsync(feature.ReflectFileName)
			.then([=](ID3D11ShaderResourceView* srv) { *reflectMapSRV = srv; }));
	}

	return concurrency::when_all(CreateTasks.begin(), CreateTasks.end())
		.then([=]()
	{
		// Copy cache
		m_reflectMapSRV = reflectMapSRV->Get();
		m_inputLayout = inputLayout->Get();
		m_basicVS = basicVS->Get();
		m_basicPS = basicPS->Get();
		m_basicHS = basicHS->Get();
		m_basicDS = basicDS->Get();
		m_depthVS = depthVS->Get();
		m_depthPS = depthPS->Get();
		m_depthHS = depthHS->Get();
		m_depthDS = depthDS->Get();
		m_norDepVS = norDepVS->Get();
		m_norDepPS = norDepPS->Get();
		m_norDepHS = norDepHS->Get();
		m_norDepDS = norDepDS->Get();

		// Update tess constant buffer
		if (m_feature.TessEnable)
		{
			m_tessSettingsCB.Data = m_feature.TessDesc;
			m_tessSettingsCB.ApplyChanges(m_deviceResources->GetD3DDeviceContext());
		}
	});
}

concurrency::task<void> BasicObject::UpdateFeatureAsync(BasicFeatureConfigure feature)
{
	return LoadFeatureAsync(feature)
		.then([=]()
	{
		m_feature = feature;
	});
}

concurrency::task<void> BasicObject::BuildDataAsync()
{
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The objects haven't been initialized!");
		return concurrency::task_from_result();
	}

	auto textureMgr = TextureMgr::Instance();

	// Create VB and IB
	D3D11_BUFFER_DESC vbd;
	D3D11_SUBRESOURCE_DATA vinitData;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	if (m_object->UseEx)
	{
		vbd.ByteWidth = sizeof(PosNormalTexTan) * m_object->VertexDataEx.size();
		vinitData.pSysMem = &m_object->VertexDataEx[0];
	}
	else
	{
		vbd.ByteWidth = sizeof(Basic32) * m_object->VertexData.size();
		vinitData.pSysMem = &m_object->VertexData[0];
	}
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, &vinitData, m_objectVB.GetAddressOf()));

	if (m_object->UseIndex)
	{
		D3D11_BUFFER_DESC ibd;
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.ByteWidth = sizeof(UINT) * m_object->IndexData.size();
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.CPUAccessFlags = 0;
		ibd.MiscFlags = 0;
		D3D11_SUBRESOURCE_DATA iinitData;
		iinitData.pSysMem = &m_object->IndexData[0];
		ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&ibd, &iinitData, m_objectIB.GetAddressOf()));
	}

	// Load texture. Avoid loading same file at the same time.
	std::vector<concurrency::task<void>> LoadTasks;
	std::vector<std::wstring> fileCache;
	std::map<UINT, std::wstring> diffuseAd;
	std::map<UINT, std::wstring> normalAd;
	bool cacheFlag;

	UINT textureCount = 0;
	UINT normalCount = 0;
	for (auto& item : m_object->Units)
	{
		textureCount += item.TextureFileNames.size();
		normalCount += item.NorTextureFileNames.size();
	}
	m_diffuseMapSRV.resize(textureCount);
	m_norMapSRV.resize(normalCount);
	textureCount = 0;
	normalCount = 0;
	for (auto& item : m_object->Units)
	{
		if (m_texture)
		{
			for (size_t k = 0; k < item.TextureFileNames.size(); ++k)
			{
				cacheFlag = false;
				for (auto& file : fileCache)
					if (file == item.TextureFileNames[k])
					{
						cacheFlag = true;
						diffuseAd[textureCount] = item.TextureFileNames[k];
						break;
					}
				if (!cacheFlag)
				{
					LoadTasks.push_back(textureMgr->GetTextureAsync(item.TextureFileNames[k])
						.then([=](ID3D11ShaderResourceView* srv) { m_diffuseMapSRV[textureCount] = srv; }));
					fileCache.push_back(item.TextureFileNames[k]);
				}
				++textureCount;
			}
		}
		if (m_normal)
		{
			for (size_t k = 0; k < item.NorTextureFileNames.size(); ++k)
			{
				cacheFlag = false;
				for (auto& file : fileCache)
					if (file == item.NorTextureFileNames[k])
					{
						cacheFlag = true;
						normalAd[normalCount] = item.NorTextureFileNames[k];
						break;
					}
				if (!cacheFlag)
				{
					LoadTasks.push_back(textureMgr->GetTextureAsync(item.NorTextureFileNames[k])
						.then([=](ID3D11ShaderResourceView* srv) {m_norMapSRV[normalCount] = srv; }));
					fileCache.push_back(item.NorTextureFileNames[k]);
				}
				++normalCount;
			}
		}
	}

	return concurrency::when_all(LoadTasks.begin(), LoadTasks.end())
		.then([=]()
	{
		for (auto& item : diffuseAd)
			m_diffuseMapSRV[item.first] = textureMgr->GetTexture(item.second);
		for (auto& item : normalAd)
			m_norMapSRV[item.first] = textureMgr->GetTexture(item.second);
	});
}

void BasicObject::UpdateReflectMapSRV(ID3D11ShaderResourceView* srv)
{
	if (!m_feature.ReflectEnable) return;
	m_reflectMapSRV = srv;
}

void BasicObject::UpdateShadowMapSRV(ID3D11ShaderResourceView* srv)
{
	if (!m_feature.ShadowEnable) return;
	m_depthMapSRV = srv;
}

void BasicObject::UpdateSsaoMapSRV(ID3D11ShaderResourceView* srv)
{
	if (!m_feature.SsaoEnable) return;
	m_ssaoMapSRV = srv;
}

void BasicObject::UpdateDiffuseMapSRV(int i, ID3D11ShaderResourceView* srv)
{
	m_diffuseMapSRV[i] = srv;
}

void BasicObject::UpdateNormalMapSRV(int i, ID3D11ShaderResourceView* srv)
{
	m_norMapSRV[i] = srv;
}

BoundingBox BasicObject::GetTransBoundingBox(int i, int j)
{
	BoundingBox res;
	m_boundingBox[i].Transform(res, XMLoadFloat4x4(&m_object->Units[i].Worlds[j]));
	return res;
}

BoundingSphere BasicObject::GetTransBoundingSphere(int i, int j)
{
	BoundingSphere res;
	m_boundingSphere[i].Transform(res, XMLoadFloat4x4(&m_object->Units[i].Worlds[j]));
	return res;
}