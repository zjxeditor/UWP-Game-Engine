#pragma once

#include "BasicLoader.h"
#include <map>
#include <ppltasks.h>

namespace DX
{
#pragma region Vertex definition
	// Basic 32-byte vertex structure.
	struct Basic32
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 Tex;
	};

	struct PosNormalTexTan
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 Tex;
		DirectX::XMFLOAT3 TangentU;
	};

	struct PosNormalTexTanSkinned
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 Tex;
		DirectX::XMFLOAT3 TangentU;
		DirectX::XMFLOAT3 Weights;
		byte BoneIndices[4];
	};

	struct PosColor
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT4 Color;
	};

	struct PointSize
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT2 Size;
	};

	struct PosTexBound
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT2 Tex;
		DirectX::XMFLOAT2 BoundsY;
	};

	struct BasicParticle
	{
		DirectX::XMFLOAT3 InitialPos;
		DirectX::XMFLOAT3 InitialVel;
		DirectX::XMFLOAT2 Size;
		float Age;
		unsigned int Type;
	};

	enum class InputLayoutType
	{
		None,
		Pos,
		Basic32,
		PosNormalTexTan,
		PosNormalTexTanSkinned,
		PosColor,
		PointSize,
		PosTexBound,
		BasicParticle
	};

	enum class StreamOutType
	{
		BasicParticle
	};

#pragma endregion


	class ShaderMgr
	{
	public:
		ShaderMgr(const std::shared_ptr<BasicLoader>& loader);
		~ShaderMgr() { m_instance = nullptr; }
		// Singleton
		static ShaderMgr* Instance() { return m_instance; }

		ID3D11InputLayout* GetInputLayout(InputLayoutType type);
		ID3D11VertexShader* GetVS(std::wstring name, InputLayoutType type);
		concurrency::task<ID3D11VertexShader*> GetVSAsync(std::wstring name, InputLayoutType type);
		ID3D11PixelShader* GetPS(std::wstring name);
		concurrency::task<ID3D11PixelShader*> GetPSAsync(std::wstring name);
		ID3D11ComputeShader* GetCS(std::wstring name);
		concurrency::task<ID3D11ComputeShader*> GetCSAsync(std::wstring name);
		ID3D11GeometryShader* GetGS(std::wstring name);
		concurrency::task<ID3D11GeometryShader*> GetGSAsync(std::wstring name);
		ID3D11GeometryShader* GetGS(std::wstring name, StreamOutType type);
		concurrency::task<ID3D11GeometryShader*> GetGSAsync(std::wstring name, StreamOutType type);
		ID3D11HullShader* GetHS(std::wstring name);
		concurrency::task<ID3D11HullShader*> GetHSAsync(std::wstring name);
		ID3D11DomainShader* GetDS(std::wstring name);
		concurrency::task<ID3D11DomainShader*> GetDSAsync(std::wstring name);

	private:
		std::shared_ptr<BasicLoader> m_loader;

		std::map<InputLayoutType, Microsoft::WRL::ComPtr<ID3D11InputLayout>> m_inputLayout;
		std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11VertexShader>> m_vs;
		std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11PixelShader>> m_ps;
		std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ComputeShader>> m_cs;
		std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11GeometryShader>> m_gs;
		std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11HullShader>> m_hs;
		std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11DomainShader>> m_ds;
		
		// Singleton
		static ShaderMgr* m_instance;
	};
}


