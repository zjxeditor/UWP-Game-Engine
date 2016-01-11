#include "pch.h"
#include "DirectXHelper.h"
#include "MathHelper.h"

using namespace DirectX;
using namespace Microsoft::WRL;

void DX::ExtractFrustumPlanes(XMFLOAT4 planes[6], const XMFLOAT4X4& M)
{
	//
	// Left
	//
	planes[0].x = M(0, 3) + M(0, 0);
	planes[0].y = M(1, 3) + M(1, 0);
	planes[0].z = M(2, 3) + M(2, 0);
	planes[0].w = M(3, 3) + M(3, 0);

	//
	// Right
	//
	planes[1].x = M(0, 3) - M(0, 0);
	planes[1].y = M(1, 3) - M(1, 0);
	planes[1].z = M(2, 3) - M(2, 0);
	planes[1].w = M(3, 3) - M(3, 0);

	//
	// Bottom
	//
	planes[2].x = M(0, 3) + M(0, 1);
	planes[2].y = M(1, 3) + M(1, 1);
	planes[2].z = M(2, 3) + M(2, 1);
	planes[2].w = M(3, 3) + M(3, 1);

	//
	// Top
	//
	planes[3].x = M(0, 3) - M(0, 1);
	planes[3].y = M(1, 3) - M(1, 1);
	planes[3].z = M(2, 3) - M(2, 1);
	planes[3].w = M(3, 3) - M(3, 1);

	//
	// Near
	//
	planes[4].x = M(0, 2);
	planes[4].y = M(1, 2);
	planes[4].z = M(2, 2);
	planes[4].w = M(3, 2);

	//
	// Far
	//
	planes[5].x = M(0, 3) - M(0, 2);
	planes[5].y = M(1, 3) - M(1, 2);
	planes[5].z = M(2, 3) - M(2, 2);
	planes[5].w = M(3, 3) - M(3, 2);

	// Normalize the plane equations.
	for (int i = 0; i < 6; ++i)
	{
		XMVECTOR v = XMPlaneNormalize(XMLoadFloat4(&planes[i]));
		XMStoreFloat4(&planes[i], v);
	}
}

void DX::CreateRandomTexture1DSRV(ID3D11Device* device, ID3D11ShaderResourceView** textureView)
{
	// 
	// Create the random data.
	//
	XMFLOAT4 randomValues[1024];

	for (int i = 0; i < 1024; ++i)
	{
		randomValues[i].x = MathHelper::RandF(-1.0f, 1.0f);
		randomValues[i].y = MathHelper::RandF(-1.0f, 1.0f);
		randomValues[i].z = MathHelper::RandF(-1.0f, 1.0f);
		randomValues[i].w = MathHelper::RandF(-1.0f, 1.0f);
	}

	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = randomValues;
	initData.SysMemPitch = 1024 * sizeof(XMFLOAT4);
	initData.SysMemSlicePitch = 0;

	//
	// Create the texture.
	//
	D3D11_TEXTURE1D_DESC texDesc;
	texDesc.Width = 1024;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;
	texDesc.ArraySize = 1;

	Microsoft::WRL::ComPtr<ID3D11Texture1D> randomTex;
	DX::ThrowIfFailed(device->CreateTexture1D(&texDesc, &initData, randomTex.GetAddressOf()));

	//
	// Create the resource view.
	//
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	viewDesc.Format = texDesc.Format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	viewDesc.Texture1D.MipLevels = texDesc.MipLevels;
	viewDesc.Texture1D.MostDetailedMip = 0;

	DX::ThrowIfFailed(device->CreateShaderResourceView(randomTex.Get(), &viewDesc, textureView));
}

Concurrency::task<std::shared_ptr<std::vector<byte>>> DX::ReadDataAsync(const std::wstring& filename)
{
	using namespace Windows::Storage;
	using namespace Concurrency;

	auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

	return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([](StorageFile^ file)
	{
		return FileIO::ReadBufferAsync(file);
	}).then([](Streams::IBuffer^ fileBuffer) -> std::shared_ptr<std::vector<byte>>
	{
		std::shared_ptr<std::vector<byte>> returnBuffer(new std::vector<byte>());
		returnBuffer->resize(fileBuffer->Length);
		Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer->data(), fileBuffer->Length));
		return returnBuffer;
	});
}

std::shared_ptr<std::vector<byte>> DX::ReadData(const std::wstring& filename)
{
	CREATEFILE2_EXTENDED_PARAMETERS extendedParams = { 0 };
	extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
	extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
	extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
	extendedParams.lpSecurityAttributes = nullptr;
	extendedParams.hTemplateFile = nullptr;

	Wrappers::FileHandle file(
		CreateFile2(
			filename.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			OPEN_EXISTING,
			&extendedParams
			)
		);
	if (file.Get() == INVALID_HANDLE_VALUE)
	{
		throw ref new Platform::FailureException();
	}

	FILE_STANDARD_INFO fileInfo = { 0 };
	if (!GetFileInformationByHandleEx(
		file.Get(),
		FileStandardInfo,
		&fileInfo,
		sizeof(fileInfo)
		))
	{
		throw ref new Platform::FailureException();
	}

	if (fileInfo.EndOfFile.HighPart != 0)
	{
		throw ref new Platform::OutOfMemoryException();
	}

	std::shared_ptr<std::vector<byte>> fileData(new std::vector<byte>());
	fileData->resize(fileInfo.EndOfFile.LowPart);

	if (!ReadFile(
		file.Get(),
		fileData->data(),
		fileData->size(),
		nullptr,
		nullptr
		))
	{
		throw ref new Platform::FailureException();
	}

	return fileData;
}

BoundingBox DX::MergeBoundingBox(const std::vector<DirectX::BoundingBox>& boxList)
{
	BoundingBox temp0, temp1;
	temp0 = boxList[0];
	for (UINT i = 1; i < boxList.size(); ++i)
	{
		BoundingBox::CreateMerged(temp1, temp0, boxList[i]);
		if (++i >= boxList.size())
			return temp1;
		BoundingBox::CreateMerged(temp0, temp1, boxList[i]);
	}
	return temp0;
}

BoundingSphere DX::MergeBoundingSphere(const std::vector<DirectX::BoundingSphere>& sphereList)
{
	BoundingSphere temp0, temp1;
	temp0 = sphereList[0];
	for (UINT i = 1; i < sphereList.size(); ++i)
	{
		BoundingSphere::CreateMerged(temp1, temp0, sphereList[i]);
		if (++i >= sphereList.size())
			return temp1;
		BoundingSphere::CreateMerged(temp0, temp1, sphereList[i]);
	}
	return temp0;
}
