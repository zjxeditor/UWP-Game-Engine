#include "pch.h"
#include "ParticleSystemRenderer.h"

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
ParticleSystemRenderer::ParticleSystemRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,
	std::shared_ptr<DX::Camera>& camera) :
	m_loadingComplete(false), m_initialized(false),
	m_deviceResources(deviceResources), 
	m_camera(camera)
{
	m_camera->SetPosition(0.0f, 2.0f, 100.0f);
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
	m_fire = std::make_unique<BasicParticleSystem>(deviceResources, m_perFrameCB);
	m_rain = std::make_unique<BasicParticleSystem>(deviceResources, m_perFrameCB);
}

// Initialize components
void ParticleSystemRenderer::Initialize()
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

		BasicParticleSystemInitInfo psii;
		std::vector<std::wstring> texFileNames;

		psii.SOGSFileName = L"BasicFireSOGS.cso";
		psii.DrawVSFileName = L"BasicFireDrawVS.cso";
		psii.DrawGSFileName = L"BasicFireDrawGS.cso";
		psii.DrawPSFileName = L"BasicFireDrawPS.cso";
		texFileNames.push_back(L"Media\\Textures\\flare0.dds");
		psii.TexFileNames = texFileNames;
		psii.RandomTexSRV = nullptr;
		psii.MaxParticles = 500;
		psii.AccelW = XMFLOAT3(0.0f, 7.8f, 0.0f);
		psii.EmitDirW = XMFLOAT3(0.0f, 1.0f, 0.0f);
		psii.EmitPosW = XMFLOAT3(0.0f, 1.0f, 120.0f);
		m_fire->Initialize(psii);

		psii.SOGSFileName = L"BasicRainSOGS.cso";
		psii.DrawVSFileName = L"BasicRainDrawVS.cso";
		psii.DrawGSFileName = L"BasicRainDrawGS.cso";
		psii.DrawPSFileName = L"BasicRainDrawPS.cso";
		texFileNames.clear();
		texFileNames.push_back(L"Media\\Textures\\raindrop.dds");
		psii.TexFileNames = texFileNames;
		psii.RandomTexSRV = nullptr;
		psii.MaxParticles = 10000;
		psii.AccelW = XMFLOAT3(-1.0f, -9.8f, 0.0f);
		psii.EmitDirW = XMFLOAT3(0.0f, 1.0f, 0.0f);
		psii.EmitPosW = XMFLOAT3(0.0f, 0.0f, 0.0f);
		m_rain->Initialize(psii);

		m_initialized = true;
	}, concurrency::task_continuation_context::use_arbitrary());
}

void ParticleSystemRenderer::CreateDeviceDependentResources()
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
		return m_fire->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		return m_rain->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		// Once the data is loaded, the object is ready to be rendered.
		m_loadingComplete = true;
	});
}

// Initializes view parameters when the window size changes.
void ParticleSystemRenderer::CreateWindowSizeDependentResources()
{
	// To do: create windows size related objects
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void ParticleSystemRenderer::Update(DX::GameTimer const& timer)
{
	if (!m_loadingComplete)
	{
		return;
	}

	//XMFLOAT3 camPos = m_camera->GetPosition();
	//float y = m_terrain->GetHeight(camPos.x, camPos.z);
	//m_camera->SetPosition(camPos.x, y + 2.0f, camPos.z);

	m_perFrameCB->Data.GameTime = (float)timer.GetTotalSeconds();
	m_perFrameCB->Data.ElapseTime = (float)timer.GetElapsedSeconds();
	
	m_fire->Update((float)timer.GetElapsedSeconds());
	m_rain->Update((float)timer.GetElapsedSeconds());
	m_sky->Update();
}

// Renders one frame using the vertex and pixel shaders.
void ParticleSystemRenderer::Render()
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
	m_fire->Render();
	m_rain->SetEmitPos(m_camera->GetPosition());
	m_rain->Render();
}

void ParticleSystemRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_perFrameCB->Reset();
	m_perObjectCB->Reset();
	m_sky->ReleaseDeviceDependentResources();
	m_terrain->ReleaseDeviceDependentResources();
	m_fire->ReleaseDeviceDependentResources();
	m_rain->ReleaseDeviceDependentResources();
}

// Input control
void ParticleSystemRenderer::OnPointerPressed(Windows::UI::Core::PointerEventArgs^ args)
{

}
void ParticleSystemRenderer::OnPointerReleased(Windows::UI::Core::PointerEventArgs^ args)
{

}
void ParticleSystemRenderer::OnPointerMoved(Windows::UI::Core::PointerEventArgs^ args)
{

}
void ParticleSystemRenderer::OnKeyDown(Windows::UI::Core::KeyEventArgs^ args)
{

}
void ParticleSystemRenderer::OnKeyUp(Windows::UI::Core::KeyEventArgs^ args)
{

}
