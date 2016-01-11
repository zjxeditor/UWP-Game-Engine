//***************************************************************************************
// GpuWaves.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "pch.h"
#include "GpuWaves.h"
#include <algorithm>
#include <vector>
#include "Common/DirectXHelper.h"
#include "Common/GeometryGenerator.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;

using namespace DX;

GpuWaves::GpuWaves(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB)
	: m_numRows(0), m_numCols(0), m_indexCount(0),
	m_timeStep(0.0f), m_spatialStep(0.0f), m_renderOptions(GpuWavesRenderOption::Light3TexFog),
	m_loadingComplete(false), m_initialized(false),
	m_deviceResources(deviceResources), m_perFrameCB(perFrameCB),
	m_perObjectCB(perObjectCB)
{
	m_k[0] = 0.0f;
	m_k[1] = 0.0f;
	m_k[2] = 0.0f;

	XMStoreFloat4x4(&m_wavesTexTransform, XMMatrixIdentity());
	XMStoreFloat4x4(&m_wavesWorld, XMMatrixIdentity());
	m_wavesMat.Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_wavesMat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	m_wavesMat.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 32.0f);
}

void GpuWaves::Initialize(UINT m, UINT n, float dx, float dt, float speed, float damping, std::wstring texName)
{
	m_numRows = m;
	m_numCols = n;
	m_timeStep = dt;
	m_spatialStep = dx;
	m_textureFileName = texName;

	float d = damping*dt + 2.0f;
	float e = (speed*speed)*(dt*dt) / (dx*dx);
	m_k[0] = (damping*dt - 2.0f) / d;
	m_k[1] = (4.0f - 8.0f*e) / d;
	m_k[2] = (2.0f*e) / d;

	m_initialized = true;
}

concurrency::task<void> GpuWaves::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The waves haven't been initialized!");
		return concurrency::task_from_result();
	}

	// Initialize constant buffer
	m_rareChangedCB.Initialize(m_deviceResources->GetD3DDevice());
	m_updateConstantsCB.Initialize(m_deviceResources->GetD3DDevice());
	m_disturbSettingsCB.Initialize(m_deviceResources->GetD3DDevice());

	m_rareChangedCB.Data.GridSpatialStep = m_spatialStep;
	m_rareChangedCB.Data.DisplacementMapTexelSize = XMFLOAT2(1.0f / m_numCols, 1.0f / m_numRows);
	m_rareChangedCB.ApplyChanges(m_deviceResources->GetD3DDeviceContext());
	m_updateConstantsCB.Data.Constants.x = m_k[0];
	m_updateConstantsCB.Data.Constants.y = m_k[1];
	m_updateConstantsCB.Data.Constants.z = m_k[2];
	m_updateConstantsCB.ApplyChanges(m_deviceResources->GetD3DDeviceContext());

	auto shaderMgr = ShaderMgr::Instance();
	auto textureMgr = TextureMgr::Instance();

	std::vector<concurrency::task<void>> CreateTasks;
	// Load shaders
	CreateTasks.push_back(shaderMgr->GetVSAsync(L"WavesBaseVS.cso", InputLayoutType::Basic32)
		.then([=](ID3D11VertexShader* vs)
	{
		m_wavesVS = vs;
		m_wavesInputLayout = shaderMgr->GetInputLayout(InputLayoutType::Basic32);
	}));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"WavesLight3PS.cso")
		.then([=](ID3D11PixelShader* ps) {m_wavesLight3PS = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"WavesLight3TexPS.cso")
		.then([=](ID3D11PixelShader* ps) {m_wavesLight3TexPS = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"WavesLight3TexFogPS.cso")
		.then([=](ID3D11PixelShader* ps) {m_wavesLight3TexFogPS = ps; }));
	CreateTasks.push_back(shaderMgr->GetCSAsync(L"WavesDisturbCS.cso")
		.then([=](ID3D11ComputeShader* cs) {m_wavesDisturbCS = cs; }));
	CreateTasks.push_back(shaderMgr->GetCSAsync(L"WavesUpdateCS.cso")
		.then([=](ID3D11ComputeShader* cs) {m_wavesUpdateCS = cs; }));
	CreateTasks.push_back(textureMgr->GetTextureAsync(m_textureFileName)
		.then([=](ID3D11ShaderResourceView* srv) {m_wavesMapSRV = srv; }));

	return concurrency::when_all(CreateTasks.begin(), CreateTasks.end())
		.then([=]()
	{
		// Build input data
		BuildWaveSimulationViews();
		BuildWaveGeometryBuffers();

		m_loadingComplete = true;
	});
}

void GpuWaves::Update(DX::GameTimer const& timer)
{
	// Generate a random wave.
	static float t_base = 0.0f;

	if (!m_loadingComplete)
		return;

	if (((float)timer.GetTotalSeconds() - t_base) >= 0.1f)
	{
		t_base += 0.1f;

		DWORD i = 5 + rand() % (m_numRows - 10);
		DWORD j = 5 + rand() % (m_numCols - 10);

		float r = MathHelper::RandF(1.0f, 2.0f);

		Disturb(i, j, r);
	}

	float dt = (float)timer.GetElapsedSeconds();

	Update(dt);
}

void GpuWaves::Update(float dt)
{
	static float t = 0;

	// Accumulate time.
	t += dt;

	// Only update the simulation at the specified time step.
	if (t >= m_timeStep)
	{
		ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();
		ID3D11Buffer* cbuffers[1] = { m_updateConstantsCB.GetBuffer() };
		context->CSSetConstantBuffers(0, 1, cbuffers);
		ID3D11ShaderResourceView* srvs[2] = { m_wavesPrevSolSRV.Get(), m_wavesCurrSolSRV.Get() };
		context->CSSetShaderResources(0, 2, srvs);
		context->CSSetUnorderedAccessViews(0, 1, m_wavesNextSolUAV.GetAddressOf(), 0);

		context->CSSetShader(m_wavesUpdateCS.Get(), 0, 0);

		// How many groups do we need to dispatch to cover the wave grid.  
		// Note that mNumRows and mNumCols should be divisible by 16
		// so there is no remainder.
		UINT numGroupsX = m_numCols / 16;
		UINT numGroupsY = m_numRows / 16;
		context->Dispatch(numGroupsX, numGroupsY, 1);

		// Unbind the input textures from the CS for good housekeeping.
		ID3D11ShaderResourceView* nullSRV[1] = { 0 };
		context->CSSetShaderResources(0, 1, nullSRV);

		// Unbind output from compute shader (we are going to use this output as an input in the next pass, 
		// and a resource cannot be both an output and input at the same time.
		ID3D11UnorderedAccessView* nullUAV[1] = { 0 };
		context->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);

		// Disable compute shader.
		context->CSSetShader(nullptr, 0, 0);

		// Ping-pong buffers in preparation for the next update.
		// The previous solution is no longer needed and becomes the target of the next solution in the next update.
		// The current solution becomes the previous solution.
		// The next solution becomes the current solution.

		m_wavesPrevSolSRV.Swap(m_wavesCurrSolSRV);
		m_wavesCurrSolSRV.Swap(m_wavesNextSolSRV);

		m_wavesPrevSolUAV.Swap(m_wavesCurrSolUAV);
		m_wavesCurrSolUAV.Swap(m_wavesNextSolUAV);

		t = 0.0f; // reset time

		// Update ShaderChangment cache
		ShaderChangement::CS = nullptr;
	}
}

void GpuWaves::Disturb(UINT i, UINT j, float magnitude)
{
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	// Update constant buffer
	m_disturbSettingsCB.Data.Index.x = i;
	m_disturbSettingsCB.Data.Index.y = j;
	m_disturbSettingsCB.Data.Mag = magnitude;
	m_disturbSettingsCB.ApplyChanges(context);
	ID3D11Buffer* cbuffers[1] = { m_disturbSettingsCB.GetBuffer() };
	context->CSSetConstantBuffers(0, 1, cbuffers);
	context->CSSetUnorderedAccessViews(0, 1, m_wavesCurrSolUAV.GetAddressOf(), 0);

	context->CSSetShader(m_wavesDisturbCS.Get(), 0, 0);

	context->Dispatch(1, 1, 1);

	// Unbind output from compute shader (we are going to use this output as an input in the next pass, 
	// and a resource cannot be both an output and input at the same time.
	ID3D11UnorderedAccessView* nullUAV[1] = { 0 };
	context->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);
}

void GpuWaves::Render()
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
	if (ShaderChangement::InputLayout != m_wavesInputLayout.Get())
	{
		context->IASetInputLayout(m_wavesInputLayout.Get());
		ShaderChangement::InputLayout = m_wavesInputLayout.Get();
	}
	context->IASetVertexBuffers(0, 1, m_wavesVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_wavesIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Update per-object constant buffer.
	XMMATRIX world = XMLoadFloat4x4(&m_wavesWorld);
	XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
	XMMATRIX texTransform = XMLoadFloat4x4(&m_wavesTexTransform);
	XMStoreFloat4x4(&m_perObjectCB->Data.World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&m_perObjectCB->Data.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
	XMStoreFloat4x4(&m_perObjectCB->Data.TexTransform, XMMatrixTranspose(texTransform));
	m_perObjectCB->Data.Mat = m_wavesMat;
	m_perObjectCB->ApplyChanges(context);

	// Bind shaders, constant buffers, srvs and samplers
	ID3D11Buffer* cbuffers[3] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer(), m_rareChangedCB.GetBuffer() };
	ID3D11SamplerState* samplers[2] = { renderStateMgr->PointSam(), renderStateMgr->AnisotropicSam() };
	//vs
	context->VSSetShader(m_wavesVS.Get(), 0, 0);
	ShaderChangement::VS = m_wavesVS.Get();
	context->VSSetConstantBuffers(0, 3, cbuffers);
	context->VSSetSamplers(0, 1,samplers);
	context->VSSetShaderResources(0, 1, m_wavesCurrSolSRV.GetAddressOf());
	// ps
	switch (m_renderOptions)
	{
	case GpuWavesRenderOption::Light3:		// Light
		context->PSSetShader(m_wavesLight3PS.Get(), 0, 0);
		ShaderChangement::PS = m_wavesLight3PS.Get();
		break;
	case GpuWavesRenderOption::Light3Tex:		// LightTex
		context->PSSetShader(m_wavesLight3TexPS.Get(), 0, 0);
		ShaderChangement::PS = m_wavesLight3TexPS.Get();
		break;
	case GpuWavesRenderOption::Light3TexFog:		// LightTexFog
		context->PSSetShader(m_wavesLight3TexFogPS.Get(), 0, 0);
		ShaderChangement::PS = m_wavesLight3TexFogPS.Get();
		break;
	default:
		throw ref new Platform::InvalidArgumentException("No such render option");
	}
	context->PSSetConstantBuffers(0, 2, cbuffers);
	context->PSSetSamplers(0, 1, samplers + 1);
	context->PSSetShaderResources(0, 1, m_wavesMapSRV.GetAddressOf());

	float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->OMSetBlendState(renderStateMgr->TransparentBS(), blendFactor, 0xffffffff);
	context->DrawIndexed(m_indexCount, 0, 0);

	// Unbind displacement map from vertex shader, as we will be binding it as a compute
	// shader resource in the next update.
	ID3D11ShaderResourceView* nullSRV[1] = { 0 };
	context->VSSetShaderResources(0, 1, nullSRV);

	// recover default blend state
	context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}

void GpuWaves::ReleaseDeviceDependentResources()
{
	m_loadingComplete = true;

	m_wavesVB.Reset();
	m_wavesIB.Reset();
	m_rareChangedCB.Reset();
	m_updateConstantsCB.Reset();
	m_disturbSettingsCB.Reset();
	m_wavesInputLayout.Reset();
	m_wavesVS.Reset();
	m_wavesLight3PS.Reset();
	m_wavesLight3TexPS.Reset();
	m_wavesLight3TexFogPS.Reset();
	m_wavesUpdateCS.Reset();
	m_wavesDisturbCS.Reset();
	m_wavesMapSRV.Reset();
	m_wavesPrevSolSRV.Reset();
	m_wavesCurrSolSRV.Reset();
	m_wavesNextSolSRV.Reset();
	m_wavesPrevSolUAV.Reset();
	m_wavesCurrSolUAV.Reset();
	m_wavesNextSolUAV.Reset();
}

void GpuWaves::BuildWaveGeometryBuffers()
{
	GeometryGenerator::MeshData grid;
	GeometryGenerator geoGen;

	float width = m_numCols*m_spatialStep;
	float depth = m_numRows*m_spatialStep;

	geoGen.CreateGrid(width, depth, m_numRows, m_numCols, grid);

	m_indexCount = grid.Indices.size();

	// Extract the vertex elements we are interested.

	std::vector<Basic32> vertices(grid.Vertices.size());
	for (UINT i = 0; i < grid.Vertices.size(); ++i)
	{
		vertices[i].Pos = grid.Vertices[i].Position;
		vertices[i].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertices[i].Tex = grid.Vertices[i].TexC;
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(Basic32) * grid.Vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, &vinitData, m_wavesVB.GetAddressOf()));


	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * m_indexCount;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &grid.Indices[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&ibd, &iinitData, m_wavesIB.GetAddressOf()));
}

void GpuWaves::BuildWaveSimulationViews()
{
	// All the textures for the wave simulation will be bound as a shader resource and
	// unordered access view at some point since we ping-pong the buffers.

	ID3D11Device* device = m_deviceResources->GetD3DDevice();

	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = m_numCols;
	texDesc.Height = m_numRows;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	// Zero out the buffers initially.
	std::vector<float> zero(m_numRows*m_numCols, 0.0f);
	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = &zero[0];
	initData.SysMemPitch = m_numCols*sizeof(float);

	ComPtr<ID3D11Texture2D> prevWaveSolutionTex;
	ComPtr<ID3D11Texture2D> currWaveSolutionTex;
	ComPtr<ID3D11Texture2D> nextWaveSolutionTex;
	ThrowIfFailed(device->CreateTexture2D(&texDesc, &initData, prevWaveSolutionTex.GetAddressOf()));
	ThrowIfFailed(device->CreateTexture2D(&texDesc, &initData, currWaveSolutionTex.GetAddressOf()));
	ThrowIfFailed(device->CreateTexture2D(&texDesc, &initData, nextWaveSolutionTex.GetAddressOf()));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	ThrowIfFailed(device->CreateShaderResourceView(prevWaveSolutionTex.Get(), &srvDesc, m_wavesPrevSolSRV.GetAddressOf()));
	ThrowIfFailed(device->CreateShaderResourceView(currWaveSolutionTex.Get(), &srvDesc, m_wavesCurrSolSRV.GetAddressOf()));
	ThrowIfFailed(device->CreateShaderResourceView(nextWaveSolutionTex.Get(), &srvDesc, m_wavesNextSolSRV.GetAddressOf()));

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	ThrowIfFailed(device->CreateUnorderedAccessView(prevWaveSolutionTex.Get(), &uavDesc, m_wavesPrevSolUAV.GetAddressOf()));
	ThrowIfFailed(device->CreateUnorderedAccessView(currWaveSolutionTex.Get(), &uavDesc, m_wavesCurrSolUAV.GetAddressOf()));
	ThrowIfFailed(device->CreateUnorderedAccessView(nextWaveSolutionTex.Get(), &uavDesc, m_wavesNextSolUAV.GetAddressOf()));
}


