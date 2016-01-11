#include "pch.h"
#include "TextureMgr.h"
#include "BasicLoader.h"

using namespace DX;
using namespace Microsoft::WRL;

TextureMgr* TextureMgr::m_instance = nullptr;

TextureMgr::TextureMgr(const std::shared_ptr<BasicLoader>& loader) : m_loader(loader)
{
	if (m_instance == nullptr)
		m_instance = this;
	else
		throw ref new Platform::FailureException("Cannot create more than one TextureMgr!");
}

ID3D11ShaderResourceView* TextureMgr::GetTexture(std::wstring filename)
{
	// Does it already exist?
	if(m_textureSRV.find(filename) != m_textureSRV.end())
	{
		return m_textureSRV[filename].Get();
	}
	else
	{
		Platform::String^ file = ref new Platform::String(filename.c_str());
		ComPtr<ID3D11ShaderResourceView> textureView;

		m_loader->LoadTexture(file, false, nullptr, textureView.GetAddressOf());
		m_textureSRV[filename] = textureView.Get();
		return textureView.Get();
	}
}
concurrency::task<ID3D11ShaderResourceView*> TextureMgr::GetTextureAsync(std::wstring filename)
{
	// Does it already exist?
	if (m_textureSRV.find(filename) != m_textureSRV.end())
	{
		return concurrency::task_from_result(m_textureSRV[filename].Get());
	}
	else
	{
		Platform::String^ file = ref new Platform::String(filename.c_str());
		std::shared_ptr<ComPtr<ID3D11ShaderResourceView>> textureView = std::make_shared<ComPtr<ID3D11ShaderResourceView>>();

		return m_loader->LoadTextureAsync(file, false, nullptr, textureView->GetAddressOf()).then([=](concurrency::task<void> t)
		{
			try
			{
				t.get();
			}
			catch (Platform::COMException^ e)
			{
				//Example output: The system cannot find the specified file.
				throw ref new Platform::FailureException("Cannot load file " + file);
			}

			m_textureSRV[filename] = textureView->Get();
			return textureView->Get();
		});
	}
}

ID3D11ShaderResourceView* TextureMgr::GetTextureArray(std::vector<std::wstring>& filenames, std::wstring name)
{
	// Does it already exist?
	if (m_textureSRV.find(name) != m_textureSRV.end())
	{
		return m_textureSRV[name].Get();
	}
	else
	{
		Platform::Array<Platform::String^>^ files = ref new Platform::Array<Platform::String^>(filenames.size());
		for (size_t i = 0; i < filenames.size(); ++i)
			files[i] = ref new Platform::String(filenames[i].c_str());

		ComPtr<ID3D11ShaderResourceView> textureView;

		m_loader->LoadTextureArray(files, textureView.GetAddressOf());
		m_textureSRV[name] = textureView.Get();
		return textureView.Get();
	}
}
concurrency::task<ID3D11ShaderResourceView*> TextureMgr::GetTextureArrayAsync(std::vector<std::wstring>& filenames, std::wstring name)
{
	// Does it already exist?
	if (m_textureSRV.find(name) != m_textureSRV.end())
	{
		return concurrency::task_from_result(m_textureSRV[name].Get());
	}
	else
	{
		Platform::Array<Platform::String^>^ files = ref new Platform::Array<Platform::String^>(filenames.size());
		for (size_t i = 0; i < filenames.size(); ++i)
			files[i] = ref new Platform::String(filenames[i].c_str());

		std::shared_ptr<ComPtr<ID3D11ShaderResourceView>> textureView = std::make_shared<ComPtr<ID3D11ShaderResourceView>>();

		return m_loader->LoadTextureArrayAsync(files, textureView->GetAddressOf()).then([=]()
		{
			m_textureSRV[name] = textureView->Get();
			return textureView->Get();
		});
	}
}