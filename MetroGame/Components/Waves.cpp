//***************************************************************************************
// Waves.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "pch.h"
#include "Waves.h"
#include <vector>
#include "Common/DirectXHelper.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;

using namespace DX;


Waves::Waves(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB)
	: m_numRows(0), m_numCols(0), m_triangleCount(0), m_vertexCount(0),
	m_timeStep(0.0f), m_spatialStep(0.0f), m_renderOptions(WavesRenderOption::Light3TexFog),
	m_loadingComplete(false), m_initialized(false),
	m_deviceResources(deviceResources), m_perFrameCB(perFrameCB),
	m_perObjectCB(perObjectCB)
{
	m_k[0] = 0.0f;
	m_k[1] = 0.0f;
	m_k[2] = 0.0f;

	m_currSolution = nullptr;
	m_prevSolution = nullptr;
	m_normals = nullptr;

	XMStoreFloat4x4(&m_wavesTexTransform, XMMatrixIdentity());
	XMStoreFloat4x4(&m_wavesWorld, XMMatrixIdentity());
	m_wavesMat.Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_wavesMat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	m_wavesMat.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 32.0f);
}

Waves::~Waves()
{
	if (m_prevSolution != nullptr)
		delete[] m_prevSolution;
	if (m_currSolution != nullptr)
		delete[] m_currSolution;
	if (m_normals != nullptr)
		delete[] m_normals;
}

void Waves::Initialize(UINT m, UINT n, float dx, float dt, float speed, float damping, std::wstring texName)
{
	m_numRows = m;
	m_numCols = n;
	m_textureFileName = texName;

	m_vertexCount = m*n;
	m_triangleCount = (m - 1)*(n - 1) * 2;

	m_timeStep = dt;
	m_spatialStep = dx;

	float d = damping*dt + 2.0f;
	float e = (speed*speed)*(dt*dt) / (dx*dx);
	m_k[0] = (damping*dt - 2.0f) / d;
	m_k[1] = (4.0f - 8.0f*e) / d;
	m_k[2] = (2.0f*e) / d;

	if(m_prevSolution != nullptr)
		delete[] m_prevSolution;
	if(m_currSolution != nullptr)
		delete[] m_currSolution;
	if(m_normals != nullptr)
		delete[] m_normals;

	m_prevSolution = new XMFLOAT3[m*n];
	m_currSolution = new XMFLOAT3[m*n];
	m_normals = new XMFLOAT3[m*n];

	// Generate grid vertices in system memory.

	float halfWidth = (n - 1)*dx*0.5f;
	float halfDepth = (m - 1)*dx*0.5f;

	for (UINT i = 0; i < m; ++i)
	{
		float z = halfDepth - i*dx;
		for (UINT j = 0; j < n; ++j)
		{
			float x = -halfWidth + j*dx;

			m_prevSolution[i*n + j] = XMFLOAT3(x, 0.0f, z);
			m_currSolution[i*n + j] = XMFLOAT3(x, 0.0f, z);
			m_normals[i*n + j] = XMFLOAT3(0.0f, 1.0f, 0.0f);
		}
	}

	m_initialized = true;
}

concurrency::task<void> Waves::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The waves haven't been initialized!");
		return concurrency::task_from_result();
	}

	// Initialize constant buffer
	if (!m_perFrameCB->GetInitalizeState())
		m_perFrameCB->Initialize(m_deviceResources->GetD3DDevice());
	if (!m_perObjectCB->GetInitalizeState())
		m_perObjectCB->Initialize(m_deviceResources->GetD3DDevice());

	auto shaderMgr = ShaderMgr::Instance();
	auto textureMgr = TextureMgr::Instance();

	std::vector<concurrency::task<void>> CreateTasks;
	// Load shaders
	CreateTasks.push_back(shaderMgr->GetVSAsync(L"BasicVS0000.cso", InputLayoutType::Basic32)
		.then([=](ID3D11VertexShader* vs)
	{
		m_wavesVS = vs;
		m_wavesInputLayout = shaderMgr->GetInputLayout(InputLayoutType::Basic32);
	}));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"BasicPS00000300.cso")
		.then([=](ID3D11PixelShader* ps) {m_wavesLight3PS = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"BasicPS10000300.cso")
		.then([=](ID3D11PixelShader* ps) {m_wavesLight3TexPS = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"BasicPS10000301.cso")
		.then([=](ID3D11PixelShader* ps) {m_wavesLight3TexFogPS = ps; }));
	CreateTasks.push_back(textureMgr->GetTextureAsync(m_textureFileName)
		.then([=](ID3D11ShaderResourceView* srv) {m_wavesMapSRV = srv; }));

	return concurrency::when_all(CreateTasks.begin(), CreateTasks.end())
		.then([=]()
	{
		// Build input data
		BuildWaveGeometryBuffers();
		m_loadingComplete = true;
	});
}

void Waves::Update(DX::GameTimer const& timer)
{
	if (!m_loadingComplete)
	{
		return;
	}

	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((timer.GetTotalSeconds() - t_base) >= 0.1f)
	{
		t_base += 0.1f;

		DWORD i = 5 + rand() % (m_numRows - 10);
		DWORD j = 5 + rand() % (m_numCols - 10);

		float r = MathHelper::RandF(1.0f, 2.0f);
		Disturb(i, j, r);
	}
	Update((float)timer.GetElapsedSeconds());

	// Update the wave vertex buffer with the new solution.
	D3D11_MAPPED_SUBRESOURCE mappedData;
	ThrowIfFailed(m_deviceResources->GetD3DDeviceContext()->Map(m_wavesVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
	Basic32* v = reinterpret_cast<Basic32*>(mappedData.pData);
	
	float width = m_numCols*m_spatialStep;
	float depth = m_numRows*m_spatialStep;

	for (UINT i = 0; i < m_vertexCount; ++i)
	{
		v[i].Pos = m_currSolution[i];
		v[i].Normal = m_normals[i];

		// Derive texture-coordinates in [0,1] from position.
		v[i].Tex.x = 0.5f + m_currSolution[i].x / width;
		v[i].Tex.y = 0.5f - m_currSolution[i].z / depth;
	}
	m_deviceResources->GetD3DDeviceContext()->Unmap(m_wavesVB.Get(), 0);
}

void Waves::Update(float dt)
{
	static float t = 0;

	// Accumulate time.
	t += dt;

	// Only update the simulation at the specified time step.
	if (t >= m_timeStep)
	{
		// Only update interior points; we use zero boundary conditions.
		for (UINT i = 1; i < m_numRows - 1; ++i)
		{
			for (UINT j = 1; j < m_numCols - 1; ++j)
			{
				// After this update we will be discarding the old previous
				// buffer, so overwrite that buffer with the new update.
				// Note how we can do this inplace (read/write to same element) 
				// because we won't need prev_ij again and the assignment happens last.

				// Note j indexes x and i indexes z: h(x_j, z_i, t_k)
				// Moreover, our +z axis goes "down"; this is just to 
				// keep consistent with our row indices going down.

				m_prevSolution[i*m_numCols + j].y =
					m_k[0]*m_prevSolution[i*m_numCols + j].y +
					m_k[1]*m_currSolution[i*m_numCols + j].y +
					m_k[2]*(m_currSolution[(i + 1)*m_numCols + j].y +
						m_currSolution[(i - 1)*m_numCols + j].y +
						m_currSolution[i*m_numCols + j + 1].y +
						m_currSolution[i*m_numCols + j - 1].y);
			}
		}

		// We just overwrote the previous buffer with the new data, so
		// this data needs to become the current solution and the old
		// current solution becomes the new previous solution.
		std::swap(m_prevSolution, m_currSolution);

		t = 0.0f; // reset time

		// Compute normals using finite difference scheme.
		for (UINT i = 1; i < m_numRows - 1; ++i)
		{
			for (UINT j = 1; j < m_numCols - 1; ++j)
			{
				float l = m_currSolution[i*m_numCols + j - 1].y;
				float r = m_currSolution[i*m_numCols + j + 1].y;
				float t = m_currSolution[(i - 1)*m_numCols + j].y;
				float b = m_currSolution[(i + 1)*m_numCols + j].y;
				m_normals[i*m_numCols + j].x = -r + l;
				m_normals[i*m_numCols + j].y = 2.0f*m_spatialStep;
				m_normals[i*m_numCols + j].z = b - t;

				XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&m_normals[i*m_numCols + j]));
				XMStoreFloat3(&m_normals[i*m_numCols + j], n);
			}
		}
	}
}

void Waves::Disturb(UINT i, UINT j, float magnitude)
{
	// Don't disturb boundaries.
	assert(i > 1 && i < m_numRows - 2);
	assert(j > 1 && j < m_numCols - 2);

	float halfMag = 0.5f*magnitude;
	// Disturb the ijth vertex height and its neighbors.
	m_currSolution[i*m_numCols + j].y += magnitude;
	m_currSolution[i*m_numCols + j + 1].y += halfMag;
	m_currSolution[i*m_numCols + j - 1].y += halfMag;
	m_currSolution[(i + 1)*m_numCols + j].y += halfMag;
	m_currSolution[(i - 1)*m_numCols + j].y += halfMag;
}

void Waves::Render()
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
	ID3D11Buffer* cbuffers[2] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer() };
	ID3D11SamplerState* samplers[1] = { renderStateMgr->AnisotropicSam() };
	// vs
	if (ShaderChangement::VS != m_wavesVS.Get())
	{
		context->VSSetShader(m_wavesVS.Get(), 0, 0);
		ShaderChangement::VS = m_wavesVS.Get();
	}
	context->VSSetConstantBuffers(0, 2, cbuffers);
	// ps
	switch (m_renderOptions)
	{
	case WavesRenderOption::Light3:		// Light
		if (ShaderChangement::PS != m_wavesLight3PS.Get())
		{
			context->PSSetShader(m_wavesLight3PS.Get(), 0, 0);
			ShaderChangement::PS = m_wavesLight3PS.Get();
		}
		break;
	case WavesRenderOption::Light3Tex:		// LightTex
		if (ShaderChangement::PS != m_wavesLight3TexPS.Get())
		{
			context->PSSetShader(m_wavesLight3TexPS.Get(), 0, 0);
			ShaderChangement::PS = m_wavesLight3TexPS.Get();
		}
		break;
	case WavesRenderOption::Light3TexFog:		// LightTexFog
		if (ShaderChangement::PS != m_wavesLight3TexFogPS.Get())
		{
			context->PSSetShader(m_wavesLight3TexFogPS.Get(), 0, 0);
			ShaderChangement::PS = m_wavesLight3TexFogPS.Get();
		}
		break;
	default:
		throw ref new Platform::InvalidArgumentException("No such render option");
	}
	context->PSSetConstantBuffers(0, 2, cbuffers);
	if (m_renderOptions != WavesRenderOption::Light3)
	{
		context->PSSetSamplers(0, 1, samplers);
		context->PSSetShaderResources(0, 1, m_wavesMapSRV.GetAddressOf());
	}
	float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->OMSetBlendState(renderStateMgr->TransparentBS(), blendFactor, 0xffffffff);
	context->DrawIndexed(3 * m_triangleCount, 0, 0);

	// recover default blend state
	context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}

void Waves::ReleaseDeviceDependentResources()
{
	m_loadingComplete = true;

	m_wavesVB.Reset();
	m_wavesIB.Reset();
	m_wavesInputLayout.Reset();
	m_wavesVS.Reset();
	m_wavesLight3PS.Reset();
	m_wavesLight3TexPS.Reset();
	m_wavesLight3TexFogPS.Reset();
	m_wavesMapSRV.Reset();
}

void Waves::BuildWaveGeometryBuffers()
{
	// Create the vertex buffer.  Note that we allocate space only, as
	// we will be updating the data every time step of the simulation.
	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(Basic32) * m_vertexCount;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbd.MiscFlags = 0;
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, 0, m_wavesVB.GetAddressOf()));


	// Create the index buffer.  The index buffer is fixed, so we only 
	// need to create and set once.
	std::vector<UINT> indices(3 * m_triangleCount); // 3 indices per face
														   // Iterate over each quad.
	UINT m = m_numRows;
	UINT n = m_numCols;
	int k = 0;
	for (UINT i = 0; i < m - 1; ++i)
	{
		for (DWORD j = 0; j < n - 1; ++j)
		{
			indices[k] = i*n + j;
			indices[k + 1] = i*n + j + 1;
			indices[k + 2] = (i + 1)*n + j;

			indices[k + 3] = (i + 1)*n + j;
			indices[k + 4] = i*n + j + 1;
			indices[k + 5] = (i + 1)*n + j + 1;

			k += 6; // next quad
		}
	}

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &indices[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&ibd, &iinitData, m_wavesIB.GetAddressOf()));
}
	
