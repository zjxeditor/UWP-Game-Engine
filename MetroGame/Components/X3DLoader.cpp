#include "pch.h"
#include "X3DLoader.h"
#include "Common/DirectXHelper.h"
#include "Common/MathHelper.h"
#include <fstream>

using namespace Microsoft::WRL;
using namespace DXFramework;
using namespace DirectX;

using namespace DX;

// Note: do not use wifstream to read ASCII data. It is too slow.

void X3DLoader::LoadX3dStatic(const std::wstring& filename,
	std::vector<PosNormalTexTan>& vertices,
	std::vector<UINT>& indices,
	std::vector<Subset>& subsets,
	std::vector<X3dMaterial>& mats)
{
	// Read binary data
	std::ifstream fin(filename, std::ios::binary);

	UINT numMaterials = 0;
	UINT numSubsets = 0;
	UINT numVertices = 0;
	UINT numIndices = 0;

	if (fin)
	{
		fin.read((char*)&numMaterials, sizeof(int));
		fin.read((char*)&numSubsets, sizeof(int));
		fin.read((char*)&numVertices, sizeof(int));
		fin.read((char*)&numIndices, sizeof(int));

		ReadMaterials(fin, numMaterials, mats);
		ReadSubsetTable(fin, numSubsets, subsets);
		ReadVertices(fin, numVertices, vertices);
		ReadIndices(fin, numIndices, indices);

		return;
	}
	throw ref new Platform::FailureException("Can not load .m3d model!");
}

// Skinned mesh data isn't support now due to the difficulty of file format converter. 
// We are now work with FBX SDK and ASSIMP to export skinned animaton dat to our custom 
// .x3d file.
void X3DLoader::LoadX3dSkinned(const std::wstring& filename,
	std::vector<PosNormalTexTanSkinned>& vertices,
	std::vector<UINT>& indices,
	std::vector<Subset>& subsets,
	std::vector<X3dMaterial>& mats,
	SkinnedData& skinInfo)
{
	// Read binary data
	std::ifstream fin(filename);

	UINT numMaterials = 0;
	UINT numSubsets = 0;
	UINT numVertices = 0;
	UINT numIndices = 0;
	UINT numBones = 0;
	UINT numAnimationClips = 0;

	if (fin)
	{
		fin.read((char*)&numMaterials, sizeof(int));
		fin.read((char*)&numSubsets, sizeof(int));
		fin.read((char*)&numVertices, sizeof(int));
		fin.read((char*)&numIndices, sizeof(int));
		fin.read((char*)&numBones, sizeof(int));
		fin.read((char*)&numAnimationClips, sizeof(int));
		
		std::vector<XMFLOAT4X4> boneOffsets;
		std::vector<int> boneIndexToParentIndex;
		std::map<std::wstring, AnimationClip> animations;

		ReadMaterials(fin, numMaterials, mats);
		ReadSubsetTable(fin, numSubsets, subsets);
		ReadSkinnedVertices(fin, numVertices, vertices);
		ReadIndices(fin, numIndices, indices);
		ReadBoneOffsets(fin, numBones, boneOffsets);
		ReadBoneHierarchy(fin, numBones, boneIndexToParentIndex);
		ReadAnimationClips(fin, numBones, numAnimationClips, animations);

		skinInfo.Initialize(boneIndexToParentIndex, boneOffsets, animations);

		return;
	}
	throw ref new Platform::FailureException("Can not load .m3d model!");
}


void X3DLoader::ReadMaterials(std::ifstream& fin, UINT numMaterials, std::vector<X3dMaterial>& mats)
{
	mats.resize(numMaterials);
	std::string diffuseMapName, normalMapName;
	int intTemp;

	for (auto& item : mats)
	{
		fin.read((char*)&item.Mat.Ambient, sizeof(XMFLOAT3));
		fin.read((char*)&item.Mat.Diffuse, sizeof(XMFLOAT3));
		fin.read((char*)&item.Mat.Specular, sizeof(XMFLOAT3));
		fin.read((char*)&item.Mat.Specular.w, sizeof(float));
		fin.read((char*)&item.Mat.Reflect, sizeof(XMFLOAT3));
		fin.read((char*)&intTemp, sizeof(int));
		item.Effect = (EffectType)intTemp;
		fin.read((char*)&intTemp, sizeof(int));
		diffuseMapName.resize(intTemp);
		fin.read((char*)&diffuseMapName[0], intTemp);
		fin.read((char*)&intTemp, sizeof(int));
		normalMapName.resize(intTemp);
		fin.read((char*)&normalMapName[0], intTemp);

		item.DiffuseMap = std::wstring(diffuseMapName.begin(), diffuseMapName.end());
		item.NormalMap = std::wstring(normalMapName.begin(), normalMapName.end());
	}
}

void X3DLoader::ReadSubsetTable(std::ifstream& fin, UINT numSubsets, std::vector<Subset>& subsets)
{
	subsets.resize(numSubsets);

	for (auto& item : subsets)
	{
		fin.read((char*)&item.MtlIndex, sizeof(int));
		fin.read((char*)&item.VertexBase, sizeof(int));
		fin.read((char*)&item.IndexStart, sizeof(int));
		fin.read((char*)&item.IndexCount, sizeof(int));
	}
}

void X3DLoader::ReadVertices(std::ifstream& fin, UINT numVertices, std::vector<PosNormalTexTan>& vertices)
{
	vertices.resize(numVertices);

	for (auto& item : vertices)
	{
		fin.read((char*)&item.Pos, sizeof(XMFLOAT3));
		fin.read((char*)&item.Normal, sizeof(XMFLOAT3));
		fin.read((char*)&item.TangentU, sizeof(XMFLOAT3));
		fin.read((char*)&item.Tex, sizeof(XMFLOAT2));
	}
}

void X3DLoader::ReadIndices(std::ifstream& fin, UINT numIndices, std::vector<UINT>& indices)
{
	indices.resize(numIndices);

	for (auto& item : indices)
	{
		fin.read((char*)&item, sizeof(int));
	}
}

void X3DLoader::ReadSkinnedVertices(std::ifstream& fin, UINT numVertices, std::vector<PosNormalTexTanSkinned>& vertices)
{
	vertices.resize(numVertices);

	UINT boneIndices[4];
	float weights[4];
	for (auto& item : vertices)
	{
		fin.read((char*)&item.Pos, sizeof(XMFLOAT3));
		fin.read((char*)&item.Normal, sizeof(XMFLOAT3));
		fin.read((char*)&item.TangentU, sizeof(XMFLOAT3));
		fin.read((char*)&item.Tex, sizeof(XMFLOAT2));
		fin.read((char*)&weights[0], 4 * sizeof(float));
		fin.read((char*)&boneIndices[0], 4 * sizeof(int));

		item.Weights.x = weights[0];
		item.Weights.y = weights[1];
		item.Weights.z = weights[2];

		item.BoneIndices[0] = (byte)boneIndices[0];
		item.BoneIndices[1] = (byte)boneIndices[1];
		item.BoneIndices[2] = (byte)boneIndices[2];
		item.BoneIndices[3] = (byte)boneIndices[3];
	}
}

void X3DLoader::ReadBoneOffsets(std::ifstream& fin, UINT numBones, std::vector<XMFLOAT4X4>& boneOffsets)
{
	boneOffsets.resize(numBones);

	for (auto& item : boneOffsets)
		fin.read((char*)&item, sizeof(XMFLOAT4X4));
}

void X3DLoader::ReadBoneHierarchy(std::ifstream& fin, UINT numBones, std::vector<int>& boneIndexToParentIndex)
{
	boneIndexToParentIndex.resize(numBones);

	for (auto& item : boneIndexToParentIndex)
	{
		fin.read((char*)&item, sizeof(int));
	}
}

void X3DLoader::ReadAnimationClips(std::ifstream& fin, UINT numBones, UINT numAnimationClips,
	std::map<std::wstring, AnimationClip>& animations)
{
	for (UINT clipIndex = 0; clipIndex < numAnimationClips; ++clipIndex)
	{
		int intTemp;
		std::string clipName;
		fin.read((char*)&intTemp, sizeof(int));
		clipName.resize(intTemp);
		fin.read((char*)&clipName[0], intTemp);

		AnimationClip clip;
		clip.BoneAnimations.resize(numBones);

		for (UINT boneIndex = 0; boneIndex < numBones; ++boneIndex)
		{
			ReadBoneKeyframes(fin, numBones, clip.BoneAnimations[boneIndex]);
		}

		animations[std::wstring(clipName.begin(), clipName.end())] = clip;
	}
}

void X3DLoader::ReadBoneKeyframes(std::ifstream& fin, UINT numBones, BoneAnimation& boneAnimation)
{
	UINT numKeyframes = 0;
	fin.read((char*)&numKeyframes, sizeof(int));

	boneAnimation.Keyframes.resize(numKeyframes);
	for (auto& item : boneAnimation.Keyframes)
	{
		float t = 0.0f;
		XMFLOAT3 p(0.0f, 0.0f, 0.0f);
		XMFLOAT3 s(1.0f, 1.0f, 1.0f);
		XMFLOAT4 q(0.0f, 0.0f, 0.0f, 1.0f);
		fin.read((char*)&t, sizeof(float));
		fin.read((char*)&p, sizeof(XMFLOAT3));
		fin.read((char*)&s, sizeof(XMFLOAT3));
		fin.read((char*)&q, sizeof(XMFLOAT4));

		item.TimePos = t;
		item.Translation = p;
		item.Scale = s;
		item.RotationQuat = q;
	}
}

