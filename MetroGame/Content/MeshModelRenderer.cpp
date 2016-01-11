#include "pch.h"
#include "MeshModelRenderer.h"
#include "Common\DirectXHelper.h"
#include "Common\MathHelper.h"
#include "Common\GeometryGenerator.h"
#include "Common\BasicReaderWriter.h"
#include "Common\ShaderChangement.h"
#include <fstream>

using namespace DXFramework;

using namespace DirectX;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;

using namespace DX;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
MeshModelRenderer::MeshModelRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, const std::shared_ptr<DX::Camera>& camera)
	: m_loadingComplete(false), m_initialized(false), m_deviceResources(deviceResources), m_camera(camera), m_lightRotationAngle(0.0f)
{
	m_dirLights[0].Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[0].Diffuse = XMFLOAT4(0.7f, 0.7f, 0.6f, 1.0f);
	m_dirLights[0].Specular = XMFLOAT4(0.8f, 0.8f, 0.7f, 1.0f);
	m_dirLights[0].Direction = XMFLOAT3(-0.57735f, -0.57735f, 0.57735f);

	m_dirLights[1].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_dirLights[1].Diffuse = XMFLOAT4(0.40f, 0.40f, 0.40f, 1.0f);
	m_dirLights[1].Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[1].Direction = XMFLOAT3(0.707f, -0.707f, 0.0f);

	m_dirLights[2].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_dirLights[2].Diffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[2].Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	m_dirLights[2].Direction = XMFLOAT3(0.0f, 0.0, -1.0f);

	m_originalLightDir[0] = m_dirLights[0].Direction;
	m_originalLightDir[1] = m_dirLights[1].Direction;
	m_originalLightDir[2] = m_dirLights[2].Direction;

	m_perFrameCB = std::make_shared<ConstantBuffer<BasicPerFrameCB>>();
	m_perObjectCB = std::make_shared<ConstantBuffer<BasicPerObjectCB>>();
	m_mesh = std::make_unique<MeshObject>(deviceResources, m_perFrameCB, m_perObjectCB);
}

// Initialize components
void MeshModelRenderer::Initialize()
{
	concurrency::task<void> initTask = concurrency::task_from_result();
	initTask.then([=]()
	{
		InitMesh();
		m_initialized = true;
	}, concurrency::task_continuation_context::use_arbitrary());
}

void MeshModelRenderer::CreateDeviceDependentResources()
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

	m_mesh->CreateDeviceDependentResourcesAsync()
		.then([=]()
	{
		// Once the data is loaded, the object is ready to be rendered.
		m_loadingComplete = true;
	});
}

// Initializes view parameters when the window size changes.
void MeshModelRenderer::CreateWindowSizeDependentResources()
{
	// To do: create windows size related objects
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void MeshModelRenderer::Update(DX::GameTimer const& timer)
{
	if (!m_loadingComplete)
	{
		return;
	}

	// Animate the lights (and hence shadows).
	m_lightRotationAngle += 0.5f*(float)timer.GetElapsedSeconds();
	XMMATRIX R = XMMatrixRotationY(m_lightRotationAngle);
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&m_originalLightDir[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&m_dirLights[i].Direction, lightDir);
	}
}

// Renders one frame using the vertex and pixel shaders.
void MeshModelRenderer::Render()
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

	m_perFrameCB->Data.FogStart = 10.0f;
	m_perFrameCB->Data.FogRange = 60.0f;
	m_perFrameCB->Data.FogColor = XMFLOAT4(0.65f, 0.65f, 0.65f, 1.0f);

	m_perFrameCB->ApplyChanges(context.Get());
	
	m_mesh->Render(true);
}

void MeshModelRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_perFrameCB->Reset();
	m_perObjectCB->Reset();
	m_mesh->ReleaseDeviceDependentResources();
}

void MeshModelRenderer::InitMesh()
{
	MeshObjectData* objectData = new MeshObjectData();
	MeshFeatureConfigure objectFeature = { 0 };
	objectData->Skinned = false;
	X3DLoader::LoadX3dStatic(L"Media\\Meshes\\Eagle\\Eagle.x3d", objectData->VertexData, objectData->IndexData, objectData->Subsets, objectData->Material);
	
	objectData->Worlds.resize(1);
	// Reflect to change coordinate system from the RHS the data was exported out as.
	XMMATRIX modelScale = XMMatrixScaling(0.1f, 0.1f, 0.1f);
	XMMATRIX modelRot = XMMatrixRotationX(-0.5f * MathHelper::Pi);
	XMMATRIX modelOffset = XMMatrixTranslation(0.0f, 0.0f, 10.0f);
	XMStoreFloat4x4(&objectData->Worlds[0], modelScale*modelRot*modelOffset);

	objectFeature.LightCount = 3;

	m_mesh->Initialize(objectData, objectFeature, L"Media\\Meshes\\Eagle\\");
}

// Input control
void MeshModelRenderer::OnPointerPressed(Windows::UI::Core::PointerEventArgs^ args)
{

}
void MeshModelRenderer::OnPointerReleased(Windows::UI::Core::PointerEventArgs^ args)
{

}
void MeshModelRenderer::OnPointerMoved(Windows::UI::Core::PointerEventArgs^ args)
{

}
void MeshModelRenderer::OnKeyDown(Windows::UI::Core::KeyEventArgs^ args)
{
}
void MeshModelRenderer::OnKeyUp(Windows::UI::Core::KeyEventArgs^ args)
{
}
