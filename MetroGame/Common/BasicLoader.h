//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include "BasicReaderWriter.h"

// A simple loader class that provides support for loading shaders, textures,
// and meshes from files on disk. Provides synchronous and asynchronous methods.
namespace DX
{
	class BasicLoader
	{
	public:
		BasicLoader(
			ID3D11Device* d3dDevice,
			ID3D11DeviceContext* d3dContext,
			IWICImagingFactory2* wicFactory = nullptr
			);

		void LoadTexture(
			Platform::String^ filename,
			bool needMap,
			ID3D11Texture2D** texture,
			ID3D11ShaderResourceView** textureView
			);

		concurrency::task<void> LoadTextureAsync(
			Platform::String^ filename,
			bool needMap,
			ID3D11Texture2D** texture,
			ID3D11ShaderResourceView** textureView
			);

		// Must be called on the render thread
		void LoadTextureArray(
			Platform::Array<Platform::String^>^ filenames,
			ID3D11ShaderResourceView** textureView
			);
		// Must be called on the render thread
		concurrency::task<void> LoadTextureArrayAsync(
			Platform::Array<Platform::String^>^ filenames,
			ID3D11ShaderResourceView** textureView
			);

		void LoadShader(
			Platform::String^ filename,
			D3D11_INPUT_ELEMENT_DESC layoutDesc[],
			uint32 layoutDescNumElements,
			ID3D11VertexShader** shader,
			ID3D11InputLayout** layout
			);

		concurrency::task<void> LoadShaderAsync(
			Platform::String^ filename,
			D3D11_INPUT_ELEMENT_DESC layoutDesc[],
			uint32 layoutDescNumElements,
			ID3D11VertexShader** shader,
			ID3D11InputLayout** layout
			);

		void LoadShader(
			Platform::String^ filename,
			ID3D11PixelShader** shader
			);

		concurrency::task<void> LoadShaderAsync(
			Platform::String^ filename,
			ID3D11PixelShader** shader
			);

		void LoadShader(
			Platform::String^ filename,
			ID3D11ComputeShader** shader
			);

		concurrency::task<void> LoadShaderAsync(
			Platform::String^ filename,
			ID3D11ComputeShader** shader
			);

		void LoadShader(
			Platform::String^ filename,
			ID3D11GeometryShader** shader
			);

		concurrency::task<void> LoadShaderAsync(
			Platform::String^ filename,
			ID3D11GeometryShader** shader
			);

		void LoadShader(
			Platform::String^ filename,
			const D3D11_SO_DECLARATION_ENTRY* streamOutDeclaration,
			uint32 numEntries,
			const uint32* bufferStrides,
			uint32 numStrides,
			uint32 rasterizedStream,
			ID3D11GeometryShader** shader
			);

		concurrency::task<void> LoadShaderAsync(
			Platform::String^ filename,
			const D3D11_SO_DECLARATION_ENTRY* streamOutDeclaration,
			uint32 numEntries,
			const uint32* bufferStrides,
			uint32 numStrides,
			uint32 rasterizedStream,
			ID3D11GeometryShader** shader
			);

		void LoadShader(
			Platform::String^ filename,
			ID3D11HullShader** shader
			);

		concurrency::task<void> LoadShaderAsync(
			Platform::String^ filename,
			ID3D11HullShader** shader
			);

		void LoadShader(
			Platform::String^ filename,
			ID3D11DomainShader** shader
			);

		concurrency::task<void> LoadShaderAsync(
			Platform::String^ filename,
			ID3D11DomainShader** shader
			);

		/*void LoadMesh(
			Platform::String^ filename,
			ID3D11Buffer** vertexBuffer,
			ID3D11Buffer** indexBuffer,
			uint32* vertexCount,
			uint32* indexCount
			);*/

		/*concurrency::task<void> LoadMeshAsync(
			Platform::String^ filename,
			ID3D11Buffer** vertexBuffer,
			ID3D11Buffer** indexBuffer,
			uint32* vertexCount,
			uint32* indexCount
			);*/

	private:
		Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext>	m_d3dContext;
		Microsoft::WRL::ComPtr<IWICImagingFactory2> m_wicFactory;
		std::unique_ptr<BasicReaderWriter> m_basicReaderWriter;

		template <class DeviceChildType>
		inline void SetDebugName(
			DeviceChildType* object,
			Platform::String^ name
			);

		Platform::String^ GetExtension(
			Platform::String^ filename
			);

		void CreateTexture(
			bool decodeAsDDS,
			bool needMap,
			byte* data,
			uint32 dataSize,
			ID3D11Texture2D** texture,
			ID3D11ShaderResourceView** textureView,
			Platform::String^ debugName
			);

		void CreateInputLayout(
			byte* bytecode,
			uint32 bytecodeSize,
			D3D11_INPUT_ELEMENT_DESC* layoutDesc,
			uint32 layoutDescNumElements,
			ID3D11InputLayout** layout
			);

		/*void CreateMesh(
			byte* meshData,
			ID3D11Buffer** vertexBuffer,
			ID3D11Buffer** indexBuffer,
			uint32* vertexCount,
			uint32* indexCount,
			Platform::String^ debugName
			);*/
	};
}


