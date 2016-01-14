#pragma once

#include <DirectXMath.h>
#include <sstream>
#include "Common/ShaderMgr.h"
#include "MeshGeometry.h"

namespace DXFramework
{
	class X3DLoader
	{
	public:
		static void LoadX3dStatic(const std::wstring& filename,
			std::vector<DX::PosNormalTexTan>& vertices,
			std::vector<UINT>& indices,
			std::vector<Subset>& subsets,
			std::vector<X3dMaterial>& mats);
		static void X3DLoader::LoadX3dSkinned(const std::wstring& filename,
			std::vector<DX::PosNormalTexTanSkinned>& vertices,
			std::vector<UINT>& indices,
			std::vector<Subset>& subsets,
			std::vector<X3dMaterial>& mats,
			SkinnedData& skinInfo);

	private:
		static void ReadMaterials(std::ifstream& fin, UINT numMaterials, std::vector<X3dMaterial>& mats);
		static void ReadSubsetTable(std::ifstream& fin, UINT numSubsets, std::vector<Subset>& subsets);
		static void ReadVertices(std::ifstream& fin, UINT numVertices, std::vector<DX::PosNormalTexTan>& vertices);
		static void ReadIndices(std::ifstream& fin, UINT numTriangles, std::vector<UINT>& indices);
		static void ReadSkinnedVertices(std::ifstream& fin, UINT numVertices, std::vector<DX::PosNormalTexTanSkinned>& vertices);
		static void ReadBoneOffsets(std::ifstream& fin, UINT numBones, std::vector<DirectX::XMFLOAT4X4>& boneOffsets);
		static void ReadAnimationClips(std::ifstream& fin, UINT numBones, UINT numAnimationClips, std::map<std::wstring, AnimationClip>& animations);
		static void ReadBoneKeyframes(std::ifstream& fin, UINT numBones, BoneAnimation& boneAnimation);
	};
}



