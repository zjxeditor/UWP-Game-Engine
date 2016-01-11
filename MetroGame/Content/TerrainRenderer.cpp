#include "pch.h"
#include "TerrainRenderer.h"

#include "Common\DirectXHelper.h"
#include "Common\MathHelper.h"
#include "Common\GeometryGenerator.h"
#include "Common\BasicReaderWriter.h"
#include "Common\ShaderChangement.h"

using namespace DXFramework;

using namespace DirectX;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;

using namespace DX;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
TerrainRenderer::TerrainRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,
	std::shared_ptr<DX::Camera>& camera) :
	m_loadingComplete(false), m_initialized(false),
	m_deviceResources(deviceResources), 
	m_camera(camera)
{
	m_camera->SetPosition(0.0f, 2.0f, 60.0f);
	m_camera->UpdateViewMatrix();

	m_dirLights[0].Ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	m_dirLights[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_dirLights[0].Specular = XMFLOAT4(0.8f, 0.8f, 0.7f, 1.0f);
	m_dirLights[0].Direction = XMFLOAT3(0.707f, -0.707f, 0.0f);

	m_dirLights[1].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_dirLights[1].Diffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[1].Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[1].Direction = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);

	m_dirLights[2].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_dirLights[2].Diffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[2].Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[2].Direction = XMFLOAT3(-0.57735f, -0.57735f, -0.57735f);

	m_perFrameCB = std::make_shared<ConstantBuffer<BasicPerFrameCB>>();
	m_perObjectCB = std::make_shared<ConstantBuffer<BasicPerObjectCB>>();
	m_sky = std::make_unique<Sky>(deviceResources, m_perFrameCB, m_perObjectCB, m_camera);
	m_terrain = std::make_unique<Terrain>(deviceResources, m_perFrameCB, m_perObjectCB);
}

// Initialize components
void TerrainRenderer::Initialize()
{
	concurrency::task<void> initTask = concurrency::task_from_result();
	initTask.then([=]()
	{
		m_sky->Initialize(L"Media\\Textures\\grasscube1024.dds", 5000.0f);

		TerrainInitInfo tii;
		tii.HeightMapFilename = L"Media\\Models\\terrain.raw";
		tii.LayerMapFilename0 = L"Media\\Textures\\terraingrass.dds";
		tii.LayerMapFilename1 = L"Media\\Textures\\terraindarkdirt.dds";
		tii.LayerMapFilename2 = L"Media\\Textures\\terrainstone.dds";
		tii.LayerMapFilename3 = L"Media\\Textures\\terrainlightdirt.dds";
		tii.LayerMapFilename4 = L"Media\\Textures\\terrainsnow.dds";
		tii.BlendMapFilename = L"Media\\Textures\\terrainblend.dds";
		tii.HeightScale = 50.0f;
		tii.HeightmapWidth = 2049;
		tii.HeightmapHeight = 2049;
		tii.CellSpacing = 0.5f;
		tii.MaxDist = 500.0f;
		tii.MaxTess = 6.0f;
		tii.MinDist = 20.0f;
		tii.MinTess = 0.0f;
		tii.TexScale = XMFLOAT2(50.0f, 50.0f);

		m_terrain->Initialize(tii);
		m_initialized = true;
	}, concurrency::task_continuation_context::use_arbitrary());
}

void TerrainRenderer::CreateDeviceDependentResources()
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

	m_sky->CreateDeviceDependentResourcesAsync()
		.then([=]()
	{
		return m_terrain->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		// Once the data is loaded, the object is ready to be rendered.
		m_loadingComplete = true;
	});
}

// Initializes view parameters when the window size changes.
void TerrainRenderer::CreateWindowSizeDependentResources()
{
	// To do: create windows size related objects
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void TerrainRenderer::Update(DX::GameTimer const& timer)
{
	if (!m_loadingComplete)
	{
		return;
	}

	XMFLOAT3 camPos = m_camera->GetPosition();
	float y = m_terrain->GetHeight(camPos.x, camPos.z);
	m_camera->SetPosition(camPos.x, y + 2.0f, camPos.z);

	m_sky->Update();
}

// Renders one frame using the vertex and pixel shaders.
void TerrainRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return;
	}

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

	m_perFrameCB->Data.FogStart = 15.0f;
	m_perFrameCB->Data.FogRange = 175.0f;
	m_perFrameCB->Data.FogColor = XMFLOAT4(0.65f, 0.65f, 0.65f, 1.0f);

	m_perFrameCB->ApplyChanges(context.Get());

	m_terrain->Render();
	m_sky->Render();
}

void TerrainRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_perFrameCB->Reset();
	m_perObjectCB->Reset();
	m_sky->ReleaseDeviceDependentResources();
	m_terrain->ReleaseDeviceDependentResources();
}

// Input control
void TerrainRenderer::OnPointerPressed(Windows::UI::Core::PointerEventArgs^ args)
{

}
void TerrainRenderer::OnPointerReleased(Windows::UI::Core::PointerEventArgs^ args)
{

}
void TerrainRenderer::OnPointerMoved(Windows::UI::Core::PointerEventArgs^ args)
{

}
void TerrainRenderer::OnKeyDown(Windows::UI::Core::KeyEventArgs^ args)
{
	switch (args->VirtualKey)
	{
	case Windows::System::VirtualKey::Number1:
		m_terrain->SetRenderOption(TerrainRenderOption::Light3);
		return;
	case Windows::System::VirtualKey::Number2:
		m_terrain->SetRenderOption(TerrainRenderOption::Light3Tex);
		return;
	case Windows::System::VirtualKey::Number3:
		m_terrain->SetRenderOption(TerrainRenderOption::Light3TexFog);
		return;
	default:
		break;
	}
}
void TerrainRenderer::OnKeyUp(Windows::UI::Core::KeyEventArgs^ args)
{

}
