#include "pch.h"
#include "GPUWavesRenderer.h"

#include "Common\DirectXHelper.h"
#include "Common\MathHelper.h"
#include "Common\GeometryGenerator.h"
#include "Common\BasicReaderWriter.h"
#include "Common\ShaderChangement.h"
#include <sstream>


using namespace DXFramework;

using namespace DirectX;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;

using namespace DX;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
GPUWavesRenderer::GPUWavesRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,
	std::shared_ptr<DX::Camera>& camera) :
	m_loadingComplete(false), m_initialized(false),
	m_deviceResources(deviceResources),
	m_camera(camera), m_wavesTexOffset(0, 0)
{
	m_dirLights[0].Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[0].Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_dirLights[0].Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	m_dirLights[0].Direction = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);

	m_dirLights[1].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_dirLights[1].Diffuse = XMFLOAT4(0.20f, 0.20f, 0.20f, 1.0f);
	m_dirLights[1].Specular = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
	m_dirLights[1].Direction = XMFLOAT3(-0.57735f, -0.57735f, 0.57735f);

	m_dirLights[2].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_dirLights[2].Diffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[2].Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_dirLights[2].Direction = XMFLOAT3(0.0f, -0.707f, -0.707f);

	m_perFrameCB = std::make_shared<ConstantBuffer<BasicPerFrameCB>>();
	m_perObjectCB = std::make_shared<ConstantBuffer<BasicPerObjectCB>>();
	m_waves = std::make_unique<GpuWaves>(m_deviceResources, m_perFrameCB, m_perObjectCB);
	m_land = std::make_unique<BasicObject>(deviceResources, m_perFrameCB, m_perObjectCB);
	m_crate = std::make_unique<BasicObject>(deviceResources, m_perFrameCB, m_perObjectCB);	
	m_trees = std::make_unique<BillboardTrees>(BillboardTrees(m_deviceResources, m_perFrameCB));
}

// Initialize components
void GPUWavesRenderer::Initialize()
{
	concurrency::task<void> initTask = concurrency::task_from_result();
	initTask.then([=]()
	{
		XMFLOAT4X4 One;
		XMStoreFloat4x4(&One, XMMatrixIdentity());
		m_waves->Initialize(320, 320, 0.5f, 0.03f, 4.0f, 0.2f, L"Media\\Textures\\water.dds");
		m_waves->SetRenderOption(GpuWavesRenderOption::Light3TexFog);
		InitTreeSprites();
		InitLand();
		InitCrate();

		m_initialized = true;
	}, concurrency::task_continuation_context::use_arbitrary());
}

void GPUWavesRenderer::CreateDeviceDependentResources()
{
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The components haven't been initialized!");
		return;
	}

	// Initialize constant buffer
	m_perFrameCB->Initialize(m_deviceResources->GetD3DDevice());
	m_perObjectCB->Initialize(m_deviceResources->GetD3DDevice());

	m_waves->CreateDeviceDependentResourcesAsync()
		.then([=]()
	{
		return m_land->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		return m_crate->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		return m_trees->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		// Once the data is loaded, the object is ready to be rendered.
		m_loadingComplete = true;
	});
}

// Initializes view parameters when the window size changes.
void GPUWavesRenderer::CreateWindowSizeDependentResources()
{
	// To do: create windows size related objects
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void GPUWavesRenderer::Update(DX::GameTimer const& timer)
{
	if (!m_loadingComplete)
	{
		return;
	}

	// Animate water texture coordinates.
	// Tile water texture.
	float dt = (float)timer.GetElapsedSeconds();
	XMMATRIX wavesScale = XMMatrixScaling(5.0f, 5.0f, 0.0f);
	// Translate texture over time.
	m_wavesTexOffset.y += 0.05f*dt;
	m_wavesTexOffset.x += 0.1f*dt;
	XMMATRIX wavesOffset = XMMatrixTranslation(m_wavesTexOffset.x, m_wavesTexOffset.y, 0.0f);
	XMFLOAT4X4 wavesTexTrans;
	XMStoreFloat4x4(&wavesTexTrans, wavesScale*wavesOffset);
	m_waves->SetTexTransform(wavesTexTrans);

	m_waves->Update(timer);
}

// Renders one frame using the vertex and pixel shaders.
void GPUWavesRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return;
	}

	auto renderStateMgr = RenderStateMgr::Instance();
	ComPtr<ID3D11DeviceContext> context = m_deviceResources->GetD3DDeviceContext();

	// Update per-frame constant buffer
	XMMATRIX view = m_camera->View();
	XMMATRIX proj = m_camera->Proj();
	XMMATRIX viewProj = m_camera->ViewProj();

	XMStoreFloat4x4(&m_perFrameCB->Data.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_perFrameCB->Data.InvView, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(view), view)));
	XMStoreFloat4x4(&m_perFrameCB->Data.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_perFrameCB->Data.InvProj, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(proj), proj)));
	XMStoreFloat4x4(&m_perFrameCB->Data.ViewProj, XMMatrixTranspose(viewProj));

	m_perFrameCB->Data.DirLights[0] = m_dirLights[0];
	m_perFrameCB->Data.DirLights[1] = m_dirLights[1];
	m_perFrameCB->Data.DirLights[2] = m_dirLights[2];
	m_perFrameCB->Data.EyePosW = m_camera->GetPosition();

	m_perFrameCB->Data.FogStart = 30.0f;
	m_perFrameCB->Data.FogRange = 250.0f;
	XMStoreFloat4(&m_perFrameCB->Data.FogColor, Colors::Silver);

	m_perFrameCB->ApplyChanges(context.Get());

	m_land->Render();
	m_crate->Render(true);
	m_trees->Render();
	m_waves->Render();
}

void GPUWavesRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_perFrameCB->Reset();
	m_perObjectCB->Reset();

	m_waves->ReleaseDeviceDependentResources();
	m_land->ReleaseDeviceDependentResources();
	m_crate->ReleaseDeviceDependentResources();
	m_trees->ReleaseDeviceDependentResources();
}

float GPUWavesRenderer::GetHillHeight(float x, float z)const
{
	return 0.3f*(z*sinf(0.1f*x) + x*cosf(0.1f*z));
}

XMFLOAT3 GPUWavesRenderer::GetHillNormal(float x, float z)const
{
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
		-0.03f*z*cosf(0.1f*x) - 0.3f*cosf(0.1f*z),
		1.0f,
		-0.3f*sinf(0.1f*x) + 0.03f*x*sinf(0.1f*z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}

void GPUWavesRenderer::InitLand()
{
	BasicObjectData* objectData = new BasicObjectData();
	objectData->UseIndex = true;
	objectData->UseEx = false;

	GeometryGenerator::MeshData grid;
	GeometryGenerator geoGen;
	geoGen.CreateGrid(160.0f, 160.0f, 50, 50, grid);

	// Extract the vertex elements we are interested and apply the height function to
	// each vertex.  
	auto& vertices = objectData->VertexData;
	vertices.resize(grid.Vertices.size());
	for (UINT i = 0; i < grid.Vertices.size(); ++i)
	{
		XMFLOAT3 p = grid.Vertices[i].Position;

		p.y = GetHillHeight(p.x, p.z);

		vertices[i].Pos = p;
		vertices[i].Normal = GetHillNormal(p.x, p.z);
		vertices[i].Tex = grid.Vertices[i].TexC;
	}

	objectData->IndexData.assign(grid.Indices.begin(), grid.Indices.end());

	// Set unit data
	XMFLOAT4X4 One, grassTexTransform;
	XMStoreFloat4x4(&One, XMMatrixIdentity());
	XMStoreFloat4x4(&grassTexTransform, XMMatrixScaling(5.0f, 5.0f, 0.0f));
	Material landMat;
	landMat.Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	landMat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	landMat.Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 16.0f);

	objectData->Units.resize(1);
	auto& unit = objectData->Units[0];
	unit.Base = 0;
	unit.Count = objectData->IndexData.size();
	unit.VCount = vertices.size();
	unit.Start = 0;
	unit.Worlds.push_back(One);
	unit.Material.push_back(landMat);
	unit.TextureFileNames.push_back(L"Media\\Textures\\grass.dds");
	unit.TextureTransform.push_back(grassTexTransform);

	BasicFeatureConfigure objectFeature = { 0 };
	objectFeature.LightCount = 3;
	objectFeature.TextureEnable = true;
	objectFeature.FogEnable = true;

	m_land->Initialize(objectData, objectFeature);
}

void GPUWavesRenderer::InitCrate()
{
	BasicObjectData* objectData = new BasicObjectData();
	objectData->UseIndex = true;
	objectData->UseEx = false;

	GeometryGenerator::MeshData box;
	GeometryGenerator geoGen;
	geoGen.CreateBox(1.0f, 1.0f, 1.0f, box);

	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	auto& vertices = objectData->VertexData;
	vertices.resize(box.Vertices.size());
	for (UINT i = 0; i < box.Vertices.size(); ++i)
	{
		vertices[i].Pos = box.Vertices[i].Position;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].Tex = box.Vertices[i].TexC;
	}

	objectData->IndexData.assign(box.Indices.begin(), box.Indices.end());

	// Set unit data
	XMFLOAT4X4 One, boxWorld;
	XMStoreFloat4x4(&One, XMMatrixIdentity());
	XMMATRIX boxScale = XMMatrixScaling(15.0f, 15.0f, 15.0f);
	XMMATRIX boxOffset = XMMatrixTranslation(8.0f, 5.0f, -15.0f);
	XMStoreFloat4x4(&boxWorld, boxScale*boxOffset);
	Material boxMat;
	boxMat.Ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	boxMat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	boxMat.Specular = XMFLOAT4(0.4f, 0.4f, 0.4f, 16.0f);

	objectData->Units.resize(1);
	auto& unit = objectData->Units[0];
	unit.Base = 0;
	unit.Count = objectData->IndexData.size();
	unit.VCount = vertices.size();
	unit.Start = 0;
	unit.Worlds.push_back(boxWorld);
	unit.Material.push_back(boxMat);
	unit.TextureFileNames.push_back(L"Media\\Textures\\wireFence.dds");
	unit.TextureTransform.push_back(One);

	BasicFeatureConfigure objectFeature = { 0 };
	objectFeature.LightCount = 3;
	objectFeature.TextureEnable = true;
	objectFeature.ClipEnable = true;
	objectFeature.FogEnable = true;

	m_crate->Initialize(objectData, objectFeature);
}

void GPUWavesRenderer::InitTreeSprites()
{
	std::vector<PointSize> v(16);

	for (UINT i = 0; i < 16; ++i)
	{
		float x = MathHelper::RandF(-35.0f, 35.0f);
		float z = MathHelper::RandF(-35.0f, 35.0f);
		float y = GetHillHeight(x, z);

		// Move tree slightly above land height.
		y += 10.0f;
		v[i].Pos = XMFLOAT3(x, y, z);
		v[i].Size = XMFLOAT2(24.0f, 24.0f);
	}

	std::vector<std::wstring> treeFilenames;
	treeFilenames.push_back(L"Media\\Textures\\tree0.dds");
	treeFilenames.push_back(L"Media\\Textures\\tree1.dds");
	treeFilenames.push_back(L"Media\\Textures\\tree2.dds");
	treeFilenames.push_back(L"Media\\Textures\\tree3.dds");

	m_trees->Initialize(v, treeFilenames);
}

// Input control
void GPUWavesRenderer::OnPointerPressed(Windows::UI::Core::PointerEventArgs^ args)
{
}
void GPUWavesRenderer::OnPointerReleased(Windows::UI::Core::PointerEventArgs^ args)
{
}
void GPUWavesRenderer::OnPointerMoved(Windows::UI::Core::PointerEventArgs^ args)
{
}
void GPUWavesRenderer::OnKeyDown(Windows::UI::Core::KeyEventArgs^ args)
{
}
void GPUWavesRenderer::OnKeyUp(Windows::UI::Core::KeyEventArgs^ args)
{
}