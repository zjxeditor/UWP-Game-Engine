#include "pch.h"
#include "ShadowHelper.h"
#include "Common/DirectXHelper.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"
#include "Common/GeometryGenerator.h"

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;

using namespace DX;

ShadowHelper::ShadowHelper(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
	const std::shared_ptr<DX::Camera>& camera)
	: m_deviceResources(deviceResources), m_perFrameCB(perFrameCB), m_camera(camera),
	m_loadingComplete(false), m_initialized(false)
{
	XMStoreFloat4x4(&m_lightView, XMMatrixIdentity());
	XMStoreFloat4x4(&m_lightProj, XMMatrixIdentity());
}

void ShadowHelper::Initialize(const DirectX::XMFLOAT3& sceneCenter, float sceneRadius, int texelSize /* = 2048 */)
{
	m_sceneCenter = sceneCenter;
	m_sceneRadius = sceneRadius;
	m_texelSize = texelSize;
	m_initialized = true;
}

concurrency::task<void> ShadowHelper::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The ShadowHelper hasn't been initialized!");
		return concurrency::task_from_result();
	}

	BuildDepthMapViews();
	m_loadingComplete = true;
	return concurrency::task_from_result();
}

void ShadowHelper::Update(const DirectX::XMFLOAT3& lightDirection)
{
	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&lightDirection);
	XMVECTOR lightPos = -2.0f*m_sceneRadius*lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&m_sceneCenter);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX View = XMMatrixLookAtLH(lightPos, targetPos, up);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos,View));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - m_sceneRadius;
	float b = sphereCenterLS.y - m_sceneRadius;
	float n = sphereCenterLS.z - m_sceneRadius;
	float r = sphereCenterLS.x + m_sceneRadius;
	float t = sphereCenterLS.y + m_sceneRadius;
	float f = sphereCenterLS.z + m_sceneRadius;
	XMMATRIX Proj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	XMStoreFloat4x4(&m_lightView, View);
	XMStoreFloat4x4(&m_lightProj, Proj);
}

void ShadowHelper::Render(const std::function<void()>& DrawMap)
{
	if (!m_loadingComplete)
		return;

	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	context->RSSetViewports(1, &m_viewport);
	// Set null render target because we are only going to draw to depth buffer.
	// Setting a null render target will disable color writes.
	ID3D11RenderTargetView* renderTargets[1] = { nullptr };
	context->OMSetRenderTargets(1, renderTargets, m_depthMapDSV.Get());
	context->ClearDepthStencilView(m_depthMapDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	
	// Update per-frame constant buffer according to light
	XMMATRIX view = XMLoadFloat4x4(&m_lightView);
	XMMATRIX proj = XMLoadFloat4x4(&m_lightProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMStoreFloat4x4(&m_perFrameCB->Data.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_perFrameCB->Data.InvView, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(view), view)));
	XMStoreFloat4x4(&m_perFrameCB->Data.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_perFrameCB->Data.InvProj, XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(proj), proj)));
	XMStoreFloat4x4(&m_perFrameCB->Data.ViewProj, XMMatrixTranspose(viewProj));
	m_perFrameCB->ApplyChanges(context);

	DrawMap();

	// Restore old Viewport and render targets
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);
	renderTargets[0] = m_deviceResources->GetBackBufferRenderTargetView();
	context->OMSetRenderTargets(1, renderTargets, m_deviceResources->GetDepthStencilView());
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Silver);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	// Recovery per-frame constant buffer according to m_camera.
	m_perFrameCB->Data.LightProj = m_perFrameCB->Data.ViewProj;
	m_perFrameCB->Data.Pad0 = 1.0f / m_texelSize;
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

void ShadowHelper::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	m_depthMapDSV.Reset();
	m_depthMapSRV.Reset();
}

void ShadowHelper::BuildDepthMapViews()
{
	ID3D11Device* device = m_deviceResources->GetD3DDevice();

	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
	m_viewport.Width = static_cast<float>(m_texelSize);
	m_viewport.Height = static_cast<float>(m_texelSize);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	// Use typeless format because the DSV is going to interpret
	// the bits as DXGI_FORMAT_D24_UNORM_S8_UINT, whereas the SRV is going to interpret
	// the bits as DXGI_FORMAT_R24_UNORM_X8_TYPELESS.
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = m_texelSize;
	texDesc.Height = m_texelSize;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ComPtr<ID3D11Texture2D> depthMap;
	ThrowIfFailed(device->CreateTexture2D(&texDesc, 0, depthMap.GetAddressOf()));

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	ThrowIfFailed(device->CreateDepthStencilView(depthMap.Get(), &dsvDesc, m_depthMapDSV.GetAddressOf()));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	ThrowIfFailed(device->CreateShaderResourceView(depthMap.Get(), &srvDesc, m_depthMapSRV.GetAddressOf()));
}