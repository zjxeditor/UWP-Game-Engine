#pragma once

#include "Content\SampleFpsTextRenderer.h"
#include "Content\ObjectsRenderer.h"
#include "Content\DynamicMapObjectsRenderer.h"
#include "Content\ShadowObjectsRenderer.h"
#include "Content\SsaoObjectsRenderer.h"
#include "Content\TerrainRenderer.h"
#include "Content\ParticleSystemRenderer.h"
#include "Content\GPUWavesRenderer.h"
#include "Content\MeshModelRenderer.h"

#include "Common\GameTimer.h"
#include "Common\DeviceResources.h"
#include "Common\BasicLoader.h"
#include "Common\Camera.h"
#include "Common\RenderStateMgr.h"
#include "Common\ShaderMgr.h"
#include "Common\TextureMgr.h"
#include "Common\LoadScreen.h"

// Renders Direct2D and 3D content on the screen.
namespace DXFramework
{
	class DXFrameworkMain : public DX::IDeviceNotify
	{
	public:
		DXFrameworkMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~DXFrameworkMain();
		void CreateWindowSizeDependentResources();
		void Update();
		bool Render();

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

		// Windows control
		void OnWindowActivationChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowActivatedEventArgs^ args);

		// Input control
		void OnPointerPressed(Windows::UI::Core::PointerEventArgs^ args);
		void OnPointerReleased(Windows::UI::Core::PointerEventArgs^ args);
		void OnPointerMoved(Windows::UI::Core::PointerEventArgs^ args);
		void OnKeyDown(Windows::UI::Core::KeyEventArgs^ args);
		void OnKeyUp(Windows::UI::Core::KeyEventArgs^ args);

	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Shared between classes
		std::shared_ptr<DX::BasicLoader> m_loader;
		std::shared_ptr<DX::Camera> m_camera;
		
		std::unique_ptr<DX::ShaderMgr> m_shaderMgr;
		std::unique_ptr<DX::TextureMgr> m_textureMgr;
		std::unique_ptr<DX::RenderStateMgr> m_renderStateMgr;
		 
		// Game time controller
		DX::GameTimer m_timer;

		// TODO: Replace with your own content renderers.
		//std::unique_ptr<ObjectsRenderer> m_sceneRenderer;
		//std::unique_ptr<DynamicMapObjectsRenderer> m_sceneRenderer;
		//std::unique_ptr<ShadowObjectsRenderer> m_sceneRenderer;
		//std::unique_ptr<SsaoObjectsRenderer> m_sceneRenderer;
		//std::unique_ptr<TerrainRenderer> m_sceneRenderer;
		//std::unique_ptr<ParticleSystemRenderer> m_sceneRenderer;
		//std::unique_ptr<GPUWavesRenderer> m_sceneRenderer;
		std::unique_ptr<MeshModelRenderer> m_sceneRenderer;
		std::unique_ptr<SampleFpsTextRenderer> m_fpsTextRenderer;

		std::unique_ptr<DX::LoadScreen> m_loadScreen;

		bool m_updating;
		bool m_firstFlag;

		const std::wstring loadScreenImage = L"Media/Other/cover.jpg";

		// Input control
		Windows::Foundation::Point m_lastPointPos;		
		bool m_forward;		// These four boolean variants are used for caching camera move states
		bool m_back;
		bool m_left;
		bool m_right;
		float m_cameraSpeed;
	};
}