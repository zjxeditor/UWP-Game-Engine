#include "pch.h"
#include "RenderStateMgr.h"
#include "DirectXHelper.h"

using namespace DX;

RenderStateMgr* RenderStateMgr::m_instance = nullptr;

RenderStateMgr::RenderStateMgr()
{
	if (m_instance == nullptr)
		m_instance = this;
	else
		throw ref new Platform::FailureException("Cannot create more than one RenderStateMgr!");
}

void RenderStateMgr::Initialize(ID3D11Device* device)
{
	// WireframeRS
	D3D11_RASTERIZER_DESC rasterizerDesc;
	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
	rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.DepthClipEnable = true;
	ThrowIfFailed(device->CreateRasterizerState(&rasterizerDesc, m_wireframeRS.GetAddressOf()));

	// NoCullRS
	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.DepthClipEnable = true;
	ThrowIfFailed(device->CreateRasterizerState(&rasterizerDesc, m_noCullRS.GetAddressOf()));

	// CullClockwiseRS
	// Note: Define such that we still cull back faces by making front faces CCW.
	// If we did not cull back faces, then we have to worry about the BackFace
	// property in the D3D11_DEPTH_STENCIL_DESC.
	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FrontCounterClockwise = true;
	rasterizerDesc.DepthClipEnable = true;
	ThrowIfFailed(device->CreateRasterizerState(&rasterizerDesc, m_cullClockwiseRS.GetAddressOf()));

	// DepthBiasRS
	// [From MSDN]
	// If the depth buffer currently bound to the output-merger stage has a UNORM format or
	// no depth buffer is bound the bias value is calculated like this: 
	//
	// Bias = (float)DepthBias * r + SlopeScaledDepthBias * MaxDepthSlope;
	//
	// where r is the minimum representable value > 0 in the depth-buffer format converted to float32.
	// [/End MSDN]
	// 
	// For a 24-bit depth buffer, r = 1 / 2^24.
	//
	// Example: DepthBias = 100000 ==> Actual DepthBias = 100000/2^24 = .006

	// You need to experiment with these values for your scene.
	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
	int bias = 100000;
	float clamp = 0.0f;
	float slope = 1.0f;
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FrontCounterClockwise = false;
	rasterizerDesc.DepthClipEnable = true;
	rasterizerDesc.DepthBias = bias;
	rasterizerDesc.DepthBiasClamp = clamp;
	rasterizerDesc.SlopeScaledDepthBias = slope;
	ThrowIfFailed(device->CreateRasterizerState(&rasterizerDesc, m_depthBiasRS.GetAddressOf()));

	// DepthBiasNoCullRS
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	ThrowIfFailed(device->CreateRasterizerState(&rasterizerDesc, m_depthBiasNoCullRS.GetAddressOf()));



	// EqualsDSS
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
	ThrowIfFailed(device->CreateDepthStencilState(&depthStencilDesc, m_equalsDSS.GetAddressOf()));

	// LessEqualsDSS
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	ThrowIfFailed(device->CreateDepthStencilState(&depthStencilDesc, m_lessEqualsDSS.GetAddressOf()));

	// NoDoubleBlendDSS
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xff;
	depthStencilDesc.StencilWriteMask = 0xff;

	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;

	// We are not rendering back facing polygons, so these settings do not matter.
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
	ThrowIfFailed(device->CreateDepthStencilState(&depthStencilDesc, m_noDoubleBlendDSS.GetAddressOf()));

	// DisableDSS
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	ThrowIfFailed(device->CreateDepthStencilState(&depthStencilDesc, m_disableDSS.GetAddressOf()));

	// NoDepthWritesDSS
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	ThrowIfFailed(device->CreateDepthStencilState(&depthStencilDesc, m_noWritesDSS.GetAddressOf()));

	// AlphaToCoverageBS
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	blendDesc.AlphaToCoverageEnable = true;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = false;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	ThrowIfFailed(device->CreateBlendState(&blendDesc, m_alphaToCoverageBS.GetAddressOf()));

	// TransparentBS
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	ThrowIfFailed(device->CreateBlendState(&blendDesc, m_transparentBS.GetAddressOf()));

	// NoRenderTargetWritesBS
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = false;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
	ThrowIfFailed(device->CreateBlendState(&blendDesc, m_noRenderTargetWritesBS.GetAddressOf()));

	// AdditiveBS
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	ThrowIfFailed(device->CreateBlendState(&blendDesc, m_additiveBS.GetAddressOf()));


	//D3D11_SAMPLER_DESC linearSamDesc;
	//linearSamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	//linearSamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	//linearSamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	//linearSamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	//linearSamDesc.MipLODBias = 0;
	//linearSamDesc.MaxAnisotropy = 1;
	//linearSamDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	//linearSamDesc.BorderColor[0] = 1.0f;
	//linearSamDesc.BorderColor[1] = 1.0f;
	//linearSamDesc.BorderColor[2] = 1.0f;
	//linearSamDesc.BorderColor[3] = 1.0f;
	//linearSamDesc.MinLOD = -3.402823466e+38F; // -FLT_MAX
	//linearSamDesc.MaxLOD = 3.402823466e+38F; // FLT_MAX

	// LinearSam
	D3D11_SAMPLER_DESC SamDesc;
	ZeroMemory(&SamDesc, sizeof(D3D11_SAMPLER_DESC));
	SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.MipLODBias = 0;
	SamDesc.MaxAnisotropy = 1;
	SamDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamDesc.BorderColor[0] = 1.0f;
	SamDesc.BorderColor[1] = 1.0f;
	SamDesc.BorderColor[2] = 1.0f;
	SamDesc.BorderColor[3] = 1.0f;
	SamDesc.MinLOD = -3.402823466e+38F; // -FLT_MAX
	SamDesc.MaxLOD = 3.402823466e+38F; // FLT_MAX
	ThrowIfFailed(device->CreateSamplerState(&SamDesc, m_linearSam.GetAddressOf()));

	// AnisotropicSam
	ZeroMemory(&SamDesc, sizeof(D3D11_SAMPLER_DESC));
	SamDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.MipLODBias = 0;
	SamDesc.MaxAnisotropy = 4;
	SamDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamDesc.BorderColor[0] = 1.0f;
	SamDesc.BorderColor[1] = 1.0f;
	SamDesc.BorderColor[2] = 1.0f;
	SamDesc.BorderColor[3] = 1.0f;
	SamDesc.MinLOD = -3.402823466e+38F; // -FLT_MAX
	SamDesc.MaxLOD = 3.402823466e+38F; // FLT_MAX
	ThrowIfFailed(device->CreateSamplerState(&SamDesc, m_anisotropicSam.GetAddressOf()));

	// PointSam
	ZeroMemory(&SamDesc, sizeof(D3D11_SAMPLER_DESC));
	SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.MipLODBias = 0;
	SamDesc.MaxAnisotropy = 1;
	SamDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamDesc.BorderColor[0] = 1.0f;
	SamDesc.BorderColor[1] = 1.0f;
	SamDesc.BorderColor[2] = 1.0f;
	SamDesc.BorderColor[3] = 1.0f;
	SamDesc.MinLOD = -3.402823466e+38F; // -FLT_MAX
	SamDesc.MaxLOD = 3.402823466e+38F; // FLT_MAX
	ThrowIfFailed(device->CreateSamplerState(&SamDesc, m_pointSam.GetAddressOf()));

	// LinearMipPointSam
	ZeroMemory(&SamDesc, sizeof(D3D11_SAMPLER_DESC));
	SamDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.MipLODBias = 0;
	SamDesc.MaxAnisotropy = 1;
	SamDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamDesc.BorderColor[0] = 1.0f;
	SamDesc.BorderColor[1] = 1.0f;
	SamDesc.BorderColor[2] = 1.0f;
	SamDesc.BorderColor[3] = 1.0f;
	SamDesc.MinLOD = -3.402823466e+38F; // -FLT_MAX
	SamDesc.MaxLOD = 3.402823466e+38F; // FLT_MAX
	ThrowIfFailed(device->CreateSamplerState(&SamDesc, m_linearMipPointSam.GetAddressOf()));

	// ShadowSam
	ZeroMemory(&SamDesc, sizeof(D3D11_SAMPLER_DESC));
	SamDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	SamDesc.MipLODBias = 0;
	SamDesc.MaxAnisotropy = 1;
	SamDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
	SamDesc.BorderColor[0] = 0.0f;
	SamDesc.BorderColor[1] = 0.0f;
	SamDesc.BorderColor[2] = 0.0f;
	SamDesc.BorderColor[3] = 0.0f;
	SamDesc.MinLOD = -3.402823466e+38F; // -FLT_MAX
	SamDesc.MaxLOD = 3.402823466e+38F; // FLT_MAX
	ThrowIfFailed(device->CreateSamplerState(&SamDesc, m_shadowSam.GetAddressOf()));
	
	// SsaoSam
	ZeroMemory(&SamDesc, sizeof(D3D11_SAMPLER_DESC));
	SamDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	SamDesc.MipLODBias = 0;
	SamDesc.MaxAnisotropy = 1;
	SamDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamDesc.BorderColor[0] = 0.0f;
	SamDesc.BorderColor[1] = 0.0f;
	SamDesc.BorderColor[2] = 0.0f;
	SamDesc.BorderColor[3] = 1.0e+5f;
	SamDesc.MinLOD = -3.402823466e+38F; // -FLT_MAX
	SamDesc.MaxLOD = 3.402823466e+38F; // FLT_MAX
	ThrowIfFailed(device->CreateSamplerState(&SamDesc, m_ssaoSam.GetAddressOf()));
}

void RenderStateMgr::Release()
{
	m_wireframeRS.Reset();
	m_noCullRS.Reset();
	m_cullClockwiseRS.Reset();
	m_depthBiasRS.Reset();

	m_equalsDSS.Reset();
	m_lessEqualsDSS.Reset();
	m_noDoubleBlendDSS.Reset();
	m_disableDSS.Reset();
	m_noWritesDSS.Reset();

	m_alphaToCoverageBS.Reset();
	m_transparentBS.Reset();
	m_noRenderTargetWritesBS.Reset();
	m_additiveBS.Reset();

	m_linearSam.Reset();
	m_anisotropicSam.Reset();
	m_pointSam.Reset();
	m_linearMipPointSam.Reset();
	m_shadowSam.Reset();
	m_ssaoSam.Reset();
}
