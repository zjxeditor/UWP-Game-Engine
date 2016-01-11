#pragma once

namespace DX
{
	class RenderStateMgr
	{
	public:
		RenderStateMgr();
		~RenderStateMgr() { m_instance = nullptr; }
		// Singleton
		static RenderStateMgr* Instance() { return m_instance; }
		
		void Initialize(ID3D11Device* device);
		void Release();

		ID3D11RasterizerState* WireFrameRS() { return m_wireframeRS.Get(); }
		ID3D11RasterizerState* NoCullRS() { return m_noCullRS.Get(); }
		ID3D11RasterizerState* CullClockwiseRS() { return m_cullClockwiseRS.Get(); }
		ID3D11RasterizerState* DepthBiasRS() { return m_depthBiasRS.Get(); }
		ID3D11RasterizerState* DepthBiasNoCullRS() { return m_depthBiasNoCullRS.Get(); }

		ID3D11DepthStencilState* EqualStateDSS() { return m_equalsDSS.Get(); }
		ID3D11DepthStencilState* LessEqualStateDSS() { return m_lessEqualsDSS.Get(); }
		ID3D11DepthStencilState* NoDoubleBlendDSS() { return m_noDoubleBlendDSS.Get(); }
		ID3D11DepthStencilState* DisableDSS() { return m_disableDSS.Get(); }
		ID3D11DepthStencilState* NoWritesDSS() { return m_noWritesDSS.Get(); }

		ID3D11BlendState* AlphaToCoverBS() { return m_alphaToCoverageBS.Get(); }
		ID3D11BlendState* TransparentBS() { return m_transparentBS.Get(); }
		ID3D11BlendState* NoRenderTargetWritesBS() { return m_noRenderTargetWritesBS.Get(); }
		ID3D11BlendState* AdditiveBS() { return m_additiveBS.Get(); }

		ID3D11SamplerState* LinearSam() { return m_linearSam.Get(); }
		ID3D11SamplerState* AnisotropicSam() { return m_anisotropicSam.Get(); }
		ID3D11SamplerState* PointSam() { return m_pointSam.Get(); }
		ID3D11SamplerState* LinearMipPointSam() { return m_linearMipPointSam.Get(); }
		ID3D11SamplerState* ShadowSam() { return m_shadowSam.Get(); }
		ID3D11SamplerState* SsaoSam() { return m_ssaoSam.Get(); }

	private:
		// Rasterizer states
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_wireframeRS;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_noCullRS;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_cullClockwiseRS;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_depthBiasRS;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_depthBiasNoCullRS;

		// Depth/stencil states
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_equalsDSS;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_lessEqualsDSS;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_noDoubleBlendDSS;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_disableDSS;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_noWritesDSS;

		// Blend states
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_alphaToCoverageBS;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_transparentBS;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_noRenderTargetWritesBS;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_additiveBS;

		// Sample filters
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_linearSam;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_anisotropicSam;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pointSam;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_linearMipPointSam;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_shadowSam;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_ssaoSam;
	
		static RenderStateMgr* m_instance;
	};
}


