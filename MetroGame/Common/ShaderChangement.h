#pragma once


namespace DX
{
	class ShaderChangement
	{
	public:
		static D3D_PRIMITIVE_TOPOLOGY PrimitiveType;
		static ID3D11InputLayout* InputLayout;
		static ID3D11VertexShader* VS;
		static ID3D11PixelShader* PS;
		static ID3D11ComputeShader* CS;
		static ID3D11GeometryShader* GS;
		static ID3D11HullShader* HS;
		static ID3D11DomainShader* DS;
		static ID3D11RasterizerState* RSS;

	public:
		static void Reset();
	};
}
