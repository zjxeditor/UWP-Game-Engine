#include "pch.h"
#include "BasicParticleSystem.h"
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

int BasicParticleSystem::m_signatureIndex = -1;

BasicParticleSystem::BasicParticleSystem(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB)
	: m_deviceResources(deviceResources), m_perFrameCB(perFrameCB),
	m_firstRun(true), m_needUpdateCB(false), m_age(0), m_initialized(false), m_loadingComplete(false)
{
	++m_signatureIndex;
	std::wostringstream wos;
	wos << m_signatureBase << m_signatureIndex;
	m_textureArraySignature = wos.str();
}

void BasicParticleSystem::Initialize(const BasicParticleSystemInitInfo& initInfo)
{
	m_initInfo = initInfo;
	m_initialized = true;
}

void BasicParticleSystem::Update(float dt)
{
	m_age += dt;
}

concurrency::task<void> BasicParticleSystem::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The BasicParticleSystem hasn't been initialized!");
		return concurrency::task_from_result();
	}

	// Initialize constant buffer
	m_settingsCB.Initialize(m_deviceResources->GetD3DDevice());
	m_settingsCB.Data.AccelW = m_initInfo.AccelW;
	m_settingsCB.Data.TexNum = m_initInfo.TexFileNames.size();
	m_settingsCB.Data.EmitDirW = m_initInfo.EmitDirW;
	m_settingsCB.Data.EmitPosW = m_initInfo.EmitPosW;
	m_settingsCB.ApplyChanges(m_deviceResources->GetD3DDeviceContext());

	auto shaderMgr = ShaderMgr::Instance();
	auto textureMgr = TextureMgr::Instance();

	std::vector<concurrency::task<void>> CreateTasks;

	// Load shaders
	CreateTasks.push_back(shaderMgr->GetVSAsync(m_initInfo.DrawVSFileName, InputLayoutType::BasicParticle)
		.then([=](ID3D11VertexShader* vs) 
	{
		m_drawVS = vs;
		m_inputLayout = shaderMgr->GetInputLayout(InputLayoutType::BasicParticle);
	}));
	CreateTasks.push_back(shaderMgr->GetVSAsync(L"BasicCommonSOVS.cso", InputLayoutType::BasicParticle)
		.then([=](ID3D11VertexShader* vs) {m_soVS = vs; }));
	CreateTasks.push_back(shaderMgr->GetGSAsync(m_initInfo.DrawGSFileName)
		.then([=](ID3D11GeometryShader* gs) {m_drawGS = gs; }));
	CreateTasks.push_back(shaderMgr->GetGSAsync(m_initInfo.SOGSFileName, StreamOutType::BasicParticle)
		.then([=](ID3D11GeometryShader* gs) {m_soGS = gs; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(m_initInfo.DrawPSFileName)
		.then([=](ID3D11PixelShader* ps) {m_drawPS = ps; }));
	// Load textures
	CreateTasks.push_back(textureMgr->GetTextureArrayAsync(m_initInfo.TexFileNames, m_textureArraySignature)
		.then([=](ID3D11ShaderResourceView* srv) {m_texArraySRV = srv; }));
	return concurrency::when_all(CreateTasks.begin(), CreateTasks.end())
		.then([=]()
	{
		if (m_initInfo.RandomTexSRV != nullptr)
			m_randomTexSRV = m_initInfo.RandomTexSRV;
		else
			CreateRandomTexture1DSRV(m_deviceResources->GetD3DDevice(), m_randomTexSRV.GetAddressOf());

		// Build input data
		BuildVB();

		m_loadingComplete = true;
	});
}

void BasicParticleSystem::Render()
{
	if (!m_loadingComplete)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	// Set IA stage.
	UINT stride = sizeof(BasicParticle);
	UINT offset = 0;
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_POINTLIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	}
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	// On the first pass, use the initialization VB.  Otherwise, use
	// the VB that contains the current particle list.
	if (m_firstRun)
		context->IASetVertexBuffers(0, 1, m_initVB.GetAddressOf(), &stride, &offset);
	else
		context->IASetVertexBuffers(0, 1, m_drawVB.GetAddressOf(), &stride, &offset);
	// Update settingsCB
	if (m_needUpdateCB)
	{
		m_settingsCB.ApplyChanges(context);
		m_needUpdateCB = false;
	}

	ID3D11Buffer* cbuffers[2] = { m_perFrameCB->GetBuffer(), m_settingsCB.GetBuffer() };
	ID3D11SamplerState* samplers[1] = { renderStateMgr->LinearSam() };
	
	//
	// Draw the current particle list using stream-out only to update them.  
	// The updated vertices are streamed-out to the target VB. 
	//
	context->VSSetShader(m_soVS.Get(), 0, 0);
	context->GSSetShader(m_soGS.Get(), 0, 0);
	context->PSSetShader(nullptr, 0, 0);
	context->GSSetConstantBuffers(0, 2, cbuffers);
	context->GSSetSamplers(0, 1, samplers);
	context->GSSetShaderResources(0, 1, m_randomTexSRV.GetAddressOf());
	// Set stream out buffer.
	context->SOSetTargets(1, m_streamOutVB.GetAddressOf(), &offset);
	// Set depth stencil state.
	context->OMSetDepthStencilState(renderStateMgr->DisableDSS(), 0);

	if (m_firstRun)
	{
		context->Draw(1, 0);
		m_firstRun = false;
	}
	else
	{
		context->DrawAuto();
	}

	// done streaming-out--unbind the vertex buffer
	ID3D11Buffer* bufferArray[1] = { 0 };
	context->SOSetTargets(1, bufferArray, &offset);
	// ping-pong the vertex buffers
	m_drawVB.Swap(m_streamOutVB);


	//
	// Draw the updated particle system we just streamed-out. 
	//
	context->VSSetShader(m_drawVS.Get(), 0, 0);
	context->GSSetShader(m_drawGS.Get(), 0, 0);
	context->PSSetShader(m_drawPS.Get(), 0, 0);
	context->VSSetConstantBuffers(0, 1, cbuffers + 1);
	context->GSSetConstantBuffers(0, 2, cbuffers);
	context->PSSetConstantBuffers(0, 2, cbuffers);
	context->PSSetSamplers(0, 1, samplers);
	ID3D11ShaderResourceView* srvs[2] = { m_randomTexSRV.Get(), m_texArraySRV.Get() };
	context->PSSetShaderResources(0, 2, srvs);
	// Set depth stencil state.
	context->OMSetDepthStencilState(renderStateMgr->NoWritesDSS(), 0);
	// Set blend state.
	float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->OMSetBlendState(renderStateMgr->AdditiveBS(), blendFactor, 0xffffffff);

	context->DrawAuto();

	// Clear and recover
	context->GSSetShader(nullptr, 0, 0);
	context->OMSetDepthStencilState(nullptr, 0);
	context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);

	// Update ShaderChangement cache
	ShaderChangement::VS = m_drawVS.Get();
	ShaderChangement::GS = nullptr;
	ShaderChangement::PS = m_drawPS.Get();
}

void BasicParticleSystem::ReleaseDeviceDependentResources()
{
	m_loadingComplete = true;

	m_settingsCB.Reset();
	m_initVB.Reset();
	m_drawVB.Reset();
	m_streamOutVB.Reset();
	m_inputLayout.Reset();
	m_soVS.Reset();
	m_drawVS.Reset();
	m_soGS.Reset();
	m_drawGS.Reset();
	m_drawPS.Reset();
	m_texArraySRV.Reset();
	m_randomTexSRV.Reset();
}

void BasicParticleSystem::BuildVB()
{
	//
	// Create the buffer to kick-off the particle system.
	//

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.ByteWidth = sizeof(BasicParticle) * 1;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	// The initial particle emitter has type 0 and age 0.  The rest
	// of the particle attributes do not apply to an emitter.
	BasicParticle p;
	ZeroMemory(&p, sizeof(BasicParticle));
	p.Age = 0.0f;
	p.Type = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &p;

	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, &vinitData, m_initVB.GetAddressOf()));

	//
	// Create the ping-pong buffers for stream-out and drawing.
	//
	vbd.ByteWidth = sizeof(BasicParticle) * m_initInfo.MaxParticles;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_STREAM_OUTPUT;

	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, 0, m_drawVB.GetAddressOf()));
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, 0, m_streamOutVB.GetAddressOf()));
}

void BasicParticleSystem::Reset()
{
	m_firstRun = true;
	m_age = 0.0f;
}


