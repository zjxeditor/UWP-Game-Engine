#include "pch.h"
#include "BillboardTrees.h"
#include <algorithm>
#include <vector>
#include <sstream>
#include "Common/DirectXHelper.h"
#include "Common/GeometryGenerator.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;

using namespace DX;

int BillboardTrees::m_signatureIndex = 0;

BillboardTrees::BillboardTrees(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB)
	: m_renderOptions(BillTreeRenderOption::Light3TexClipFog), m_alphaToCoverage(true), m_treeTypeNum(0), m_treeCount(0),
	m_loadingComplete(false), m_initialized(false), m_deviceResources(deviceResources), m_perFrameCB(perFrameCB)
{
	m_treeMat.Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_treeMat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_treeMat.Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 16.0f);

	++m_signatureIndex;
	std::wostringstream wos;
	wos << m_signatureBase << m_signatureIndex;
	m_textureArraySignature = wos.str();
}

void BillboardTrees::Initialize(const std::vector<PointSize>& data, const std::vector<std::wstring>& treeFileNames)
{
	m_treeFileNames = treeFileNames;
	m_treeTypeNum = treeFileNames.size();
	m_treeCount = data.size();
	m_positionData = data;

	m_initialized = true;
}

concurrency::task<void> BillboardTrees::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The trees haven't been initialized!");
		return concurrency::task_from_result();
	}

	// Initialize constant buffer
	m_treeSettingsCB.Initialize(m_deviceResources->GetD3DDevice());
	m_treeSettingsCB.Data.Mat = m_treeMat;
	m_treeSettingsCB.Data.TypeNum = m_treeTypeNum;
	m_treeSettingsCB.ApplyChanges(m_deviceResources->GetD3DDeviceContext());

	auto shaderMgr = ShaderMgr::Instance();
	auto textureMgr = TextureMgr::Instance();

	std::vector<concurrency::task<void>> CreateTasks;
	CreateTasks.push_back(shaderMgr->GetVSAsync(L"TreeBaseVS.cso", InputLayoutType::PointSize)
		.then([=](ID3D11VertexShader* vs)
	{
		m_treeVS = vs;
		m_treeInputLayout = shaderMgr->GetInputLayout(InputLayoutType::PointSize);
	}));
	CreateTasks.push_back(shaderMgr->GetGSAsync(L"TreeBaseGS.cso")
		.then([=](ID3D11GeometryShader* gs) {m_treeGS = gs; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"TreeLight3PS.cso")
		.then([=](ID3D11PixelShader* ps) { m_treeLight3PS = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"TreeLight3TexClipPS.cso")
		.then([=](ID3D11PixelShader* ps) { m_treeLight3TexClipPS = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"TreeLight3TexClipFogPS.cso")
		.then([=](ID3D11PixelShader* ps) { m_treeLight3TexClipFogPS = ps; }));
	CreateTasks.push_back(textureMgr->GetTextureArrayAsync(m_treeFileNames, m_textureArraySignature)
		.then([=](ID3D11ShaderResourceView* srv) {m_treeTextureMapArraySRV = srv; }));

	return concurrency::when_all(CreateTasks.begin(), CreateTasks.end())
		.then([=]()
	{
		// Build input data
		BuildTreeSpritesBuffer();
		m_loadingComplete = true;
	});
}

void BillboardTrees::Render()
{
	if (!m_loadingComplete)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	// Set IA stage
	UINT stride = sizeof(PointSize);
	UINT offset = 0;
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_POINTLIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	}
	if (ShaderChangement::InputLayout != m_treeInputLayout.Get())
	{
		context->IASetInputLayout(m_treeInputLayout.Get());
		ShaderChangement::InputLayout = m_treeInputLayout.Get();
	}
	context->IASetVertexBuffers(0, 1, m_treeSpriteVB.GetAddressOf(), &stride, &offset);

	ID3D11Buffer* cbuffers[2] = { m_perFrameCB->GetBuffer(), m_treeSettingsCB.GetBuffer() };
	ID3D11SamplerState* samplers[1] = { renderStateMgr->LinearSam() };
	// Bind shaders, constant buffers, srvs and samplers
	context->VSSetShader(m_treeVS.Get(), 0, 0);
	ShaderChangement::VS = m_treeVS.Get();
	context->GSSetShader(m_treeGS.Get(), 0, 0);
	context->GSSetConstantBuffers(0, 1, cbuffers);
	switch (m_renderOptions)
	{
	case BillTreeRenderOption::Light3:		// Light
		context->PSSetShader(m_treeLight3PS.Get(), 0, 0);
		ShaderChangement::PS = m_treeLight3PS.Get();
		break;
	case BillTreeRenderOption::Light3TexClip:		// LightTexClip
		context->PSSetShader(m_treeLight3TexClipPS.Get(), 0, 0);
		ShaderChangement::PS = m_treeLight3TexClipPS.Get();
		break;
	case BillTreeRenderOption::Light3TexClipFog:		// LightTexClipFog
		context->PSSetShader(m_treeLight3TexClipFogPS.Get(), 0, 0);
		ShaderChangement::PS = m_treeLight3TexClipFogPS.Get();
		break;
	default:
		throw ref new Platform::InvalidArgumentException("No such render option");
	}
	context->PSSetConstantBuffers(0, 2, cbuffers);
	context->PSSetSamplers(0, 1, samplers);
	context->PSSetShaderResources(0, 1, m_treeTextureMapArraySRV.GetAddressOf());

	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if (m_alphaToCoverage)
		context->OMSetBlendState(renderStateMgr->AlphaToCoverBS(), blendFactor, 0xffffffff);
	context->Draw(m_treeCount, 0);

	// Recover render state
	if (m_alphaToCoverage)
		context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
	// Remove gs
	context->GSSetShader(nullptr, 0, 0);
	ShaderChangement::GS = nullptr;
}

void BillboardTrees::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_treeSpriteVB.Reset();
	m_treeSettingsCB.Reset();
	m_treeInputLayout.Reset();
	m_treeVS.Reset();
	m_treeGS.Reset();
	m_treeLight3PS.Reset();
	m_treeLight3TexClipPS.Reset();
	m_treeLight3TexClipFogPS.Reset();
	m_treeTextureMapArraySRV.Reset();
}

void BillboardTrees::BuildTreeSpritesBuffer()
{
	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(PointSize) * m_treeCount;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &m_positionData[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, &vinitData, m_treeSpriteVB.GetAddressOf()));
}