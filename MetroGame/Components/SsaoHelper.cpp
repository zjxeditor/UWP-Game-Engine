#include "pch.h"
#include "SsaoHelper.h"
#include "Common/DirectXHelper.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"
#include "Common/GeometryGenerator.h"
#include <DirectXPackedVector.h>

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;
using namespace DirectX::PackedVector;

using namespace DX;


SsaoHelper::SsaoHelper(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
	const std::shared_ptr<DX::Camera>& camera)
	: m_deviceResources(deviceResources), m_perFrameCB(perFrameCB), m_camera(camera),
	m_loadingComplete(false), m_initialized(false), m_updateSsaoSettings(false), m_updateTexSettings(false)
{
}

void SsaoHelper::Initialize(UINT blurCount /* = 3 */, float occlusionRadius /* = 0.5f */, float occlusionFadeStart /* = 0.2f */, float occlusionFadeEnd /* = 2.0f */, float surfaceEpsilon /* = 0.05f */)
{
	m_blurCount = blurCount;
	m_ssaoSettingsCB.Data.OcclusionRadius = occlusionRadius;
	m_ssaoSettingsCB.Data.OcclusionFadeStart = occlusionFadeStart;
	m_ssaoSettingsCB.Data.OcclusionFadeEnd = occlusionFadeEnd;
	m_ssaoSettingsCB.Data.SurfaceEpsilon = surfaceEpsilon;

	BuildOffsetVectors();
	m_updateSsaoSettings = true;
	m_updateTexSettings = true;
	m_initialized = true;
}

void SsaoHelper::CreateWindowSizeDependentResources()
{
	Windows::Foundation::Size renderTargetSize = m_deviceResources->GetRenderTargetSize();
	// We render to ambient map at half the resolution.
	m_ambientMapViewport.TopLeftX = 0.0f;
	m_ambientMapViewport.TopLeftY = 0.0f;
	m_ambientMapViewport.Width = renderTargetSize.Width / 2.0f;
	m_ambientMapViewport.Height = renderTargetSize.Height / 2.0f;
	m_ambientMapViewport.MinDepth = 0.0f;
	m_ambientMapViewport.MaxDepth = 1.0f;

	BuildFrustumFarCorners();
	BuildTextureViews();

	m_updateSsaoSettings = true;
	m_updateTexSettings = true;
}

concurrency::task<void> SsaoHelper::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The SsaoHelper hasn't been initialized!");
		return concurrency::task_from_result();
	}

	// Initialize constant buffer
	m_ssaoSettingsCB.Initialize(m_deviceResources->GetD3DDevice());
	m_texSettingsCB.Initialize(m_deviceResources->GetD3DDevice());

	auto shaderMgr = ShaderMgr::Instance();

	// Load shaders
	std::vector<concurrency::task<void>> CreateTasks;
	CreateTasks.push_back(shaderMgr->GetVSAsync(L"SsaoVS.cso", InputLayoutType::Basic32)
		.then([=](ID3D11VertexShader* vs)
	{
		m_ssaoVS = vs;
		m_inputLayout = shaderMgr->GetInputLayout(InputLayoutType::Basic32);
	}));
	CreateTasks.push_back(shaderMgr->GetVSAsync(L"BilateralBlurVS.cso", InputLayoutType::None)
		.then([=](ID3D11VertexShader* vs) { m_BilateralBlurVS = vs; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"SsaoPS.cso")
		.then([=](ID3D11PixelShader* ps) {m_ssaoPS = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"BilateralBlurPSH.cso")
		.then([=](ID3D11PixelShader* ps) {m_BilateralBlurPSHori = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"BilateralBlurPSV.cso")
		.then([=](ID3D11PixelShader* ps) {m_BilateralBlurPSVert = ps; }));

	return concurrency::when_all(CreateTasks.begin(), CreateTasks.end())
		.then([=]()
	{
		BuildFullScreenQuad();
		BuildRandomVectorTexture();

		m_loadingComplete = true;
	});
}

void SsaoHelper::Render(const std::function<void()>& DrawMap)
{
	if (!m_loadingComplete)
		return;
	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	//
	// Get normal depth map
	//
	ID3D11RenderTargetView* renderTargets[1] = { m_normalDepthRTV.Get() };
	context->OMSetRenderTargets(1, renderTargets, m_depthStencilDSV.Get());
	context->ClearRenderTargetView(m_normalDepthRTV.Get(), Colors::Silver);
	context->ClearDepthStencilView(m_depthStencilDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	DrawMap();

	//
	// Compute Ssao map
	//

	// Bind the ambient map as the render target.  Observe that this pass does not bind 
	// a depth/stencil buffer--it does not need it, and without one, no depth test is
	// performed, which is what we want.
	renderTargets[0] = { m_ambientRTV0.Get() };
	context->OMSetRenderTargets(1, renderTargets, nullptr);
	context->ClearRenderTargetView(m_ambientRTV0.Get(), Colors::Silver);
	context->RSSetViewports(1, &m_ambientMapViewport);

	// Set IA stage
	UINT stride = sizeof(Basic32);
	UINT offset = 0;
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}

	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_quadVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_quadIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Bind shaders, constant buffers, srvs and samplers
	context->VSSetShader(m_ssaoVS.Get(), nullptr, 0);
	ShaderChangement::VS = m_ssaoVS.Get();
	context->PSSetShader(m_ssaoPS.Get(), nullptr, 0);
	ShaderChangement::PS = m_ssaoPS.Get();
	ID3D11Buffer* cbuffers[2] = { m_perFrameCB->GetBuffer(), m_ssaoSettingsCB.GetBuffer() };
	ID3D11SamplerState* samplers[2] = { renderStateMgr->LinearSam(), renderStateMgr->SsaoSam() };
	context->VSSetConstantBuffers(0, 1, cbuffers + 1);
	context->PSSetConstantBuffers(0, 2, cbuffers);
	context->PSSetSamplers(0, 2, samplers);
	ID3D11ShaderResourceView* srvs[2] = { m_normalDepthSRV.Get(), m_randomVectorSRV.Get() };
	context->PSSetShaderResources(0, 2, srvs);

	// Update constant buffers
	if (m_updateSsaoSettings)
	{
		m_updateSsaoSettings = false;
		m_ssaoSettingsCB.ApplyChanges(context);
	}

	context->DrawIndexed(6, 0, 0);

	//
	// Blur the Ssao map
	//
	BlurAmbientMap();

	//
	// Recovery
	//

	// Restore old Viewport and render targets.
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);
	renderTargets[0] = m_deviceResources->GetBackBufferRenderTargetView();
	context->OMSetRenderTargets(1, renderTargets, m_deviceResources->GetDepthStencilView());
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Silver);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void SsaoHelper::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

	// Direct3D data resources 
	m_quadVB.Reset();
	m_quadIB.Reset();
	m_ssaoSettingsCB.Reset();
	m_texSettingsCB.Reset();

	// Shaders
	m_inputLayout.Reset();
	m_ssaoVS.Reset();
	m_ssaoPS.Reset();
	m_BilateralBlurVS.Reset();
	m_BilateralBlurPSHori.Reset();
	m_BilateralBlurPSVert.Reset();

	// Resources
	m_randomVectorSRV.Reset();
	m_normalDepthRTV.Reset();
	m_normalDepthSRV.Reset();
	m_depthStencilDSV.Reset();
	m_ambientRTV0.Reset();
	m_ambientSRV0.Reset();
	m_ambientRTV1.Reset();
	m_ambientSRV1.Reset();
}

void SsaoHelper::BlurAmbientMap()
{
	if (!m_loadingComplete)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	// Set IA stage
	UINT stride = sizeof(Basic32);
	UINT offset = 0;
	if (ShaderChangement::InputLayout != m_inputLayout.Get())
	{
		context->IASetInputLayout(m_inputLayout.Get());
		ShaderChangement::InputLayout = m_inputLayout.Get();
	}
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_quadVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_quadIB.Get(), DXGI_FORMAT_R32_UINT, 0);
	// Bind vs
	context->VSSetShader(m_BilateralBlurVS.Get(), nullptr, 0);
	ShaderChangement::VS = m_BilateralBlurVS.Get();
	// Set ps
	ID3D11Buffer* cbuffers[1] = { m_texSettingsCB.GetBuffer() };
	ID3D11SamplerState* samplers[1] = { renderStateMgr->LinearMipPointSam() };
	context->PSSetConstantBuffers(0, 1, cbuffers);
	context->PSSetSamplers(0, 1, samplers);

	// Update constant buffers
	if (m_updateTexSettings)
	{
		m_updateTexSettings = false;
		m_texSettingsCB.Data.TexelWidth = 1.0f / m_ambientMapViewport.Width;
		m_texSettingsCB.Data.TexelHeight = 1.0f / m_ambientMapViewport.Height;
		m_texSettingsCB.ApplyChanges(context);
	}

	for (UINT i = 0; i < m_blurCount; ++i)
	{
		// Ping-pong the two ambient map textures as we apply
		// horizontal and vertical blur passes.
		BlurAmbientMap(m_ambientSRV0.Get(), m_ambientRTV1.Get(), true);
		BlurAmbientMap(m_ambientSRV1.Get(), m_ambientRTV0.Get(), false);
	}
}

void SsaoHelper::BlurAmbientMap(ID3D11ShaderResourceView* inputSRV, ID3D11RenderTargetView* outputRTV, bool horzBlur)
{
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();
	ID3D11RenderTargetView* renderTargets[1] = { outputRTV };
	context->OMSetRenderTargets(1, renderTargets, nullptr);
	context->ClearRenderTargetView(outputRTV, Colors::Silver);
	context->RSSetViewports(1, &m_ambientMapViewport);
	
	// Bind ps
	if (horzBlur)
	{
		context->PSSetShader(m_BilateralBlurPSHori.Get(), nullptr, 0);
		ShaderChangement::PS = m_BilateralBlurPSHori.Get();
	}
	else
	{
		context->PSSetShader(m_BilateralBlurPSVert.Get(), nullptr, 0);
		ShaderChangement::PS = m_BilateralBlurPSVert.Get();
	}
	// Bind srv
	ID3D11ShaderResourceView* srvs[2] = { inputSRV, m_normalDepthSRV.Get() };
	context->PSSetShaderResources(0, 2, srvs);

	context->DrawIndexed(6, 0, 0);

	// Unbind the input SRV as it is going to be an output in the next blur.
	ID3D11ShaderResourceView* nullView[1] = { nullptr };
	context->PSSetShaderResources(0, 1, nullView);
}

void SsaoHelper::BuildFrustumFarCorners()
{
	float halfHeight = 0.5f * m_camera->GetFarWindowHeight();
	float halfWidth = 0.5f * m_camera->GetFarWindowWidth();
	float farZ = m_camera->GetFarZ();

	m_ssaoSettingsCB.Data.FrustumCorners[0] = XMFLOAT4(-halfWidth, -halfHeight, farZ, 0.0f);
	m_ssaoSettingsCB.Data.FrustumCorners[1] = XMFLOAT4(-halfWidth, +halfHeight, farZ, 0.0f);
	m_ssaoSettingsCB.Data.FrustumCorners[2] = XMFLOAT4(+halfWidth, +halfHeight, farZ, 0.0f);
	m_ssaoSettingsCB.Data.FrustumCorners[3] = XMFLOAT4(+halfWidth, -halfHeight, farZ, 0.0f);
}

void SsaoHelper::BuildFullScreenQuad()
{
	Basic32 v[4];
	v[0].Pos = XMFLOAT3(-1.0f, -1.0f, 0.0f);
	v[1].Pos = XMFLOAT3(-1.0f, +1.0f, 0.0f);
	v[2].Pos = XMFLOAT3(+1.0f, +1.0f, 0.0f);
	v[3].Pos = XMFLOAT3(+1.0f, -1.0f, 0.0f);
	// Store far plane frustum corner indices in Normal.x slot
	v[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	v[1].Normal = XMFLOAT3(1.0f, 0.0f, 0.0f);
	v[2].Normal = XMFLOAT3(2.0f, 0.0f, 0.0f);
	v[3].Normal = XMFLOAT3(3.0f, 0.0f, 0.0f);
	v[0].Tex = XMFLOAT2(0.0f, 1.0f);
	v[1].Tex = XMFLOAT2(0.0f, 0.0f);
	v[2].Tex = XMFLOAT2(1.0f, 0.0f);
	v[3].Tex = XMFLOAT2(1.0f, 1.0f);

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(Basic32) * 4;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = v;
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, &vinitData, m_quadVB.GetAddressOf()));

	UINT indices[6] =
	{
		0, 1, 2,
		0, 2, 3
	};
	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * 6;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.StructureByteStride = 0;
	ibd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = indices;

	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&ibd, &iinitData, m_quadIB.GetAddressOf()));
}

void SsaoHelper::BuildTextureViews()
{
	ResetTextureViews();

	ID3D11Device* device = m_deviceResources->GetD3DDevice();
	Windows::Foundation::Size renderTargetSize = m_deviceResources->GetRenderTargetSize();
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = lround(renderTargetSize.Width);
	texDesc.Height = lround(renderTargetSize.Height);
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;
	ComPtr<ID3D11Texture2D> normalDepthTex;
	DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, 0, normalDepthTex.GetAddressOf()));
	DX::ThrowIfFailed(device->CreateShaderResourceView(normalDepthTex.Get(), 0, m_normalDepthSRV.GetAddressOf()));
	DX::ThrowIfFailed(device->CreateRenderTargetView(normalDepthTex.Get(), 0, m_normalDepthRTV.GetAddressOf()));
	
	// We must create a depth-stencil dsv which matches the normal-depth render target.
 	texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	ComPtr<ID3D11Texture2D> depthMap;
	ThrowIfFailed(device->CreateTexture2D(&texDesc, 0, depthMap.GetAddressOf()));
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	ThrowIfFailed(device->CreateDepthStencilView(depthMap.Get(), &dsvDesc, m_depthStencilDSV.GetAddressOf()));

	// Render ambient map at half resolution.
	texDesc.Width = (UINT)(renderTargetSize.Width / 2.0f);
	texDesc.Height = (UINT)(renderTargetSize.Height / 2.0f);
	texDesc.Format = DXGI_FORMAT_R16_FLOAT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	ComPtr<ID3D11Texture2D> ambientTex0;
	DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, 0, ambientTex0.GetAddressOf()));
	DX::ThrowIfFailed(device->CreateShaderResourceView(ambientTex0.Get(), 0, m_ambientSRV0.GetAddressOf()));
	DX::ThrowIfFailed(device->CreateRenderTargetView(ambientTex0.Get(), 0, m_ambientRTV0.GetAddressOf()));
	ComPtr<ID3D11Texture2D> ambientTex1;
	DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, 0, &ambientTex1));
	DX::ThrowIfFailed(device->CreateShaderResourceView(ambientTex1.Get(), 0, m_ambientSRV1.GetAddressOf()));
	DX::ThrowIfFailed(device->CreateRenderTargetView(ambientTex1.Get(), 0, m_ambientRTV1.GetAddressOf()));
}

void SsaoHelper::BuildRandomVectorTexture()
{
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = 256;
	texDesc.Height = 256;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = { 0 };
	initData.SysMemPitch = 256 * sizeof(XMCOLOR);
	XMCOLOR color[256 * 256];
	for (int i = 0; i < 256; ++i)
	{
		for (int j = 0; j < 256; ++j)
		{
			XMFLOAT3 v(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());

			color[i * 256 + j] = XMCOLOR(v.x, v.y, v.z, 0.0f);
		}
	}
	initData.pSysMem = color;

	ComPtr<ID3D11Texture2D> tex;
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, &initData, tex.GetAddressOf()));
	DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(tex.Get(), 0, m_randomVectorSRV.GetAddressOf()));
}

void SsaoHelper::BuildOffsetVectors()
{
	// Start with 14 uniformly distributed vectors.  We choose the 8 corners of the cube
	// and the 6 center points along each cube face.  We always alternate the points on 
	// opposites sides of the cubes.  This way we still get the vectors spread out even
	// if we choose to use less than 14 samples.

	// 8 cube corners
	auto& offsets = m_ssaoSettingsCB.Data.OffsetVectors;
	offsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
	offsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);
	offsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
	offsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);
	offsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
	offsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);
	offsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
	offsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);
	// 6 centers of cube faces
	offsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	offsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);
	offsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	offsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);
	offsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	offsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

	for (int i = 0; i < 14; ++i)
	{
		// Create random lengths in [0.25, 1.0].
		float s = MathHelper::RandF(0.25f, 1.0f);
		XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&offsets[i]));
		XMStoreFloat4(&offsets[i], v);
	}
}

void SsaoHelper::ResetTextureViews()
{
	m_normalDepthRTV.Reset();
	m_normalDepthSRV.Reset();
	m_depthStencilDSV.Reset();
	m_ambientRTV0.Reset();
	m_ambientSRV0.Reset();
	m_ambientRTV1.Reset();
	m_ambientSRV1.Reset();
}