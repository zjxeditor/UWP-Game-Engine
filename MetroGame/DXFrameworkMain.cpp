#include "pch.h"
#include "DXFrameworkMain.h"
#include "Common\DirectXHelper.h"
#include "Common\MathHelper.h"
#include "Common\ShaderChangement.h"

using namespace DXFramework;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

using namespace DX;
using namespace DirectX;

// Loads and initializes application assets when the application is loaded.
DXFrameworkMain::DXFrameworkMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources), m_updating(false), m_firstFlag(false), m_forward(false), m_back(false), m_left(false), m_right(false), m_cameraSpeed(10.0f)
{
#ifdef _DEBUG
	RecordMainThread();
#endif
	// Register to be notified if the Device is lost or recreated
	m_deviceResources->RegisterDeviceNotify(this);

	m_loader = std::make_shared<BasicLoader>(deviceResources->GetD3DDevice(), deviceResources->GetD3DDeviceContext(),
		deviceResources->GetWicImagingFactory());
	m_camera = std::make_shared<Camera>();

	m_shaderMgr = std::make_unique<ShaderMgr>(m_loader);
	m_textureMgr = std::make_unique<TextureMgr>(m_loader);
	m_renderStateMgr = std::make_unique<RenderStateMgr>();
	m_loadScreen = std::make_unique<LoadScreen>();

	m_renderStateMgr->Initialize(m_deviceResources->GetD3DDevice());
	m_loadScreen->Initialize(
		m_deviceResources->GetD2DDevice(),
		m_deviceResources->GetD2DDeviceContext(),
		m_deviceResources->GetWicImagingFactory(),
		loadScreenImage);

	// Initialize camera
	m_camera->SetPosition(0.0f, 2.0f, -20.0f);
	/*XMFLOAT3 pos = { 0.0f, 10.0f, -15.0f };
	XMFLOAT3 at = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
	m_camera->LookAt(pos, at, up);*/

	// TODO: Replace this with your App's content initialization.
	m_fpsTextRenderer = std::make_unique<SampleFpsTextRenderer>(m_deviceResources);
	m_fpsTextRenderer->CreateDeviceDependentResources();
	//m_sceneRenderer = std::make_unique<ObjectsRenderer>(m_deviceResources, m_camera);
	//m_sceneRenderer = std::make_unique<DynamicMapObjectsRenderer>(m_deviceResources, m_camera);
	//m_sceneRenderer = std::make_unique<ShadowObjectsRenderer>(m_deviceResources, m_camera);
	//m_sceneRenderer = std::make_unique<SsaoObjectsRenderer>(m_deviceResources, m_camera);
	//m_sceneRenderer = std::make_unique<TerrainRenderer>(m_deviceResources, m_camera);
	//m_sceneRenderer = std::make_unique<ParticleSystemRenderer>(m_deviceResources, m_camera);
	//m_sceneRenderer = std::make_unique<GPUWavesRenderer>(m_deviceResources, m_camera);
	m_sceneRenderer = std::make_unique<MeshModelRenderer>(m_deviceResources, m_camera);
	m_sceneRenderer->Initialize();
	
	CreateWindowSizeDependentResources();

	m_lastPointPos.X = 0;
	m_lastPointPos.Y = 0;

	// TODO: Change the timer settings if you want something other than the default variable time step mode.
	// e.g. for 60 FPS fixed time step update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

DXFrameworkMain::~DXFrameworkMain()
{
	// Deregister device notification
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Updates application state when the window size changes (e.g. device orientation change)
void DXFrameworkMain::CreateWindowSizeDependentResources() 
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 0.25f*MathHelper::Pi;

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	//if (aspectRatio < 1.0f)
	//{
	//	fovAngleY *= 2.0f;
	//}
	m_camera->SetLens(fovAngleY, aspectRatio, 1.0f, 1000.0f);
	m_camera->SetOrientation(m_deviceResources->GetOrientationTransform3D());

	// TODO: Replace this with the size-dependent initialization of your app's content.
	m_loadScreen->UpdateForWindowSizeChange();
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

// Updates the application state once per frame.
void DXFrameworkMain::Update() 
{
	if (!m_sceneRenderer->GetLoadState())
		return;
	// Update scene objects.
	if (m_updating)
	{
		m_timer.Tick([&]()
		{
			float delta = m_cameraSpeed*(float)m_timer.GetElapsedSeconds();
			if (m_forward)
				m_camera->Walk(delta);
			if (m_back)
				m_camera->Walk(-delta);
			if (m_left)
				m_camera->Strafe(-delta);
			if (m_right)
				m_camera->Strafe(delta);

			m_camera->UpdateViewMatrix();

			// TODO: Replace this with your app's content update functions.
			m_sceneRenderer->Update(m_timer);
			m_fpsTextRenderer->Update(m_timer);
		});
	}
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool DXFrameworkMain::Render()
{
	// Don't try to render anything before the first Update.
	if (/*m_timer.GetFrameCount() == 0 ||*/ !m_updating)
	{
		return false;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// Reset the viewport to target the whole screen.
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	// Reset render targets to the screen.
	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

	// Clear the back buffer and depth stencil view.
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Silver);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Render the load screen
	if (!m_sceneRenderer->GetLoadState())
	{
		if (!m_firstFlag && m_sceneRenderer->GetInitializedState())
		{
			m_firstFlag = true;
			m_sceneRenderer->CreateDeviceDependentResources();
		}
		// Only render the loading screen for now.
		m_deviceResources->GetD3DDeviceContext()->BeginEventInt(L"Render Loading Screen", 0);
		m_loadScreen->Render(m_deviceResources->GetOrientationTransform2D());
		m_deviceResources->GetD3DDeviceContext()->EndEvent();
		return true;
	}

	// Render the scene objects.
	// TODO: Replace this with your app's content rendering functions.
	m_sceneRenderer->Render();
	m_fpsTextRenderer->Render();

	return true;
}

// Notifies renderers that device resources need to be released.
void DXFrameworkMain::OnDeviceLost()
{
	m_updating = false;

	// Reset all content
	m_loadScreen->ReleaseDeviceDependentResources();
	m_sceneRenderer->ReleaseDeviceDependentResources();
	m_fpsTextRenderer->ReleaseDeviceDependentResources();

	m_loader.reset();
	m_shaderMgr.reset();
	m_textureMgr.reset();
	m_renderStateMgr.reset();

	ShaderChangement::Reset();
}

// Notifies renderers that device resources may now be recreated.
void DXFrameworkMain::OnDeviceRestored()
{
	m_updating = true;

	// Create all content
	m_loader = std::make_shared<BasicLoader>(m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext(),
		m_deviceResources->GetWicImagingFactory());
	m_shaderMgr = std::make_unique<ShaderMgr>(m_loader);
	m_textureMgr = std::make_unique<TextureMgr>(m_loader);
	m_renderStateMgr = std::make_unique<RenderStateMgr>();

	m_renderStateMgr->Initialize(m_deviceResources->GetD3DDevice());
	m_loadScreen->Initialize(
		m_deviceResources->GetD2DDevice(),
		m_deviceResources->GetD2DDeviceContext(),
		m_deviceResources->GetWicImagingFactory(),
		loadScreenImage);

	m_sceneRenderer->CreateDeviceDependentResources();
	m_fpsTextRenderer->CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Window control
void DXFrameworkMain::OnWindowActivationChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowActivatedEventArgs^ args)
{
	if (args->WindowActivationState == Windows::UI::Core::CoreWindowActivationState::Deactivated)
	{
		m_updating = false;
	}
	else if (args->WindowActivationState == Windows::UI::Core::CoreWindowActivationState::CodeActivated
		|| args->WindowActivationState == Windows::UI::Core::CoreWindowActivationState::PointerActivated)
	{
		m_updating = true;
	}
}

// UX control
void DXFrameworkMain::OnPointerPressed(Windows::UI::Core::PointerEventArgs^ args)
{
	m_lastPointPos = args->CurrentPoint->Position;
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->SetPointerCapture();
	m_sceneRenderer->OnPointerPressed(args);
}

void DXFrameworkMain::OnPointerReleased(Windows::UI::Core::PointerEventArgs^ args)
{
	Windows::UI::Core::CoreWindow::GetForCurrentThread()->ReleasePointerCapture();
	m_sceneRenderer->OnPointerReleased(args);
}

void DXFrameworkMain::OnPointerMoved(Windows::UI::Core::PointerEventArgs^ args)
{
	if (args->CurrentPoint->Properties->IsLeftButtonPressed)
	{
		float speed = 0.2f;
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(speed*static_cast<float>(args->CurrentPoint->Position.X - m_lastPointPos.X));
		float dy = XMConvertToRadians(speed*static_cast<float>(args->CurrentPoint->Position.Y - m_lastPointPos.Y));

		m_camera->Pitch(dy);
		m_camera->RotateY(dx);
	}

	m_lastPointPos = args->CurrentPoint->Position;
	m_sceneRenderer->OnPointerMoved(args);
}

void DXFrameworkMain::OnKeyDown(Windows::UI::Core::KeyEventArgs^ args)
{
	switch (args->VirtualKey)
	{
	case Windows::System::VirtualKey::W:
		m_forward = true;
		break;
	case Windows::System::VirtualKey::S:
		m_back = true;
		break;
	case Windows::System::VirtualKey::A:
		m_left = true;
		break;
	case Windows::System::VirtualKey::D:
		m_right = true;
		break;
	case Windows::System::VirtualKey::F1:
		WireFrameRender = !WireFrameRender;
		break;
	case Windows::System::VirtualKey::Up:
		m_cameraSpeed += 1.0f;
		if (m_cameraSpeed > 30.0f)
			m_cameraSpeed = 30.0f;
		break;
	case Windows::System::VirtualKey::Down:
		m_cameraSpeed -= 1.0f;
		if (m_cameraSpeed < 1.0f)
			m_cameraSpeed = 1.0f;
		break;
	default:
		break;
	}

	m_sceneRenderer->OnKeyDown(args);
}

void DXFrameworkMain::OnKeyUp(Windows::UI::Core::KeyEventArgs^ args)
{
	switch (args->VirtualKey)
	{
	case Windows::System::VirtualKey::W:
		m_forward = false;
		return;
	case Windows::System::VirtualKey::S:
		m_back = false;
		return;
	case Windows::System::VirtualKey::A:
		m_left = false;
		return;
	case Windows::System::VirtualKey::D:
		m_right = false;
		return;
	default:
		break;
	}

	m_sceneRenderer->OnKeyUp(args);
}