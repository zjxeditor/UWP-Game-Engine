#include "pch.h"
#include "ShaderChangement.h"

using namespace DX;

D3D_PRIMITIVE_TOPOLOGY ShaderChangement::PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
ID3D11InputLayout* ShaderChangement::InputLayout = nullptr;
ID3D11VertexShader* ShaderChangement::VS = nullptr;
ID3D11PixelShader* ShaderChangement::PS = nullptr;
ID3D11ComputeShader* ShaderChangement::CS = nullptr;
ID3D11GeometryShader* ShaderChangement::GS = nullptr;
ID3D11HullShader* ShaderChangement::HS = nullptr;
ID3D11DomainShader* ShaderChangement::DS = nullptr;
ID3D11RasterizerState* ShaderChangement::RSS = nullptr;

void ShaderChangement::Reset()
{
	PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	InputLayout = nullptr;
	VS = nullptr;
	PS = nullptr;
	CS = nullptr;
	GS = nullptr;
	HS = nullptr;
	DS = nullptr;
	RSS = nullptr;
}

