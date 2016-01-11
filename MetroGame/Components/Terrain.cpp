#include "pch.h"
#include "Terrain.h"
#include <algorithm>
#include <vector>
#include <sstream>
#include <ppl.h>
#include <DirectXPackedVector.h>
#include "Common/DirectXHelper.h"
#include "Common/GeometryGenerator.h"
#include "Common/MathHelper.h"
#include "Common/ShaderChangement.h"
#include "Common/RenderStateMgr.h"

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;
using namespace DirectX::PackedVector;

using namespace DX;

int Terrain::m_signatureIndex = 0;

Terrain::Terrain(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>>& perFrameCB,
	const std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>>& perObjectCB)
	: m_deviceResources(deviceResources), m_perFrameCB(perFrameCB),m_perObjectCB(perObjectCB),
	m_numPatchVertices(0), m_numPatchQuadFaces(0), m_numPatchVertRows(0), m_numPatchVertCols(0),
	m_renderOptions(TerrainRenderOption::Light3Tex), m_initialized(false), m_loadingComplete(false)
{
	m_terrainMat.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_terrainMat.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_terrainMat.Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 64.0f);
	m_terrainMat.Reflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	++m_signatureIndex;
	std::wostringstream wos;
	wos << m_signatureBase << m_signatureIndex;
	m_textureArraySignature = wos.str();
}

void Terrain::Initialize(const TerrainInitInfo& initInfo)
{
	m_initInfo = initInfo;
	// Divide height map into patches such that each patch has CellsPerPatch.
	m_numPatchVertRows = ((m_initInfo.HeightmapHeight - 1) / CellsPerPatch) + 1;
	m_numPatchVertCols = ((m_initInfo.HeightmapWidth - 1) / CellsPerPatch) + 1;

	m_numPatchVertices = m_numPatchVertRows*m_numPatchVertCols;
	m_numPatchQuadFaces = (m_numPatchVertRows - 1)*(m_numPatchVertCols - 1);

	LoadHeightmap();
	Smooth();
	CalcAllPatchBoundsY();
	CalcAllNormal();

	m_initialized = true;
}

concurrency::task<void> Terrain::CreateDeviceDependentResourcesAsync()
{
	// Must run on the main thread
	assert(IsMainThread());

	if (!m_initialized)
	{
		OutputDebugString(L"The terrain hasn't been initialized!");
		return concurrency::task_from_result();
	}

	// Initialize constant buffer
	m_terrainSettingsCB.Initialize(m_deviceResources->GetD3DDevice());
	m_frustumCB.Initialize(m_deviceResources->GetD3DDevice());
	m_terrainSettingsCB.Data.MaxDist = m_initInfo.MaxDist;
	m_terrainSettingsCB.Data.MaxTess = m_initInfo.MaxTess;
	m_terrainSettingsCB.Data.MinDist = m_initInfo.MinDist;
	m_terrainSettingsCB.Data.MinTess = m_initInfo.MinTess;
	m_terrainSettingsCB.Data.TexelCellSpaceU = 1.0f / m_initInfo.HeightmapWidth;
	m_terrainSettingsCB.Data.TexelCellSpaceV = 1.0f / m_initInfo.HeightmapHeight;
	m_terrainSettingsCB.Data.TexScale = m_initInfo.TexScale;
	m_terrainSettingsCB.Data.WorldCellSpace = m_initInfo.CellSpacing;
	m_terrainSettingsCB.ApplyChanges(m_deviceResources->GetD3DDeviceContext());

	auto shaderMgr = ShaderMgr::Instance();
	auto textureMgr = TextureMgr::Instance();


	std::vector<concurrency::task<void>> CreateTasks;

	// Load shaders
	CreateTasks.push_back(shaderMgr->GetVSAsync(L"TerrainBaseVS.cso", InputLayoutType::PosTexBound)
		.then([=](ID3D11VertexShader* vs)
	{
		m_terrainVS = vs;
		m_terrainInputLayout = shaderMgr->GetInputLayout(InputLayoutType::PosTexBound);
	}));
	CreateTasks.push_back(shaderMgr->GetHSAsync(L"TerrainBaseHS.cso")
		.then([=](ID3D11HullShader* hs) {m_terrainHS = hs; }));
	CreateTasks.push_back(shaderMgr->GetDSAsync(L"TerrainBaseDS.cso")
		.then([=](ID3D11DomainShader* ds) {m_terrainDS = ds; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"TerrainLight3PS.cso")
		.then([=](ID3D11PixelShader* ps) {m_terrainLight3PS = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"TerrainLight3TexPS.cso")
		.then([=](ID3D11PixelShader* ps) {m_terrainLight3TexPS = ps; }));
	CreateTasks.push_back(shaderMgr->GetPSAsync(L"TerrainLight3TexFogPS.cso")
		.then([=](ID3D11PixelShader* ps) {m_terrainLight3TexFogPS = ps; }));

	// Load texture
	std::vector<std::wstring> layerFilenames;
	layerFilenames.push_back(m_initInfo.LayerMapFilename0);
	layerFilenames.push_back(m_initInfo.LayerMapFilename1);
	layerFilenames.push_back(m_initInfo.LayerMapFilename2);
	layerFilenames.push_back(m_initInfo.LayerMapFilename3);
	layerFilenames.push_back(m_initInfo.LayerMapFilename4);
	CreateTasks.push_back(textureMgr->GetTextureArrayAsync(layerFilenames, m_textureArraySignature)
		.then([=](ID3D11ShaderResourceView* srv) {m_layerMapArraySRV = srv; }));
	CreateTasks.push_back(textureMgr->GetTextureAsync(m_initInfo.BlendMapFilename)
		.then([=](ID3D11ShaderResourceView* srv) {m_blendMapSRV = srv; }));

	// Once both shaders are loaded, create the mesh.
	// Note this is pretty time consuming. So put it to the background.
	return concurrency::when_all(CreateTasks.begin(), CreateTasks.end())
		.then([=]()
	{
		// Build input data
		BuildQuadPatchVB();
		BuildQuadPatchIB();
		BuildHeightmapSRV();

		m_loadingComplete = true;
	});
}

void Terrain::Render()
{
	if (!m_loadingComplete)
		return;

	auto renderStateMgr = RenderStateMgr::Instance();
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	// Set IA stage.
	UINT stride = sizeof(PosTexBound);
	UINT offset = 0;
	if (ShaderChangement::PrimitiveType != D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST)
	{
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
		ShaderChangement::PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	}
	if (ShaderChangement::InputLayout != m_terrainInputLayout.Get())
	{
		context->IASetInputLayout(m_terrainInputLayout.Get());
		ShaderChangement::InputLayout = m_terrainInputLayout.Get();
	}
	// Bind VB and IB
	context->IASetVertexBuffers(0, 1, m_quadPatchVB.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_quadPatchIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Bind shaders, constant buffers, srvs and samplers
	ID3D11Buffer* cbuffers0[3] = { m_perFrameCB->GetBuffer(), m_terrainSettingsCB.GetBuffer(), m_frustumCB.GetBuffer() };
	ID3D11Buffer* cbuffers1[3] = { m_perFrameCB->GetBuffer(), m_perObjectCB->GetBuffer(), m_terrainSettingsCB.GetBuffer() };
	ID3D11SamplerState* samplers[2] = { renderStateMgr->LinearMipPointSam(), renderStateMgr->LinearSam() };
	// Update constant buffer.
	m_perObjectCB->Data.Mat = m_terrainMat;
	m_perObjectCB->ApplyChanges(context);
	XMFLOAT4X4 VP;		
	XMStoreFloat4x4(&VP, XMMatrixTranspose(XMLoadFloat4x4(&m_perFrameCB->Data.ViewProj)));
	ExtractFrustumPlanes(m_frustumCB.Data.WorldFrustumPlanes, VP);
	m_frustumCB.ApplyChanges(context);
	// vs
	context->VSSetShader(m_terrainVS.Get(), 0, 0);
	ShaderChangement::VS = m_terrainVS.Get();
	context->VSSetSamplers(0, 1, samplers);
	context->VSSetShaderResources(0, 1, m_heightMapSRV.GetAddressOf());
	// hs
	context->HSSetShader(m_terrainHS.Get(), 0, 0);
	context->HSSetConstantBuffers(0, 3, cbuffers0);
	// ds
	context->DSSetShader(m_terrainDS.Get(), 0, 0);
	context->DSSetConstantBuffers(0, 2, cbuffers0);
	context->DSSetSamplers(0, 1, samplers);
	context->DSSetShaderResources(0, 1, m_heightMapSRV.GetAddressOf());
	// ps
	switch (m_renderOptions)
	{
	case TerrainRenderOption::Light3:		// Light
		context->PSSetShader(m_terrainLight3PS.Get(), 0, 0);
		ShaderChangement::PS = m_terrainLight3PS.Get();
		break;
	case TerrainRenderOption::Light3Tex:		// LightTex
		context->PSSetShader(m_terrainLight3TexPS.Get(), 0, 0);
		ShaderChangement::PS = m_terrainLight3TexPS.Get();
		break;
	case TerrainRenderOption::Light3TexFog:		// LightTexFog
		context->PSSetShader(m_terrainLight3TexFogPS.Get(), 0, 0);
		ShaderChangement::PS = m_terrainLight3TexFogPS.Get();
		break;
	default:
		throw ref new Platform::InvalidArgumentException("No such render option");
	}
	context->PSSetConstantBuffers(0, 3, cbuffers1);
	context->PSSetSamplers(0, 2, samplers);
	ID3D11ShaderResourceView* srvs[] = { m_layerMapArraySRV.Get(), m_blendMapSRV.Get(), m_heightMapSRV.Get() };
	context->PSSetShaderResources(0, 3, srvs);

	context->DrawIndexed(m_numPatchQuadFaces * 4, 0, 0);

	// Clear
	context->HSSetShader(nullptr, 0, 0);
	context->DSSetShader(nullptr, 0, 0);
	ShaderChangement::DS = nullptr;
	ShaderChangement::HS = nullptr;
}

void Terrain::ReleaseDeviceDependentResources()
{
	m_loadingComplete = true;

	m_terrainSettingsCB.Reset();
	m_frustumCB.Reset();
	m_quadPatchVB.Reset();
	m_quadPatchIB.Reset();
	m_terrainInputLayout.Reset();
	m_terrainVS.Reset();
	m_terrainHS.Reset();
	m_terrainDS.Reset();
	m_terrainLight3PS.Reset();
	m_terrainLight3TexPS.Reset();
	m_terrainLight3TexFogPS.Reset();
	m_layerMapArraySRV.Reset();
	m_blendMapSRV.Reset();
	m_heightMapSRV.Reset();
}

void Terrain::BuildQuadPatchVB()
{
	std::vector<PosTexBound> patchVertices(m_numPatchVertRows*m_numPatchVertCols);

	float halfWidth = 0.5f*GetWidth();
	float halfDepth = 0.5f*GetDepth();
	float patchWidth = GetWidth() / (m_numPatchVertCols - 1);
	float patchDepth = GetDepth() / (m_numPatchVertRows - 1);
	float du = 1.0f / (m_numPatchVertCols - 1);
	float dv = 1.0f / (m_numPatchVertRows - 1);

	for (UINT i = 0; i < m_numPatchVertRows; ++i)
	{
		float z = halfDepth - i*patchDepth;
		for (UINT j = 0; j < m_numPatchVertCols; ++j)
		{
			float x = -halfWidth + j*patchWidth;

			patchVertices[i*m_numPatchVertCols + j].Pos = XMFLOAT3(x, 0.0f, z);

			// Stretch texture over grid.
			patchVertices[i*m_numPatchVertCols + j].Tex.x = j*du;
			patchVertices[i*m_numPatchVertCols + j].Tex.y = i*dv;
		}
	}

	// Store axis-aligned bounding box y-bounds in upper-left patch corner.
	for (UINT i = 0; i < m_numPatchVertRows - 1; ++i)
	{
		for (UINT j = 0; j < m_numPatchVertCols - 1; ++j)
		{
			UINT patchID = i*(m_numPatchVertCols - 1) + j;
			patchVertices[i*m_numPatchVertCols + j].BoundsY = m_patchBoundsY[patchID];
		}
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(PosTexBound) * patchVertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &patchVertices[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&vbd, &vinitData, m_quadPatchVB.GetAddressOf()));
}

void Terrain::BuildQuadPatchIB()
{
	std::vector<UINT> indices(m_numPatchQuadFaces * 4); // 4 indices per quad face

														 // Iterate over each quad and compute indices.
	int k = 0;
	for (UINT i = 0; i < m_numPatchVertRows - 1; ++i)
	{
		for (UINT j = 0; j < m_numPatchVertCols - 1; ++j)
		{
			// Top row of 2x2 quad patch
			indices[k] = i*m_numPatchVertCols + j;
			indices[k + 1] = i*m_numPatchVertCols + j + 1;

			// Bottom row of 2x2 quad patch
			indices[k + 2] = (i + 1)*m_numPatchVertCols + j;
			indices[k + 3] = (i + 1)*m_numPatchVertCols + j + 1;

			k += 4; // next quad
		}
	}

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &indices[0];
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(&ibd, &iinitData, m_quadPatchIB.GetAddressOf()));
}

void Terrain::BuildHeightmapSRV()
{
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = m_initInfo.HeightmapWidth;
	texDesc.Height = m_initInfo.HeightmapHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	// HALF is defined in DirectXPackedVector.h, for storing 16-bit float.
	std::vector<HALF> hmap(m_heightmap.size());
	std::transform(m_heightmap.begin(), m_heightmap.end(), hmap.begin(), XMConvertFloatToHalf);
	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = &hmap[0];
	data.SysMemPitch = m_initInfo.HeightmapWidth*sizeof(HALF);
	data.SysMemSlicePitch = 0;

	ComPtr<ID3D11Texture2D> hmapTex;
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, &data, hmapTex.GetAddressOf()));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(hmapTex.Get(), &srvDesc, m_heightMapSRV.GetAddressOf()));
}

void Terrain::LoadHeightmap()
{
	// Read binary data
	std::shared_ptr<std::vector<byte>> heightData = ReadData(m_initInfo.HeightMapFilename);

	if (heightData->size() != m_initInfo.HeightmapHeight * m_initInfo.HeightmapWidth)
		throw ref new Platform::InvalidArgumentException("Terrain initialize data doesn't match the height map provided.");
	// Copy the array data into a float array and scale it.
	m_heightmap.resize(m_initInfo.HeightmapHeight * m_initInfo.HeightmapWidth, 0);
	for (UINT i = 0; i < m_initInfo.HeightmapHeight * m_initInfo.HeightmapWidth; ++i)
	{
		m_heightmap[i] = ((*heightData)[i] / 255.0f)*m_initInfo.HeightScale;
	}
}

void Terrain::Smooth()
{
	std::vector<float> dest(m_heightmap.size());
	// Speed up
	concurrency::parallel_for(UINT(0), m_initInfo.HeightmapHeight, [&](UINT i)
	{
		for (UINT j = 0; j < m_initInfo.HeightmapWidth; ++j)
		{
			dest[i*m_initInfo.HeightmapWidth + j] = Average(i, j);
		}
	});

	// Replace the old height map with the filtered one.
	m_heightmap = dest;
}

bool Terrain::InBounds(int i, int j)
{
	// True if ij are valid indices; false otherwise.
	return
		i >= 0 && i < (int)m_initInfo.HeightmapHeight &&
		j >= 0 && j < (int)m_initInfo.HeightmapWidth;
}

float Terrain::Average(int i, int j)
{
	// Function computes the average height of the ij element.
	// It averages itself with its eight neighbor pixels.  Note
	// that if a pixel is missing neighbor, we just don't include it
	// in the average--that is, edge pixels don't have a neighbor pixel.
	//
	// ----------
	// | 1| 2| 3|
	// ----------
	// |4 |ij| 6|
	// ----------
	// | 7| 8| 9|
	// ----------

	float avg = 0.0f;
	float num = 0.0f;

	// Use int to allow negatives.  If we use UINT, @ i=0, m=i-1=UINT_MAX
	// and no iterations of the outer for loop occur.
	for (int m = i - 1; m <= i + 1; ++m)
	{
		for (int n = j - 1; n <= j + 1; ++n)
		{
			if (InBounds(m, n))
			{
				avg += m_heightmap[m*m_initInfo.HeightmapWidth + n];
				num += 1.0f;
			}
		}
	}

	return avg / num;
}

void Terrain::CalcAllPatchBoundsY()
{
	m_patchBoundsY.resize(m_numPatchQuadFaces);

	// Speed up for each patch
	concurrency::parallel_for(UINT(0), m_numPatchVertRows - 1, [&](UINT i)
	{
		for (UINT j = 0; j < m_numPatchVertCols - 1; ++j)
		{
			CalcPatchBoundsY(i, j);
		}
	});
}

void Terrain::CalcPatchBoundsY(UINT i, UINT j)
{
	// Scan the height map values this patch covers and compute the min/max height.

	UINT x0 = j*CellsPerPatch;
	UINT x1 = (j + 1)*CellsPerPatch;

	UINT y0 = i*CellsPerPatch;
	UINT y1 = (i + 1)*CellsPerPatch;

	float minY = +MathHelper::Infinity;
	float maxY = -MathHelper::Infinity;
	for (UINT y = y0; y <= y1; ++y)
	{
		for (UINT x = x0; x <= x1; ++x)
		{
			UINT k = y*m_initInfo.HeightmapWidth + x;
			minY = MathHelper::Min(minY, m_heightmap[k]);
			maxY = MathHelper::Max(maxY, m_heightmap[k]);
		}
	}

	UINT patchID = i*(m_numPatchVertCols - 1) + j;
	m_patchBoundsY[patchID] = XMFLOAT2(minY, maxY);
}

void Terrain::CalcAllNormal()
{
	m_normal.resize(m_heightmap.size());

	// Speed up
	concurrency::parallel_for(UINT(0), m_initInfo.HeightmapHeight, [&](UINT i)
	{
		for (UINT j = 0; j < m_initInfo.HeightmapWidth; ++j)
		{
			CalcNormal(i, j);
		}
	});
}

void Terrain::CalcNormal(UINT i, UINT j)
{
	//
	// Estimate normal and tangent using central differences.
	//

	// do clamp
	float leftY = m_heightmap[i*m_initInfo.HeightmapWidth + (j == 0 ? 0 : j - 1)];
	float rightY = m_heightmap[i*m_initInfo.HeightmapWidth + min(j + 1, m_initInfo.HeightmapWidth - 1)];
	float bottomY = m_heightmap[min((i + 1), m_initInfo.HeightmapHeight - 1)*m_initInfo.HeightmapWidth + j];
	float topY = m_heightmap[(i == 0 ? 0 : i - 1)*m_initInfo.HeightmapWidth + j];

	XMVECTOR tangent = XMVector3Normalize(XMVectorSet(2.0f*m_initInfo.CellSpacing, rightY - leftY, 0.0f, 0.0f));
	XMVECTOR bitan = XMVector3Normalize(XMVectorSet(0.0f, bottomY - topY, -2.0f*m_initInfo.CellSpacing, 0.0f));
	XMVECTOR normal = XMVector3Cross(tangent, bitan);
	
	UINT normalID = i*m_initInfo.HeightmapWidth + j;
	XMStoreFloat3(&m_normal[normalID], normal);
}

float Terrain::GetHeight(float x, float z)const
{
	// Transform from terrain local space to "cell" space.
	float c = (x + 0.5f*GetWidth()) / m_initInfo.CellSpacing;
	float d = (z - 0.5f*GetDepth()) / -m_initInfo.CellSpacing;

	// Get the row and column we are in.
	int row = (int)floorf(d);
	int col = (int)floorf(c);

	// Grab the heights of the cell we are in.
	// A*--*B
	//  | /|
	//  |/ |
	// C*--*D
	float A = m_heightmap[row*m_initInfo.HeightmapWidth + col];
	float B = m_heightmap[row*m_initInfo.HeightmapWidth + col + 1];
	float C = m_heightmap[(row + 1)*m_initInfo.HeightmapWidth + col];
	float D = m_heightmap[(row + 1)*m_initInfo.HeightmapWidth + col + 1];

	// Where we are relative to the cell.
	float s = c - (float)col;
	float t = d - (float)row;

	// If upper triangle ABC.
	if (s + t <= 1.0f)
	{
		float uy = B - A;
		float vy = C - A;
		return A + s*uy + t*vy;
	}
	else // lower triangle DCB.
	{
		float uy = C - D;
		float vy = B - D;
		return D + (1.0f - s)*uy + (1.0f - t)*vy;
	}
}

XMVECTOR Terrain::GetNormal(float x, float z)const
{
	// Transform from terrain local space to "cell" space.
	float c = (x + 0.5f*GetWidth()) / m_initInfo.CellSpacing;
	float d = (z - 0.5f*GetDepth()) / -m_initInfo.CellSpacing;

	// Get the row and column we are in.
	int row = (int)floorf(d);
	int col = (int)floorf(c);

	// Grab the heights of the cell we are in.
	// A*--*B
	//  | /|
	//  |/ |
	// C*--*D
	XMVECTOR NA = XMLoadFloat3(&m_normal[row*m_initInfo.HeightmapWidth + col]);
	XMVECTOR NB = XMLoadFloat3(&m_normal[row*m_initInfo.HeightmapWidth + col + 1]);
	XMVECTOR NC = XMLoadFloat3(&m_normal[(row + 1)*m_initInfo.HeightmapWidth + col]);
	XMVECTOR ND = XMLoadFloat3(&m_normal[(row + 1)*m_initInfo.HeightmapWidth + col + 1]);

	// Where we are relative to the cell.
	float s = c - (float)col;
	float t = d - (float)row;

	// If upper triangle ABC.
	if (s + t <= 1.0f)
	{
		XMVECTOR uy = NB - NA;
		XMVECTOR vy = NC - NA;
		return XMVector3Normalize(NA + s*uy + t*vy);
	}
	else // lower triangle DCB.
	{
		XMVECTOR uy = NC - ND;
		XMVECTOR vy = NB - ND;
		return XMVector3Normalize(ND + (1.0f - s)*uy + (1.0f - t)*vy);
	}
}


