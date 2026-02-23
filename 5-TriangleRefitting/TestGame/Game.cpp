//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_shared<DX::DeviceResources>();
    // TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
    //   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
    //   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
    //   Add DX::DeviceResources::c_ReverseDepth to optimize depth buffer clears for 0 instead of 1.
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    //float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    m_fullscreen.Update(timer);
	m_triangle.Update(timer);

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    //m_fullscreen.Render();
	m_triangle.Render();
    m_deviceResources->Render();

    ID3D12GraphicsCommandList4* m_sceneCommandList = m_deviceResources->GetCurrentFrameResource()->GetCommandList(FrameResource::COMMAND_LIST_SCENE_0).Get();
    ID3D12GraphicsCommandList4* m_postCommandList = m_deviceResources->GetCurrentFrameResource()->GetCommandList(FrameResource::COMMAND_LIST_POST_1).Get();

    ID3D12CommandList* ppCommandLists[] = { m_sceneCommandList, m_postCommandList };
    //ID3D12CommandList* ppCommandLists[] = { m_sceneCommandList };
    m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_deviceResources->Present();

    //// Prepare the command list to render a new frame.
    //m_deviceResources->Prepare();
    //Clear();

    //;

    //auto commandList = m_deviceResources->GetCurrentFrameResource()->GetCommandList(FrameResource::COMMAND_LIST_SCENE_0).Get();
    //PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    //// TODO: Add your rendering code here.

    //PIXEndEvent(commandList);

    //// Show the new frame.
    //PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    //m_deviceResources->Present();

    //// If using the DirectX Tool Kit for DX12, uncomment this line:
    //// m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

    //PIXEndEvent();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    //auto commandList = m_deviceResources->GetCurrentFrameResource()->GetCommandList(FrameResource::COMMAND_LIST_SCENE_0).Get();
    //PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    //// Clear the views.
    //const auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    //const auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    //commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    //commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
    //commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);

    //// Set the viewport and scissor rect.
    //const auto viewport = m_deviceResources->GetScreenViewport();
    //const auto scissorRect = m_deviceResources->GetScissorRect();
    //commandList->RSSetViewports(1, &viewport);
    //commandList->RSSetScissorRects(1, &scissorRect);

    //PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowMoved()
{
    const auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

    // TODO: Game window is being resized.
}

void Game::OnKeyDown(UINT8 key)
{
	switch (key)
	{
    // Instrument the Space Bar to toggle between fullscreen states.
    // The window message loop callback will receive a WM_SIZE message once the
    // window is in the fullscreen state. At that point, the IDXGISwapChain should
    // be resized to match the new window size.
    //
    // NOTE: ALT+Enter will perform a similar operation; the code below is not
    // required to enable that key combination.
    //case VK_SPACE:
    //{
    //    if (m_tearingSupport)
    //    {
    //        Win32Application::ToggleFullscreenWindow();
    //    }
    //    else
    //    {
    //        BOOL fullscreenState;
    //        ThrowIfFailed(m_swapChain->GetFullscreenState(&fullscreenState, nullptr));
    //        if (FAILED(m_swapChain->SetFullscreenState(!fullscreenState, nullptr)))
    //        {
    //            // Transitions to fullscreen mode can fail when running apps over
    //            // terminal services or for some other unexpected reason.  Consider
    //            // notifying the user in some way when this happens.
    //            OutputDebugString(L"Fullscreen transition failed");
    //            assert(false);
    //        }
    //    }
    //    break;
    //}

    // Instrument the Right Arrow key to change the scene rendering resolution 
    // to the next resolution option. 
    case VK_RIGHT:
    {
        m_deviceResources->IncreaseResolutionIndex();
        CreateWindowSizeDependentResources();
    }
    break;

    // Instrument the Left Arrow key to change the scene rendering resolution 
    // to the previous resolution option.
    case VK_LEFT:
    {
		m_deviceResources->DecreaseResolutionIndex();
        CreateWindowSizeDependentResources();
    }
    break;
    
	case VK_ESCAPE:
		ExitGame();
		break;

	default:
		break;
	}
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Check Shader Model 6 support
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_0 };
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))
        || (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0))
    {
#ifdef _DEBUG
        OutputDebugStringA("ERROR: Shader Model 6.0 is not supported!\n");
#endif
        throw std::runtime_error("Shader Model 6.0 is not supported!");
    }

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    // m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    // TODO: Initialize device dependent objects here (independent of window size).
    m_fullscreen.CreateDeviceDependentResources(m_deviceResources);
	m_triangle.CreateDeviceDependentResources(m_deviceResources);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    // TODO: Initialize windows-size dependent objects here.
    m_triangle.CreateWindowSizeDependentResources();
}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    // m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
