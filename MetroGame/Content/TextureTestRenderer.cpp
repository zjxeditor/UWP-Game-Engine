#include "pch.h"
#include "TextureTestRenderer.h"

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
TextureTestRenderer::TextureTestRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,
	std::shared_ptr<DX::Camera>& camera) :
	m_loadingComplete(false), m_initialized(false),
	m_deviceResources(deviceResources), 
	m_camera(camera)
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
	m_objects = std::make_unique<BasicObjects>(BasicObjects(deviceResources, m_perFrameCB, m_perObjectCB));
}

// Initialize components
void TextureTestRenderer::Initialize()
{
	concurrency::task<void> initTask = concurrency::task_from_result();
	initTask.then([=]()
	{
		InitPlane();
		m_initialized = true;
	}, concurrency::task_continuation_context::use_arbitrary());
}

void TextureTestRenderer::CreateDeviceDependentResources(DX::TextureMgr* textureMgr, DX::ShaderMgr* shaderMgr)
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

	m_objects->CreateDeviceDependentResources(textureMgr, shaderMgr)
		.then([=]()
	{
		// Once the data is loaded, the object is ready to be rendered.
		m_loadingComplete = true;
	});
}

// Initializes view parameters when the window size changes.
void TextureTestRenderer::CreateWindowSizeDependentResources()
{
	// To do: create windows size related objects
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void TextureTestRenderer::Update(DX::GameTimer const& timer)
{
	if (!m_loadingComplete)
	{
		return;
	}
}

// Renders one frame using the vertex and pixel shaders.
void TextureTestRenderer::Render()
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

	m_objects->Render();

	ShaderChangement::Reset();
}

void TextureTestRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_perFrameCB->Reset();
	m_perObjectCB->Reset();
	m_objects->ReleaseDeviceDependentResources();
}

void TextureTestRenderer::InitPlane()
{
	// Initialize work
	auto data = new std::vector<BasicElementData>(1);
	auto& elementData = (*data)[0];

	GeometryGenerator::MeshData grid;
	GeometryGenerator geoGen;
	geoGen.CreateGrid(40.0f, 60.0f, 120, 80, grid);
	int gridIndexCount = grid.Indices.size();
	int gridVerticesCount = grid.Vertices.size();

	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	auto& vertices = elementData.VertexData;
	vertices.resize(gridVerticesCount);
	for (int i = 0; i < gridVerticesCount; ++i)
	{
		vertices[i].Pos = grid.Vertices[i].Position;
		vertices[i].Normal = grid.Vertices[i].Normal;
		vertices[i].Tex = grid.Vertices[i].TexC;
	}
	// Pack index data
	auto& indices = elementData.IndexData;
	indices.insert(indices.end(), grid.Indices.begin(), grid.Indices.end());

	// Set unit data
	XMFLOAT4X4 gridWorld;
	XMMATRIX I = XMMatrixIdentity();
	XMFLOAT4X4 One;
	XMStoreFloat4x4(&One, I);
	XMStoreFloat4x4(&gridWorld, I);
	Material gridMat;
	gridMat.Ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	gridMat.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	gridMat.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);

	// grid
	elementData.Units.resize(1);
	auto& unit = elementData.Units[0];
	unit.Base = 0;
	unit.Count = gridIndexCount;
	unit.Start = 0;
	unit.Worlds.push_back(gridWorld);
	unit.TextureFileNames.push_back(L"Media/Textures/ColorCrate.dds");
	XMFLOAT4X4 texTransform;
	XMStoreFloat4x4(&texTransform, XMMatrixScaling(12.0f, 16.0f, 1.0f));
	unit.TextureTransform.push_back(texTransform);
	unit.Material.push_back(gridMat);
	unit.RenderOption = BasicObjectsRenderOption::TexOnly;

	m_objects->Initialize(data);
}

// Input control
void TextureTestRenderer::OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{

}
void TextureTestRenderer::OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{

}
void TextureTestRenderer::OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{

}
void TextureTestRenderer::OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
{

}
void TextureTestRenderer::OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
{

}
