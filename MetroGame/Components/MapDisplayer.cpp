#include "pch.h"
#include "MapDisplayer.h"
#include "Common/DirectXHelper.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"
#include "Common/GeometryGenerator.h"

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;

using namespace DX;

MapDisplayer::MapDisplayer(const std::shared_ptr<DX::DeviceResources>& deviceResources)
	: m_loadingComplete(false), m_initialized(false), 
	m_deviceResources(deviceResources), m_type(MapDisplayType::ALL)
{
}

void MapDisplayer::Initialize(MapDisplayType type, const DirectX::XMFLOAT4X4& world, bool unbind /* = true */)
{
	m_type = type;
	m_world = world;
	m_unbind = unbind;

	// Scale and shift quad to lower-right corner.
	/*m_world = XMFLOAT4X4(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, -0.5f, 0.0f, 1.0f);*/

	//m_world = XMFLOAT4X4(
	//	0.5f, 0.0f, 0.0f, 0.5f,
	//	0.0f, 0.5f, 0.0f, -0.5f,
	//	0.0f, 0.0f, 1.0f, 0.0f,
	//	0.0f, 0.0f, 0.0f, 1.0f);

	m_initialized = true;
}

concurrency::task<void> MapDisplayer::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The MapDisplayer hasn't been initialized!");
		return concurrency::task_from_result();
	}

	// Set the offset world
	m_worldCB.Initialize(m_deviceResources->GetD3DDevice());
	XMStoreFloat4x4(&m_worldCB.Data.World, XMMatrixTranspose(XMLoadFloat4x4(&m_world)));
	m_worldCB.ApplyChanges(m_deviceResources->GetD3DDeviceContext());

	// Initialize constant buffer
	auto shaderMgr = ShaderMgr::Instance();
	auto textureMgr = TextureMgr::Instance();

	// Load shaders
	std::vector<concurrency::task<void>> CreateShaderTasks;

	CreateShaderTasks.push_back(shaderMgr->GetVSAsync(L"TextureDisplayVS.cso", InputLayoutType::Basic32)
		.then([=](ID3D11VertexShader* vs)
	{
		m_vs = vs;
		m_inputLayout = shaderMgr->GetInputLayout(InputLayoutType::Basic32);
	}));
	CreateShaderTasks.push_back(shaderMgr->GetPSAsync(L"TextureDisplayPSRed.cso")
		.then([=](ID3D11PixelShader* ps) { m_redPS = ps; }));
	CreateShaderTasks.push_back(shaderMgr->GetPSAsync(L"TextureDisplayPSGreen.cso")
		.then([=](ID3D11PixelShader* ps) { m_greenPS = ps; }));
	CreateShaderTasks.push_back(shaderMgr->GetPSAsync(L"TextureDisplayPSBlue.cso")
		.then([=](ID3D11PixelShader* ps) { m_bluePS = ps; }));
	CreateShaderTasks.push_back(shaderMgr->GetPSAsync(L"TextureDisplayPSAlpha.cso")
		.then([=](ID3D11PixelShader* ps) { m_alphaPS = ps; }));
	CreateShaderTasks.push_back(shaderMgr->GetPSAsync(L"TextureDisplayPSAll.cso")
		.then([=](ID3D11PixelShader* ps) { m_allPS = ps; }));

	return concurrency::when_all(CreateShaderTasks.begin(), CreateShaderTasks.end())
		.then([=]()
	{
		// Build input data
		BuildQuadGeometryBuffers();
		m_loadingComplete = true;
	});
}

void MapDisplayer::Render()
{
	if (!m_loadingComplete)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	// Set IA stage.
	UINT stride = sizeof(Basic32);
	UINT offset = 0;
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_quadVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_quadIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Bind shaders, constant buffers, srvs and samplers
	context->VSSetShader(m_vs.Get(), nullptr, 0);
	ShaderChangement::VS = m_vs.Get();
	switch (m_type)
	{
	case MapDisplayType::RED:
		context->PSSetShader(m_redPS.Get(), nullptr, 0);
		ShaderChangement::PS = m_redPS.Get();
		break;
	case MapDisplayType::GREEN:
		context->PSSetShader(m_greenPS.Get(), nullptr, 0);
		ShaderChangement::PS = m_greenPS.Get();
		break;
	case MapDisplayType::BLUE:
		context->PSSetShader(m_bluePS.Get(), nullptr, 0);
		ShaderChangement::PS = m_bluePS.Get();
		break;
	case MapDisplayType::ALPHA:
		context->PSSetShader(m_alphaPS.Get(), nullptr, 0);
		ShaderChangement::PS = m_alphaPS.Get();
		break;
	case MapDisplayType::ALL:
		context->PSSetShader(m_allPS.Get(), nullptr, 0);
		ShaderChangement::PS = m_allPS.Get();
		break;
	default:
		throw ref new Platform::FailureException("No such display type!");
	}

	ID3D11Buffer* cbuffers[1] = { m_worldCB.GetBuffer() };
	context->VSSetConstantBuffers(0, 1, cbuffers);
	ID3D11SamplerState* samplers[1] = { renderStateMgr->LinearSam() };
	context->PSSetSamplers(0, 1, samplers);
	context->PSSetShaderResources(0, 1, m_mapSRV.GetAddressOf());

	context->DrawIndexed(6, 0, 0);

	// Update ShaderChangment cache
	ShaderChangement::VS = nullptr;
	ShaderChangement::PS = nullptr;
	if (m_unbind)
	{
		// Unbind srv
		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		context->PSSetShaderResources(0, 1, nullSRV);
	}
}

void MapDisplayer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	// Direct3D data resources 
	m_quadVB.Reset();
	m_quadIB.Reset();
	m_worldCB.Reset();

	//Shaders
	m_inputLayout.Reset();
	m_vs.Reset();
	m_redPS.Reset();
	m_greenPS.Reset();
	m_bluePS.Reset();
	m_alphaPS.Reset();
	m_allPS.Reset();

	// SRV
	m_mapSRV.Reset();
}

void MapDisplayer::BuildQuadGeometryBuffers()
{
	GeometryGenerator::MeshData quad;

	GeometryGenerator geoGen;
	geoGen.CreateFullscreenQuad(quad);

	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	std::vector<Basic32> vertices(quad.Vertices.size());
	for (UINT i = 0; i < quad.Vertices.size(); ++i)
	{
		vertices[i].Pos = quad.Vertices[i].Position;
		vertices[i].Normal = quad.Vertices[i].Normal;
		vertices[i].Tex = quad.Vertices[i].TexC;
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(Basic32) * quad.Vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, &vinitData, m_quadVB.GetAddressOf()));

	// Pack the indices of all the meshes into one index buffer.
	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * quad.Indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &quad.Indices[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&ibd, &iinitData, m_quadIB.GetAddressOf()));
}



