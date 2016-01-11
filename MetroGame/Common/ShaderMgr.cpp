#include "pch.h"
#include "ShaderMgr.h"
#include "DirectXHelper.h"
#include "TaskExtensions.h"

using namespace DX;
using namespace Microsoft::WRL;

#pragma region Input inputLayout description

// SemanticName, SemanticIndex, Format, InputSlot, AlignedByteOffset, InputSlotClass, InstanceDataStepRate

D3D11_INPUT_ELEMENT_DESC PosDesc[1] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

D3D11_INPUT_ELEMENT_DESC Basic32Desc[3] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

D3D11_INPUT_ELEMENT_DESC PosNormalTexTanDesc[4] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

D3D11_INPUT_ELEMENT_DESC PosNormalTexTanSkinnedDesc[6] =
{
	{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "WEIGHTS",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BONEINDICES",  0, DXGI_FORMAT_R8G8B8A8_UINT,   0, 56, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

D3D11_INPUT_ELEMENT_DESC PosColorDesc[2] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

D3D11_INPUT_ELEMENT_DESC PointSizeDesc[2] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

D3D11_INPUT_ELEMENT_DESC PosTexBoundDesc[3] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

D3D11_INPUT_ELEMENT_DESC BasicParticleDesc[5] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "VELOCITY", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "SIZE",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "AGE",      0, DXGI_FORMAT_R32_FLOAT,       0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TYPE",     0, DXGI_FORMAT_R32_UINT,        0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

#pragma endregion

#pragma region StreamOut declaration

// Stream, SemanticName, SemanticIndex, StartComponent, ComponentCount, OutputSlot

D3D11_SO_DECLARATION_ENTRY  BasicParticleDecl[5] =
{
	{ 0, "POSITION", 0, 0, 3, 0 },
	{ 0, "VELOCITY", 0, 0, 3, 0 },
	{ 0, "SIZE", 0, 0, 2, 0 },
	{ 0, "AGE", 0, 0, 1, 0 },
	{ 0, "TYPE", 0, 0, 1, 0 }
};

#pragma endregion

ShaderMgr* ShaderMgr::m_instance = nullptr;

ShaderMgr::ShaderMgr(const std::shared_ptr<BasicLoader>& loader) : m_loader(loader)
{
	if (m_instance == nullptr)
		m_instance = this;
	else
		throw ref new Platform::FailureException("Cannot create more than one ShaderMgr!");
}

ID3D11InputLayout* ShaderMgr::GetInputLayout(InputLayoutType type)
{
	// Does it already exist?
	if (m_inputLayout.find(type) != m_inputLayout.end())
	{
		return m_inputLayout[type].Get();
	}
	else
	{
		return nullptr;
	}
}

ID3D11VertexShader* ShaderMgr::GetVS(std::wstring name, InputLayoutType type)
{
	// Does it already exist?
	if (m_vs.find(name) != m_vs.end())
	{
		return m_vs[name].Get();
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		ComPtr<ID3D11VertexShader> vs;
		ComPtr<ID3D11InputLayout> inputLayout;

		if (m_inputLayout.find(type) != m_inputLayout.end())
		{
			type = InputLayoutType::None;
		}

		switch (type)
		{
		case InputLayoutType::Pos:
			m_loader->LoadShader(file, PosDesc, 1, vs.GetAddressOf(), inputLayout.GetAddressOf());
			break;
		case InputLayoutType::Basic32:
			m_loader->LoadShader(file, Basic32Desc, 3, vs.GetAddressOf(), inputLayout.GetAddressOf());
			break;
		case InputLayoutType::PosNormalTexTan:
			m_loader->LoadShader(file, PosNormalTexTanDesc, 4, vs.GetAddressOf(), inputLayout.GetAddressOf());
			break;
		case InputLayoutType::PosNormalTexTanSkinned:
			m_loader->LoadShader(file, PosNormalTexTanSkinnedDesc, 6, vs.GetAddressOf(), inputLayout.GetAddressOf());
			break;
		case InputLayoutType::PosColor:
			m_loader->LoadShader(file, PosColorDesc, 2, vs.GetAddressOf(), inputLayout.GetAddressOf());
			break;
		case InputLayoutType::PointSize:
			m_loader->LoadShader(file, PointSizeDesc, 2, vs.GetAddressOf(), inputLayout.GetAddressOf());
			break;
		case InputLayoutType::PosTexBound:
			m_loader->LoadShader(file, PosTexBoundDesc, 3, vs.GetAddressOf(), inputLayout.GetAddressOf());
			break;
		case InputLayoutType::BasicParticle:
			m_loader->LoadShader(file, BasicParticleDesc, 5, vs.GetAddressOf(), inputLayout.GetAddressOf());
			break;
		case InputLayoutType::None:
			m_loader->LoadShader(file, nullptr, 0, vs.GetAddressOf(), nullptr);
			break;
		default:
			throw ref new Platform::InvalidArgumentException("No such input layout type!");
		}

		m_vs[name] = vs;

		if (type != InputLayoutType::None)
		{
			m_inputLayout[type] = inputLayout;
		}
		return vs.Get();
	}
}
concurrency::task<ID3D11VertexShader*> ShaderMgr::GetVSAsync(std::wstring name, InputLayoutType type)
{
	// Does it already exist?
	if (m_vs.find(name) != m_vs.end())
	{
		return concurrency::task_from_result(m_vs[name].Get());
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		std::shared_ptr<ComPtr<ID3D11VertexShader>> vs = std::make_shared<ComPtr<ID3D11VertexShader>>();
		std::shared_ptr<ComPtr<ID3D11InputLayout>> inputLayout = std::make_shared<ComPtr<ID3D11InputLayout>>();

		if (m_inputLayout.find(type) != m_inputLayout.end())
		{
			type = InputLayoutType::None;
		}

		switch (type)
		{
		case InputLayoutType::Pos:
			return m_loader->LoadShaderAsync(file, PosDesc, 1, vs->GetAddressOf(), inputLayout->GetAddressOf()).then([=]()
			{
				m_vs[name] = vs->Get();

				m_inputLayout[type] = inputLayout->Get();
				return vs->Get();
			});			
		case InputLayoutType::Basic32:
			return m_loader->LoadShaderAsync(file, Basic32Desc, 3, vs->GetAddressOf(), inputLayout->GetAddressOf()).then([=]()
			{
				m_vs[name] = vs->Get();

				m_inputLayout[type] = inputLayout->Get();
				return vs->Get();
			});
		case InputLayoutType::PosNormalTexTan:
			return m_loader->LoadShaderAsync(file, PosNormalTexTanDesc, 4, vs->GetAddressOf(), inputLayout->GetAddressOf()).then([=]()
			{
				m_vs[name] = vs->Get();

				m_inputLayout[type] = inputLayout->Get();
				return vs->Get();
			});
		case InputLayoutType::PosNormalTexTanSkinned:
			return m_loader->LoadShaderAsync(file, PosNormalTexTanSkinnedDesc, 6, vs->GetAddressOf(), inputLayout->GetAddressOf()).then([=]()
			{
				m_vs[name] = vs->Get();

				m_inputLayout[type] = inputLayout->Get();
				return vs->Get();
			});
		case InputLayoutType::PosColor:
			return m_loader->LoadShaderAsync(file, PosColorDesc, 2, vs->GetAddressOf(), inputLayout->GetAddressOf()).then([=]()
			{
				m_vs[name] = vs->Get();

				m_inputLayout[type] = inputLayout->Get();
				return vs->Get();
			});
		case InputLayoutType::PointSize:
			return m_loader->LoadShaderAsync(file, PointSizeDesc, 2, vs->GetAddressOf(), inputLayout->GetAddressOf()).then([=]()
			{
				m_vs[name] = vs->Get();

				m_inputLayout[type] = inputLayout->Get();
				return vs->Get();
			});
		case InputLayoutType::PosTexBound:
			return m_loader->LoadShaderAsync(file, PosTexBoundDesc, 3, vs->GetAddressOf(), inputLayout->GetAddressOf()).then([=]()
			{
				m_vs[name] = vs->Get();

				m_inputLayout[type] = inputLayout->Get();
				return vs->Get();
			});
		case InputLayoutType::BasicParticle:
			return m_loader->LoadShaderAsync(file, BasicParticleDesc, 5, vs->GetAddressOf(), inputLayout->GetAddressOf()).then([=]()
			{
				m_vs[name] = vs->Get();

				m_inputLayout[type] = inputLayout->Get();
				return vs->Get();
			});
		case InputLayoutType::None:
			return m_loader->LoadShaderAsync(file, nullptr, 0, vs->GetAddressOf(), nullptr).then([=]()
			{
				m_vs[name] = vs->Get();
				return vs->Get();
			});
		default:
			throw ref new Platform::InvalidArgumentException("No such input layout type!");
		}
	}
}

ID3D11PixelShader* ShaderMgr::GetPS(std::wstring name)
{
	// Does it already exist?
	if (m_ps.find(name) != m_ps.end())
	{
		return m_ps[name].Get();
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		ComPtr<ID3D11PixelShader> ps;

		m_loader->LoadShader(file, ps.GetAddressOf());

		m_ps[name] = ps.Get();
		
		return ps.Get();
	}
}
concurrency::task<ID3D11PixelShader*> ShaderMgr::GetPSAsync(std::wstring name)
{
	// Does it already exist?
	if (m_ps.find(name) != m_ps.end())
	{
		return concurrency::task_from_result(m_ps[name].Get());
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		std::shared_ptr<ComPtr<ID3D11PixelShader>> ps = std::make_shared<ComPtr<ID3D11PixelShader>>();

		return m_loader->LoadShaderAsync(file, ps->GetAddressOf()).then([=]()
		{
			m_ps[name] = ps->Get();
			return ps->Get();
		});
	}
}

ID3D11ComputeShader* ShaderMgr::GetCS(std::wstring name)
{
	// Does it already exist?
	if (m_cs.find(name) != m_cs.end())
	{
		return m_cs[name].Get();
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		ComPtr<ID3D11ComputeShader> cs;

		m_loader->LoadShader(file, cs.GetAddressOf());
		m_cs[name] = cs.Get();
		return cs.Get();
	}
}
concurrency::task<ID3D11ComputeShader*> ShaderMgr::GetCSAsync(std::wstring name)
{
	// Does it already exist?
	if (m_cs.find(name) != m_cs.end())
	{
		return concurrency::task_from_result(m_cs[name].Get());
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		std::shared_ptr<ComPtr<ID3D11ComputeShader>> cs = std::make_shared<ComPtr<ID3D11ComputeShader>>();

		return m_loader->LoadShaderAsync(file, cs->GetAddressOf()).then([=]()
		{
			m_cs[name] = cs->Get();
			return cs->Get();
		});
	}
}

ID3D11GeometryShader* ShaderMgr::GetGS(std::wstring name)
{
	// Does it already exist?
	if (m_gs.find(name) != m_gs.end())
	{
		return m_gs[name].Get();
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		ComPtr<ID3D11GeometryShader> gs;

		m_loader->LoadShader(file, gs.GetAddressOf());
		m_gs[name] = gs.Get();
		return gs.Get();
	}
}
concurrency::task<ID3D11GeometryShader*> ShaderMgr::GetGSAsync(std::wstring name)
{
	// Does it already exist?
	if (m_gs.find(name) != m_gs.end())
	{
		return concurrency::task_from_result(m_gs[name].Get());
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		std::shared_ptr<ComPtr<ID3D11GeometryShader>> gs = std::make_shared<ComPtr<ID3D11GeometryShader>>();

		return m_loader->LoadShaderAsync(file, gs->GetAddressOf()).then([=]()
		{
			m_gs[name] = gs->Get();
			return gs->Get();
		});
	}
}

ID3D11GeometryShader* ShaderMgr::GetGS(std::wstring name, StreamOutType type)
{
	// Does it already exist?
	if (m_gs.find(name) != m_gs.end())
	{
		return m_gs[name].Get();
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		ComPtr<ID3D11GeometryShader> gs;

		switch (type)
		{
		case StreamOutType::BasicParticle:
		{
			uint32 strides[] = { sizeof(BasicParticle) };
			m_loader->LoadShader(file, BasicParticleDecl, 5, strides, 1, 0, gs.GetAddressOf());
			break;
		}
		default:
			throw ref new Platform::InvalidArgumentException("No such stream out type!");
		}

		m_gs[name] = gs.Get();
		return gs.Get();
	}
}
concurrency::task<ID3D11GeometryShader*> ShaderMgr::GetGSAsync(std::wstring name, StreamOutType type)
{
	// Does it already exist?
	if (m_gs.find(name) != m_gs.end())
	{
		return concurrency::task_from_result(m_gs[name].Get());
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		std::shared_ptr<ComPtr<ID3D11GeometryShader>> gs = std::make_shared<ComPtr<ID3D11GeometryShader>>();

		switch (type)
		{
		case StreamOutType::BasicParticle:
		{
			uint32 strides[] = { sizeof(BasicParticle) };
			return m_loader->LoadShaderAsync(file, BasicParticleDecl, 5, strides, 1, 0, gs->GetAddressOf()).then([=]()
			{
				m_gs[name] = gs->Get();
				return gs->Get();
			});
		}
		default:
			throw ref new Platform::InvalidArgumentException("No such stream out type!");
		}
	}
}

ID3D11HullShader* ShaderMgr::GetHS(std::wstring name)
{
	// Does it already exist?
	if (m_hs.find(name) != m_hs.end())
	{
		return m_hs[name].Get();
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		ComPtr<ID3D11HullShader> hs;

		m_loader->LoadShader(file, hs.GetAddressOf());
		m_hs[name] = hs.Get();
		return hs.Get();
	}
}
concurrency::task<ID3D11HullShader*> ShaderMgr::GetHSAsync(std::wstring name)
{
	// Does it already exist?
	if (m_hs.find(name) != m_hs.end())
	{
		return concurrency::task_from_result(m_hs[name].Get());
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		std::shared_ptr<ComPtr<ID3D11HullShader>> hs = std::make_shared<ComPtr<ID3D11HullShader>>();

		return m_loader->LoadShaderAsync(file, hs->GetAddressOf()).then([=]()
		{
			m_hs[name] = hs->Get();
			return hs->Get();
		});
	}
}

ID3D11DomainShader* ShaderMgr::GetDS(std::wstring name)
{
	// Does it already exist?
	if (m_ds.find(name) != m_ds.end())
	{
		return m_ds[name].Get();
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		ComPtr<ID3D11DomainShader> ds;

		m_loader->LoadShader(file, ds.GetAddressOf());
		m_ds[name] = ds.Get();
		return ds.Get();
	}
}
concurrency::task<ID3D11DomainShader*> ShaderMgr::GetDSAsync(std::wstring name)
{
	// Does it already exist?
	if (m_ds.find(name) != m_ds.end())
	{
		return concurrency::task_from_result(m_ds[name].Get());
	}
	else
	{
		Platform::String^ file = ref new Platform::String(name.c_str());
		std::shared_ptr<ComPtr<ID3D11DomainShader>> ds = std::make_shared<ComPtr<ID3D11DomainShader>>();

		return m_loader->LoadShaderAsync(file, ds->GetAddressOf()).then([=]()
		{
			m_ds[name] = ds->Get();
			return ds->Get();
		});
	}
}
