//***************************************************************************************
// Camera.h by Frank Luna (C) 2011 All Rights Reserved.
//   
// Simple first person style camera class that lets the viewer explore the 3D scene.
//   -It keeps track of the camera coordinate system relative to the world space
//    so that the view matrix can be constructed.  
//   -It keeps track of the viewing frustum of the camera so that the projection
//    matrix can be obtained.
//***************************************************************************************

#pragma once

#include "DirectXHelper.h"

namespace DX
{
	class Camera
	{
	public:
		Camera();
		~Camera();

		// Get/Set world camera position.
		DirectX::XMVECTOR GetPositionXM()const;
		DirectX::XMFLOAT3 GetPosition()const;
		void SetPosition(float x, float y, float z);
		void SetPosition(const DirectX::XMFLOAT3& v);

		// Get camera basis vectors.
		DirectX::XMVECTOR GetRightXM()const;
		DirectX::XMFLOAT3 GetRight()const;
		DirectX::XMVECTOR GetUpXM()const;
		DirectX::XMFLOAT3 GetUp()const;
		DirectX::XMVECTOR GetLookXM()const;
		DirectX::XMFLOAT3 GetLook()const;

		// Get frustum properties.
		float GetNearZ()const;
		float GetFarZ()const;
		float GetAspect()const;
		float GetFovY()const;
		float GetFovX()const;

		// Get near and far plane dimensions in view space coordinates.
		float GetNearWindowWidth()const;
		float GetNearWindowHeight()const;
		float GetFarWindowWidth()const;
		float GetFarWindowHeight()const;

		// Set frustum.
		void SetLens(float fovY, float aspect, float zn, float zf);
		void SetOrientation(const DirectX::XMFLOAT4X4& orientation);

		// Define camera space via LookAt parameters.
		void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
		void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

		// Get View/Proj matrices.
		DirectX::XMMATRIX View()const;
		DirectX::XMMATRIX Proj()const;
		DirectX::XMMATRIX ViewProj()const;

		// Strafe/Walk the camera a distance d.
		void Strafe(float d);
		void Walk(float d);

		// Rotate the camera.
		void Pitch(float angle);
		void RotateY(float angle);

		// After modifying camera position/orientation, call to rebuild the view matrix.
		void UpdateViewMatrix();

	private:

		// Camera coordinate system with coordinates relative to world space.
		DirectX::XMFLOAT3 m_position;
		DirectX::XMFLOAT3 m_right;
		DirectX::XMFLOAT3 m_up;
		DirectX::XMFLOAT3 m_look;

		// Cache frustum properties.
		float m_nearZ;
		float m_farZ;
		float m_aspect;
		float m_fovY;
		float m_nearWindowHeight;
		float m_farWindowHeight;

		// Cache View/Proj matrices.
		DirectX::XMFLOAT4X4 m_view;
		DirectX::XMFLOAT4X4 m_proj;
	};
}
