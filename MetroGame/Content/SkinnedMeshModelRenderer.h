#pragma once

#include "Common\GameTimer.h"
#include "Common\DeviceResources.h"
#include "Common\Camera.h"
#include "Common\RenderStateMgr.h"
#include "Common\ShaderMgr.h"
#include "Common\TextureMgr.h"
#include "Common\ConstantBuffer.h"
#include "Common\LightHelper.h"
#include "Components\MeshObject.h"
#include "Components\X3DLoader.h"
#include "Components\Sky.h"
#include "Components\ShadowHelper.h"
#include "Components\SsaoHelper.h"
#include "Components\BasicObject.h"

#include <DirectXCollision.h>

namespace DXFramework
{
	// This sample renderer instantiates a basic rendering pipeline.
	class SkinnedMeshModelRenderer
	{
	public:
		SkinnedMeshModelRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, const std::shared_ptr<DX::Camera>& camera);

		void Initialize();
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::GameTimer const& timer);
		void Render();

		void OnPointerPressed(Windows::UI::Core::PointerEventArgs^ args);
		void OnPointerReleased(Windows::UI::Core::PointerEventArgs^ args);
		void OnPointerMoved(Windows::UI::Core::PointerEventArgs^ args);
		void OnKeyDown(Windows::UI::Core::KeyEventArgs^ args);
		void OnKeyUp(Windows::UI::Core::KeyEventArgs^ args);

	public:
		bool GetLoadState() { return m_loadingComplete; }
		bool GetInitializedState() { return m_initialized; }

	private:
		void InitMesh();

	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::shared_ptr<DX::Camera> m_camera;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerFrameCB>> m_perFrameCB;
		std::shared_ptr<DX::ConstantBuffer<DX::BasicPerObjectCB>> m_perObjectCB;
		
		// Custom data
		std::unique_ptr<MeshObject> m_mesh;
		DX::DirectionalLight m_dirLights[3];
		DirectX::XMFLOAT3 m_originalLightDir[3];
		float m_lightRotationAngle;

		// Variables used with the rendering loop.
		bool	m_initialized;
		bool	m_loadingComplete;
	};
}

