#include "pch.h"
#include "Sky.h"
#include "Common/DirectXHelper.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"
#include "Common/GeometryGenerator.h"

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;

using namespace DX;

Sky::Sky(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB,
	const std::shared_ptr<DX::Camera>& camera)
	: m_loadingComplete(false), m_initialized(false), m_deviceResources(deviceResources), 
	m_perFrameCB(perFrameCB), m_perObjectCB(perObjectCB), m_camera(camera)
{
	XMStoreFloat4x4(&m_skyWorld, XMMatrixIdentity());
}

void Sky::Initialize(const std::wstring& texFileName, float skySphereRadius)
{
	m_texFileName = texFileName;
	m_sphereRadius = skySphereRadius;
	m_initialized = true;
}

concurrency::task<void> Sky::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The sky hasn't been initialized!");
		return concurrency::task_from_result();
	}

	// Initialize constant buffer
	auto shaderMgr = ShaderMgr::Instance();
	auto textureMgr = TextureMgr::Instance();

	// Load shaders
	auto CreateVSTask = shaderMgr->GetVSAsync(L"SkyBaseVS.cso", InputLayoutType::Pos)
		.then([=](ID3D11VertexShader* vs)
	{
		m_skyVS = vs;
		m_skyInputLayout = shaderMgr->GetInputLayout(InputLayoutType::Pos);
	});
	auto CreatePSTask = shaderMgr->GetPSAsync(L"SkyBasePS.cso")
		.then([=](ID3D11PixelShader* ps)
	{
		m_skyPS = ps;
	});
	auto CreateTexTask = textureMgr->GetTextureAsync(m_texFileName)
		.then([=](ID3D11ShaderResourceView* srv) { m_skyCubeMapSRV = srv; });

	return (CreatePSTask && CreateVSTask && CreateTexTask)
		.then([=]()
	{
		// Build input data
		BuildSkyGeometryBuffers();

		m_loadingComplete = true;
	});
}

void Sky::Update()
{
	XMFLOAT3 eyePos = m_camera->GetPosition();
	XMMATRIX T = XMMatrixTranslation(eyePos.x, eyePos.y, eyePos.z);
	XMStoreFloat4x4(&m_skyWorld, XMMatrixTranspose(T));
}

void Sky::Render()
{
	if (!m_loadingComplete)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	// Set IA stage.
	UINT stride = sizeof(XMFLOAT3);
	UINT offset = 0;
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
	if (ShaderChangement::InputLayout != m_skyInputLayout.Get())
	{
		context->IASetInputLayout(m_skyInputLayout.Get());
		ShaderChangement::InputLayout = m_skyInputLayout.Get();
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_skyVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_skyIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Bind shaders, constant buffers, srvs and samplers
	m_perObjectCB->Data.World = m_skyWorld;		// Have already transposed in the update process
	m_perObjectCB->ApplyChanges(context);

	context->VSSetShader(m_skyVS.Get(), 0, 0);
	ShaderChangement::VS = m_skyVS.Get();
	context->PSSetShader(m_skyPS.Get(), 0, 0);
	ShaderChangement::PS = m_skyPS.Get();
	ID3D11Buffer* cbuffers[2] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer() };
	context->VSSetConstantBuffers(0, 2, cbuffers);
	ID3D11SamplerState* samplers[1] = { renderStateMgr->LinearSam() };
	context->PSSetSamplers(0, 1, samplers);
	context->PSSetShaderResources(0, 1, m_skyCubeMapSRV.GetAddressOf());

	// Set render states
	context->OMSetDepthStencilState(renderStateMgr->LessEqualStateDSS(), 0);
	context->RSSetState(renderStateMgr->NoCullRS());

	context->DrawIndexed(m_indexCount, 0, 0);

	// recover render state
	context->OMSetDepthStencilState(nullptr, 0);
	context->RSSetState(nullptr);
	ShaderChangement::RSS = nullptr;
}

void Sky::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_skyVB.Reset();
	m_skyIB.Reset();
	m_skyInputLayout.Reset();
	m_skyVS.Reset();
	m_skyPS.Reset();
	m_skyCubeMapSRV.Reset();
}

void Sky::BuildSkyGeometryBuffers()
{
	GeometryGenerator::MeshData sphere;
	GeometryGenerator geoGen;
	geoGen.CreateSphere(m_sphereRadius, 30, 30, sphere);

	std::vector<XMFLOAT3> vertices(sphere.Vertices.size());

	for (size_t i = 0; i < sphere.Vertices.size(); ++i)
	{
		vertices[i] = sphere.Vertices[i].Position;
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(XMFLOAT3) * vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, &vinitData, m_skyVB.GetAddressOf()));

	m_indexCount = sphere.Indices.size();

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * m_indexCount;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.StructureByteStride = 0;
	ibd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &sphere.Indices[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&ibd, &iinitData, m_skyIB.GetAddressOf()));
}



