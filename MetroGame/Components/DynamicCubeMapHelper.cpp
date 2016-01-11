#include "pch.h"
#include "DynamicCubeMapHelper.h"
#include "Common/DirectXHelper.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"
#include "Common/GeometryGenerator.h"

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;

using namespace DX;


DynamicCubeMapHelper::DynamicCubeMapHelper(
	const std::shared_ptr<DX::DeviceResources>& deviceResources, 
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
	const std::shared_ptr<DX::Camera>& camera)
	: m_deviceResources(deviceResources), m_perFrameCB(perFrameCB), m_camera(camera), m_center{ 0, 0, 0 },
	m_loadingComplete(false), m_initialized(false)
{
}

void DynamicCubeMapHelper::Initialize(const DirectX::XMFLOAT3 center, int cubeMapSize /* = 256 */)
{
	m_center = center;
	m_cubeMapSize = cubeMapSize;

	BuildCubeFaceCamera();
	m_initialized = true;
}

concurrency::task<void> DynamicCubeMapHelper::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The DynamicCubeMapHelper hasn't been initialized!");
		return concurrency::task_from_result();
	}

	return concurrency::task_from_result()
		.then([=]()
	{
		BuildCubeMapViews();
		m_loadingComplete = true;
	}, concurrency::task_continuation_context::use_current());
}

void DynamicCubeMapHelper::Render(const std::function<void()>& DrawMap)
{
	if (!m_loadingComplete)
		return;

	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	ID3D11RenderTargetView* renderTargets[1];
	// Generate the cube map.
	context->RSSetViewports(1, &m_cubeMapViewport);
	XMMATRIX view, proj, viewProj;
	for (int i = 0; i < 6; ++i)
	{
		// Clear cube map face and depth buffer.
		context->ClearRenderTargetView(m_cubeMapRTV[i].Get(), Colors::Silver);
		context->ClearDepthStencilView(m_cubeMapDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// Bind cube map face as render target.
		renderTargets[0] = m_cubeMapRTV[i].Get();
		context->OMSetRenderTargets(1, renderTargets, m_cubeMapDSV.Get());

		// Draw the scene with the exception of the center sphere to this cube map face.
		// Update per-frame constant buffer according to m_cubeMapCamera[i].
		view = m_cubeMapCamera[i].View();
		proj = m_cubeMapCamera[i].Proj();
		viewProj = m_cubeMapCamera[i].ViewProj();
		XMStoreFloat4x4(&m_perFrameCB->Data.View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&m_perFrameCB->Data.InvView, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(view), view)));
		XMStoreFloat4x4(&m_perFrameCB->Data.Proj, XMMatrixTranspose(proj));
		XMStoreFloat4x4(&m_perFrameCB->Data.InvProj, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(proj), proj)));
		XMStoreFloat4x4(&m_perFrameCB->Data.ViewProj, XMMatrixTranspose(viewProj));
		m_perFrameCB->Data.EyePosW = m_cubeMapCamera[i].GetPosition();
		m_perFrameCB->ApplyChanges(context);

		DrawMap();
	}

	// Restore old Viewport and render targets.
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	renderTargets[0] = m_deviceResources->GetBackBufferRenderTargetView();
	context->OMSetRenderTargets(1, renderTargets, m_deviceResources->GetDepthStencilView());

	// Have hardware generate lower mipmap levels of cube map.
	context->GenerateMips(m_cubeMapSRV.Get());

	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Silver);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Recovery per-frame constant buffer according to m_camera.
	view = m_camera->View();
	proj = m_camera->Proj();
	viewProj = m_camera->ViewProj();
	XMStoreFloat4x4(&m_perFrameCB->Data.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_perFrameCB->Data.InvView, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(view), view)));
	XMStoreFloat4x4(&m_perFrameCB->Data.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_perFrameCB->Data.InvProj, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(proj), proj)));
	XMStoreFloat4x4(&m_perFrameCB->Data.ViewProj, XMMatrixTranspose(viewProj));
	m_perFrameCB->Data.EyePosW = m_camera->GetPosition();
	m_perFrameCB->ApplyChanges(context);
}

void DynamicCubeMapHelper::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_cubeMapDSV.Reset();
	m_cubeMapSRV.Reset();
	for (int i = 0; i < 6; ++i)
		m_cubeMapRTV[i].Reset();
}

// Generate the cube map about the "m_center".
void DynamicCubeMapHelper::BuildCubeFaceCamera()
{
	// Look along each coordinate axis.
	float x = m_center.x;
	float y = m_center.y;
	float z = m_center.z;
	XMFLOAT3 targets[6] =
	{
		XMFLOAT3(x + 1.0f, y, z), // +X
		XMFLOAT3(x - 1.0f, y, z), // -X
		XMFLOAT3(x, y + 1.0f, z), // +Y
		XMFLOAT3(x, y - 1.0f, z), // -Y
		XMFLOAT3(x, y, z + 1.0f), // +Z
		XMFLOAT3(x, y, z - 1.0f)  // -Z
	};

	// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
	// are looking down +Y or -Y, so we need a different "up" vector.
	XMFLOAT3 ups[6] =
	{
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // +X
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // -X
		XMFLOAT3(0.0f, 0.0f, -1.0f), // +Y
		XMFLOAT3(0.0f, 0.0f, +1.0f), // -Y
		XMFLOAT3(0.0f, 1.0f, 0.0f),	 // +Z
		XMFLOAT3(0.0f, 1.0f, 0.0f)	 // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		m_cubeMapCamera[i].LookAt(m_center, targets[i], ups[i]);
		m_cubeMapCamera[i].SetLens(0.5f*XM_PI, 1.0f, 0.1f, m_camera->GetFarZ());
		m_cubeMapCamera[i].UpdateViewMatrix();
	}
}

void DynamicCubeMapHelper::BuildCubeMapViews()
{
	ID3D11Device* device = m_deviceResources->GetD3DDevice();

	// Cube map is a special texture array with 6 elements.
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = m_cubeMapSize;
	texDesc.Height = m_cubeMapSize;
	texDesc.MipLevels = 0;
	texDesc.ArraySize = 6;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;

	ComPtr<ID3D11Texture2D> cubeTex;
	ThrowIfFailed(device->CreateTexture2D(&texDesc, 0, cubeTex.GetAddressOf()));

	// Create a render target view to each cube map face 
	// (i.e., each element in the texture array).
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Texture2DArray.MipSlice = 0;

	for (int i = 0; i < 6; ++i)
	{
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		ThrowIfFailed(device->CreateRenderTargetView(cubeTex.Get(), &rtvDesc, m_cubeMapRTV[i].GetAddressOf()));
	}

	// Create a shader resource view to the cube map.
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = -1;

	ThrowIfFailed(device->CreateShaderResourceView(cubeTex.Get(), &srvDesc, m_cubeMapSRV.GetAddressOf()));

	// We need a depth texture for rendering the scene into the cubemap
	// that has the same resolution as the cubemap faces.  
	D3D11_TEXTURE2D_DESC depthTexDesc;
	depthTexDesc.Width = m_cubeMapSize;
	depthTexDesc.Height = m_cubeMapSize;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthTexDesc.CPUAccessFlags = 0;
	depthTexDesc.MiscFlags = 0;

	ComPtr<ID3D11Texture2D> depthTex;
	ThrowIfFailed(device->CreateTexture2D(&depthTexDesc, 0, depthTex.GetAddressOf()));

	// Create the depth stencil view for the entire cube
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = depthTexDesc.Format;
	dsvDesc.Flags = 0;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	ThrowIfFailed(device->CreateDepthStencilView(depthTex.Get(), &dsvDesc, m_cubeMapDSV.GetAddressOf()));

	// Viewport for drawing into cube map.
	m_cubeMapViewport.TopLeftX = 0.0f;
	m_cubeMapViewport.TopLeftY = 0.0f;
	m_cubeMapViewport.Width = (float)m_cubeMapSize;
	m_cubeMapViewport.Height = (float)m_cubeMapSize;
	m_cubeMapViewport.MinDepth = 0.0f;
	m_cubeMapViewport.MaxDepth = 1.0f;
}