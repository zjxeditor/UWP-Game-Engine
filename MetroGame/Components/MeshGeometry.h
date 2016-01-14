#pragma once

#include <DirectXMath.h>
#include <ppltasks.h>
#include "Common/ShaderMgr.h"
#include "Common/LightHelper.h"

enum class EffectType
{
	NoTexture,
	Texture,
	Normal
};

namespace DXFramework
{
	struct X3dMaterial
	{
		DX::Material Mat;
		EffectType Effect;
		std::wstring DiffuseMap;
		std::wstring NormalMap;
	};

	struct Subset
	{
		UINT MtlIndex;
		UINT VertexBase;
		UINT IndexStart;
		UINT IndexCount;
	};

	struct Keyframe
	{
		Keyframe();
		~Keyframe();

		float TimePos;
		DirectX::XMFLOAT3 Translation;
		DirectX::XMFLOAT3 Scale;
		DirectX::XMFLOAT4 RotationQuat;
	};

	struct BoneAnimation
	{
		float GetStartTime()const;
		float GetEndTime()const;

		void Interpolate(float t, DirectX::XMFLOAT4X4& M)const;

		std::vector<Keyframe> Keyframes;
	};

	struct AnimationClip
	{
		float GetClipStartTime()const;
		float GetClipEndTime()const;

		void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms)const;

		std::vector<BoneAnimation> BoneAnimations;
	};

	class SkinnedData
	{
	public:
		UINT GetBoneCount()const;
		float GetClipStartTime(const std::wstring& clipName)const;
		float GetClipEndTime(const std::wstring& clipName)const;

		void Initialize(
			std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
			std::map<std::wstring, AnimationClip>& animations);

		// In a real project, you'd want to cache the result if there was a chance
		// that you were calling this several times with the same clipName at 
		// the same timePos.
		void GetFinalTransforms(const std::wstring& clipName, float timePos,
			std::vector<DirectX::XMFLOAT4X4>& finalTransforms)const;

	private:
		std::vector<DirectX::XMFLOAT4X4> m_boneOffsets;
		std::map<std::wstring, AnimationClip> m_animations;
	};
}


