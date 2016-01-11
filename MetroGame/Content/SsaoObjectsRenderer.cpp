#include "pch.h"
#include "SsaoObjectsRenderer.h"
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
SsaoObjectsRenderer::SsaoObjectsRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, const std::shared_ptr<DX::Camera>& camera)
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
	m_skull = std::make_unique<BasicObject>(deviceResources, m_perFrameCB, m_perObjectCB);
	m_sphere = std::make_unique<BasicObject>(deviceResources, m_perFrameCB, m_perObjectCB);
	m_base = std::make_unique<BasicObject>(deviceResources, m_perFrameCB, m_perObjectCB);
	m_shadowHelper = std::make_unique<ShadowHelper>(deviceResources, m_perFrameCB, m_camera);
	m_ssaoHelper = std::make_unique<SsaoHelper>(deviceResources, m_perFrameCB, m_camera);
	m_sky = std::make_unique<Sky>(deviceResources, m_perFrameCB, m_perObjectCB, m_camera);
	m_mapDisplayer = std::make_unique<MapDisplayer>(deviceResources);
}

// Initialize components
void SsaoObjectsRenderer::Initialize()
{
	concurrency::task<void> initTask = concurrency::task_from_result();
	initTask.then([=]()
	{
		// Estimate the scene bounding sphere manually since we know how the scene was constructed.
		// The grid is the "widest object" with a width of 20 and depth of 30.0f, and centered at
		// the world space origin.  In general, you need to loop over every world space vertex
		// position and compute the bounding sphere.
		XMFLOAT3 center(0.0f, 0.0f, 0.0f);
		float radius = sqrtf(10.0f*10.0f + 15.0f*15.0f);
		XMFLOAT4X4 mapWorld;
		XMMATRIX mapScale = XMMatrixScaling(0.2f, 0.2f, 1.0f);
		XMMATRIX mapOffset = XMMatrixTranslation(0.8f, 0.8f, 0.0f);
		XMStoreFloat4x4(&mapWorld, XMMatrixMultiply(mapScale, mapOffset));

		m_sky->Initialize(L"Media\\Textures\\desertcube1024.dds", 5000.0f);
		m_shadowHelper->Initialize(center, radius, 2048);
		m_ssaoHelper->Initialize(2, 0.5f, 0.2f, 2.0f, 0.05f);
		m_mapDisplayer->Initialize(MapDisplayType::RED, mapWorld, true);
		InitSkull();
		InitSphere();
		InitBase();
		m_initialized = true;
	}, concurrency::task_continuation_context::use_arbitrary());
}

void SsaoObjectsRenderer::CreateDeviceDependentResources()
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

	m_skull->CreateDeviceDependentResourcesAsync()
		.then([=]()
	{
		return m_sphere->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		return m_base->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		return m_sky->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		return m_shadowHelper->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		return m_ssaoHelper->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		return m_mapDisplayer->CreateDeviceDependentResourcesAsync();
	})
		.then([=]()
	{
		// Once the data is loaded, the object is ready to be rendered.
		m_loadingComplete = true;
	});
}

// Initializes view parameters when the window size changes.
void SsaoObjectsRenderer::CreateWindowSizeDependentResources()
{
	m_ssaoHelper->CreateWindowSizeDependentResources();
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void SsaoObjectsRenderer::Update(DX::GameTimer const& timer)
{
	if (!m_loadingComplete)
	{
		return;
	}
	m_sky->Update();

	// Animate the lights (and hence shadows).
	m_lightRotationAngle += 0.1f*(float)timer.GetElapsedSeconds();
	XMMATRIX R = XMMatrixRotationY(m_lightRotationAngle);
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&m_originalLightDir[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&m_dirLights[i].Direction, lightDir);
	}

	m_shadowHelper->Update(m_dirLights[0].Direction);
}

// Renders one frame using the vertex and pixel shaders.
void SsaoObjectsRenderer::Render()
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

	m_shadowHelper->Render([&]()
	{
		m_skull->DepthRender();
		m_sphere->DepthRender();
		m_base->DepthRender(true);
	});

	m_ssaoHelper->Render([&]()
	{
		m_skull->NorDepRender();
		m_sphere->NorDepRender();
		m_base->NorDepRender(true);
	});

	m_skull->UpdateShadowMapSRV(m_shadowHelper->GetDepthMapSRV());
	m_sphere->UpdateShadowMapSRV(m_shadowHelper->GetDepthMapSRV());
	m_base->UpdateShadowMapSRV(m_shadowHelper->GetDepthMapSRV());

	m_skull->UpdateSsaoMapSRV(m_ssaoHelper->GetSsaoMapSRV());
	m_sphere->UpdateSsaoMapSRV(m_ssaoHelper->GetSsaoMapSRV());
	m_base->UpdateSsaoMapSRV(m_ssaoHelper->GetSsaoMapSRV());

	//m_mapDisplayer->UpdateMapSRV(m_ssaoHelper->GetSsaoMapSRV());
	m_mapDisplayer->UpdateMapSRV(m_shadowHelper->GetDepthMapSRV());

	m_skull->Render();
	m_sphere->Render();
	m_base->Render(true);
	m_mapDisplayer->Render();
	m_sky->Render();
}

void SsaoObjectsRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_perFrameCB->Reset();
	m_perObjectCB->Reset();
	m_skull->ReleaseDeviceDependentResources();
	m_sphere->ReleaseDeviceDependentResources();
	m_base->ReleaseDeviceDependentResources();
	m_sky->ReleaseDeviceDependentResources();
	m_shadowHelper->ReleaseDeviceDependentResources();
	m_ssaoHelper->ReleaseDeviceDependentResources();
	m_mapDisplayer->ReleaseDeviceDependentResources();
}

void SsaoObjectsRenderer::InitSkull()
{
	// Init skull
	BasicObjectData* objectData = new BasicObjectData();
	objectData->UseEx = false;
	objectData->UseIndex = true;

	// Read binary data
	std::ifstream ss(L"Media\\Models\\skull.txt");
	if (!ss)
		throw ref new Platform::FailureException("Cannot load object model file!");

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	ss >> ignore >> vcount;
	ss >> ignore >> tcount;
	ss >> ignore >> ignore >> ignore >> ignore;

	auto& vertices = objectData->VertexData;
	vertices.resize(vcount);

	for (UINT i = 0; i < vcount; ++i)
	{
		ss >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		ss >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
	}

	ss >> ignore;
	ss >> ignore;
	ss >> ignore;

	int skullIndexCount = 3 * tcount;
	auto& indices = objectData->IndexData;
	indices.resize(skullIndexCount);
	for (UINT i = 0; i < tcount; ++i)
	{
		ss >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	ss.clear();

	// Set unit data
	XMFLOAT4X4 skullWorld;
	XMMATRIX skullScale = XMMatrixScaling(0.5f, 0.5f, 0.5f);
	XMMATRIX skullOffset = XMMatrixTranslation(0.0f, 1.0f, 0.0f);
	XMStoreFloat4x4(&skullWorld, XMMatrixMultiply(skullScale, skullOffset));

	Material skullMat;
	skullMat.Ambient = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
	skullMat.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	skullMat.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
	skullMat.Reflect = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

	objectData->Units.resize(1);
	auto& unit = objectData->Units[0];
	unit.VCount = vcount;
	unit.Base = 0;
	unit.Count = skullIndexCount;
	unit.Start = 0;
	unit.Worlds.push_back(skullWorld);
	unit.Material.push_back(skullMat);

	BasicFeatureConfigure objectFeature = { 0 };
	objectFeature.LightCount = 3;
	objectFeature.ShadowEnable = true;
	objectFeature.SsaoEnable = true;
	objectFeature.ReflectEnable = true;
	objectFeature.ReflectFileName = L"Media\\Textures\\desertcube1024.dds";

	m_skull->Initialize(objectData, objectFeature);
}

void SsaoObjectsRenderer::InitSphere()
{
	// Init spheres
	BasicObjectData* objectData = new BasicObjectData();
	objectData->UseIndex = true;
	objectData->UseEx = false;

	GeometryGenerator::MeshData sphere;
	GeometryGenerator geoGen;
	geoGen.CreateSphere(0.5f, 20, 20, sphere);

	int sphereIndexCount = sphere.Indices.size();

	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	auto& vertices = objectData->VertexData;
	vertices.resize(sphere.Vertices.size());
	for (size_t i = 0; i < sphere.Vertices.size(); ++i)
	{
		vertices[i].Pos = sphere.Vertices[i].Position;
		vertices[i].Normal = sphere.Vertices[i].Normal;
		vertices[i].Tex = sphere.Vertices[i].TexC;
	}

	// Pack the indices of all the meshes into one index buffer.
	auto& indices = objectData->IndexData;
	indices.assign(sphere.Indices.begin(), sphere.Indices.end());

	// Set unit data
	XMFLOAT4X4 sphereWorld[10];
	for (int i = 0; i < 5; ++i)
	{
		XMStoreFloat4x4(&sphereWorld[i * 2 + 0], XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i*5.0f));
		XMStoreFloat4x4(&sphereWorld[i * 2 + 1], XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i*5.0f));
	}

	Material sphereMat;
	sphereMat.Ambient = XMFLOAT4(0.2f, 0.3f, 0.4f, 1.0f);
	sphereMat.Diffuse = XMFLOAT4(0.2f, 0.3f, 0.4f, 1.0f);
	sphereMat.Specular = XMFLOAT4(0.9f, 0.9f, 0.9f, 16.0f);
	sphereMat.Reflect = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);

	objectData->Units.resize(1);
	// sphere
	XMFLOAT4X4 One;
	XMStoreFloat4x4(&One, XMMatrixIdentity());
	auto& unit = objectData->Units[0];
	unit.VCount = vertices.size();
	unit.Base = 0;
	unit.Count = sphereIndexCount;
	unit.Start = 0;
	unit.Worlds.assign(sphereWorld, sphereWorld + 10);
	unit.Material.push_back(sphereMat);
	unit.MaterialStepRate = 10;
	unit.TextureFileNames.push_back(L"Media\\Textures\\stone.dds");
	unit.TextureStepRate = 10;
	unit.TextureTransform.push_back(One);
	unit.TextureTransformStepRate = 10;

	BasicFeatureConfigure objectFeature = { 0 };
	objectFeature.LightCount = 3;
	objectFeature.TextureEnable = true;
	objectFeature.ShadowEnable = true;
	objectFeature.SsaoEnable = true;
	objectFeature.ReflectEnable = true;
	objectFeature.ReflectFileName = L"Media\\Textures\\desertcube1024.dds";

	m_sphere->Initialize(objectData, objectFeature);
}

void SsaoObjectsRenderer::InitBase()
{
	// Init base objects
	BasicObjectData* objectData = new BasicObjectData();
	objectData->UseIndex = true;
	objectData->UseEx = true;

	// Init shapes
	GeometryGenerator::MeshData box;
	GeometryGenerator::MeshData grid;
	GeometryGenerator::MeshData cylinder;

	GeometryGenerator geoGen;
	geoGen.CreateBox(1.0f, 1.0f, 1.0f, box);
	geoGen.CreateGrid(20.0f, 30.0f, 60, 40, grid);
	geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20, cylinder);

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	int boxVertexOffset = 0;
	int gridVertexOffset = box.Vertices.size();
	int cylinderVertexOffset = gridVertexOffset + grid.Vertices.size();

	// Cache the index count of each object.
	int boxIndexCount = box.Indices.size();
	int gridIndexCount = grid.Indices.size();
	int cylinderIndexCount = cylinder.Indices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	int boxIndexOffset = 0;
	int gridIndexOffset = boxIndexCount;
	int cylinderIndexOffset = gridIndexOffset + gridIndexCount;

	UINT totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		cylinder.Vertices.size();

	UINT totalIndexCount =
		boxIndexCount +
		gridIndexCount +
		cylinderIndexCount;

	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.

	auto& vertices = objectData->VertexDataEx;
	vertices.resize(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].Tex = box.Vertices[i].TexC;
		vertices[k].TangentU = box.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].Tex = grid.Vertices[i].TexC;
		vertices[k].TangentU = grid.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].Tex = cylinder.Vertices[i].TexC;
		vertices[k].TangentU = cylinder.Vertices[i].TangentU;
	}

	// Pack the indices of all the meshes into one index buffer.
	auto& indices = objectData->IndexData;
	indices.insert(indices.end(), box.Indices.begin(), box.Indices.end());
	indices.insert(indices.end(), grid.Indices.begin(), grid.Indices.end());
	indices.insert(indices.end(), cylinder.Indices.begin(), cylinder.Indices.end());

	// Set unit data
	XMFLOAT4X4 gridWorld, boxWorld, cylWorld[10];

	XMMATRIX I = XMMatrixIdentity();
	XMFLOAT4X4 One;
	XMStoreFloat4x4(&One, I);
	XMStoreFloat4x4(&gridWorld, I);

	XMMATRIX boxScale = XMMatrixScaling(3.0f, 1.0f, 3.0f);
	XMMATRIX boxOffset = XMMatrixTranslation(0.0f, 0.5f, 0.0f);
	XMStoreFloat4x4(&boxWorld, XMMatrixMultiply(boxScale, boxOffset));

	for (int i = 0; i < 5; ++i)
	{
		XMStoreFloat4x4(&cylWorld[i * 2 + 0], XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i*5.0f));
		XMStoreFloat4x4(&cylWorld[i * 2 + 1], XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i*5.0f));
	}

	Material gridMat, cylinderMat, boxMat;

	gridMat.Ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	gridMat.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	gridMat.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
	gridMat.Reflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	cylinderMat.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	cylinderMat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	cylinderMat.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
	cylinderMat.Reflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	boxMat.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	boxMat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	boxMat.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
	boxMat.Reflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	objectData->Units.resize(3);
	// box
	auto& unit0 = objectData->Units[0];
	unit0.VCount = box.Vertices.size();
	unit0.Base = boxVertexOffset;
	unit0.Count = boxIndexCount;
	unit0.Start = boxIndexOffset;
	unit0.Worlds.push_back(boxWorld);
	unit0.TextureFileNames.push_back(L"Media\\Textures\\stone.dds");
	unit0.NorTextureFileNames.push_back(L"Media\\Textures\\stones_nmap.dds");
	unit0.TextureTransform.push_back(One);
	unit0.Material.push_back(boxMat);

	// grid
	auto& unit1 = objectData->Units[1];
	unit1.VCount = grid.Vertices.size();
	unit1.Base = gridVertexOffset;
	unit1.Count = gridIndexCount;
	unit1.Start = gridIndexOffset;
	unit1.Worlds.push_back(gridWorld);
	unit1.TextureFileNames.push_back(L"Media\\Textures\\floor.dds");
	unit1.NorTextureFileNames.push_back(L"Media\\Textures\\floor_nmap.dds");
	XMFLOAT4X4 texTransform;
	XMStoreFloat4x4(&texTransform, XMMatrixScaling(6.0f, 8.0f, 1.0f));
	unit1.TextureTransform.push_back(texTransform);
	unit1.Material.push_back(gridMat);

	// cylinder
	auto& unit2 = objectData->Units[2];
	unit2.VCount = cylinder.Vertices.size();
	unit2.Base = cylinderVertexOffset;
	unit2.Count = cylinderIndexCount;
	unit2.Start = cylinderIndexOffset;
	unit2.Worlds.assign(cylWorld, cylWorld + 10);
	unit2.TextureFileNames.push_back(L"Media\\Textures\\bricks.dds");
	unit2.TextureStepRate = 10;
	unit2.NorTextureFileNames.push_back(L"Media\\Textures\\bricks_nmap.dds");
	unit2.NorTextureStepRate = 10;
	unit2.TextureTransform.push_back(One);
	unit2.TextureTransformStepRate = 10;
	unit2.Material.push_back(cylinderMat);
	unit2.MaterialStepRate = 10;

	BasicFeatureConfigure objectFeature = { 0 };
	objectFeature.LightCount = 3;
	objectFeature.TextureEnable = true;
	objectFeature.NormalEnable = true;
	objectFeature.ShadowEnable = true;
	objectFeature.SsaoEnable = true;
	//objectFeature->TessEnable = true;
	//objectFeature->Enhance = true;
	//objectFeature->TessDesc.HeightScale = 0.07f;
	//objectFeature->TessDesc.MaxTessDistance = 1.0f;
	//objectFeature->TessDesc.MaxTessFactor = 5.0f;
	//objectFeature->TessDesc.MinTessDistance = 25.0f;
	//objectFeature->TessDesc.MinTessFactor = 1.0f;

	m_base->Initialize(objectData, objectFeature);
}

// Input control
void SsaoObjectsRenderer::OnPointerPressed(Windows::UI::Core::PointerEventArgs^ args)
{
}
void SsaoObjectsRenderer::OnPointerReleased(Windows::UI::Core::PointerEventArgs^ args)
{
}
void SsaoObjectsRenderer::OnPointerMoved(Windows::UI::Core::PointerEventArgs^ args)
{
}
void SsaoObjectsRenderer::OnKeyDown(Windows::UI::Core::KeyEventArgs^ args)
{
}
void SsaoObjectsRenderer::OnKeyUp(Windows::UI::Core::KeyEventArgs^ args)
{
}
