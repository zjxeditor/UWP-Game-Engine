#pragma once

#include "pch.h"
#include "BasicLoader.h"
#include <ppltasks.h>
#include <collection.h>
#include <map>

/// Simple texture manager to avoid loading duplicate textures from file.  That can
/// happen, for example, if multiple meshes reference the same texture filename. 
namespace DX
{
	class TextureMgr
	{
	public:
		TextureMgr(const std::shared_ptr<BasicLoader>& loader);
		~TextureMgr() { m_instance = nullptr; }
		// Singleton
		static TextureMgr* Instance() { return m_instance; }

		ID3D11ShaderResourceView* GetTexture(std::wstring filename);
		concurrency::task<ID3D11ShaderResourceView*> GetTextureAsync(std::wstring filename);
		ID3D11ShaderResourceView* GetTextureArray(std::vector<std::wstring>& filenames, std::wstring name);
		concurrency::task<ID3D11ShaderResourceView*> GetTextureArrayAsync(std::vector<std::wstring>& filenames, std::wstring name);

	private:
		std::shared_ptr<BasicLoader> m_loader;
		std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textureSRV;

		static TextureMgr* m_instance;
	};
}

