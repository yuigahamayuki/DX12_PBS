#include "DX12_PBS_sample.h"

#include "PBS_scene.h"

DX12PBSSample::DX12PBSSample(UINT width, UINT height, std::wstring name) :
  DXSample(width, height, name) {
}

DX12PBSSample::~DX12PBSSample() {
}

void DX12PBSSample::OnInit() {
  LoadPipeline();
  LoadAssets();
  LoadSizeDependentResources();

  EquirectangularToCubemap();
}

void DX12PBSSample::OnUpdate() {
  m_timer.Tick();
  m_scene->Update(m_timer.GetElapsedSeconds());
}

void DX12PBSSample::OnRender() {
  m_scene->Render(m_commandQueue.Get());
  ThrowIfFailed(m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING));

  MoveToNextFrame();
}

void DX12PBSSample::OnSizeChanged(UINT width, UINT height, bool minimized) {
}

void DX12PBSSample::OnDestroy() {
  CloseHandle(m_fenceEvent);
}

void DX12PBSSample::OnKeyDown(UINT8 key) {
  m_scene->KeyDown(key);
}

void DX12PBSSample::OnKeyUp(UINT8 key) {
  m_scene->KeyUp(key);
}

void DX12PBSSample::LoadPipeline() {
  UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
  // Enable the debug layer (requires the Graphics Tools "optional feature").
  // NOTE: Enabling the debug layer after device creation will invalidate the active device.
  {
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
      debugController->EnableDebugLayer();

      // Enable additional debug layers.
      dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
  }
#endif

  ComPtr<IDXGIFactory4> factory;
  ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

  ComPtr<IDXGIAdapter1> hardwareAdapter;
  GetHardwareAdapter(factory.Get(), &hardwareAdapter);

  ThrowIfFailed(D3D12CreateDevice(
    hardwareAdapter.Get(),
    D3D_FEATURE_LEVEL_11_0,
    IID_PPV_ARGS(&m_device)
  ));
  NAME_D3D12_OBJECT(m_device);

  // Describe and create the command queue.
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

  ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
  NAME_D3D12_OBJECT(m_commandQueue);

  // Describe and create the swap chain.
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.BufferCount = FrameCount;
  swapChainDesc.Width = m_width;
  swapChainDesc.Height = m_height;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.SampleDesc.Count = 1;

  // It is recommended to always use the tearing flag when it is available.
  swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

  ComPtr<IDXGISwapChain1> swapChain;
  // DXGI does not allow creating a swapchain targeting a window which has fullscreen styles(no border + topmost).
  // Temporarily remove the topmost property for creating the swapchain.
  bool prevIsFullscreen = Win32Application::IsFullscreen();
  if (prevIsFullscreen)
  {
    Win32Application::SetWindowZorderToTopMost(false);
  }
  ThrowIfFailed(factory->CreateSwapChainForHwnd(
    m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
    Win32Application::GetHwnd(),
    &swapChainDesc,
    nullptr,
    nullptr,
    &swapChain
  ));

  if (prevIsFullscreen)
  {
    Win32Application::SetWindowZorderToTopMost(true);
  }

  // With tearing support enabled we will handle ALT+Enter key presses in the
  // window message loop rather than let DXGI handle it by calling SetFullscreenState.
  factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER);

  ThrowIfFailed(swapChain.As(&m_swapChain));
  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

  // Create synchronization objects.
  {
    ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValues[m_frameIndex]++;

    // Create an event handle to use for frame synchronization.
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
      ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
  }
}

void DX12PBSSample::LoadAssets() {
  if (!m_scene) {
    m_scene = std::make_unique<PBSScene>(FrameCount, this);
  }

  // Create a temporary command queue and command list for initializing data on the GPU.
    // Performance tip: Copy command queues are optimized for transfer over PCIe.
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

  ComPtr<ID3D12CommandQueue> copyCommandQueue;
  ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&copyCommandQueue)));
  NAME_D3D12_OBJECT(copyCommandQueue);

  ComPtr<ID3D12CommandAllocator> commandAllocator;
  ThrowIfFailed(m_device->CreateCommandAllocator(queueDesc.Type, IID_PPV_ARGS(&commandAllocator)));
  NAME_D3D12_OBJECT(commandAllocator);

  ComPtr<ID3D12GraphicsCommandList> commandList;
  ThrowIfFailed(m_device->CreateCommandList(0, queueDesc.Type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
  NAME_D3D12_OBJECT(commandList);

  m_scene->Initialize(m_device.Get(), m_commandQueue.Get(), commandList.Get(), m_frameIndex);

  ThrowIfFailed(commandList->Close());

  ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
  copyCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

  // Wait until assets have been uploaded to the GPU.
  WaitForGpu(copyCommandQueue.Get());
}

void DX12PBSSample::LoadSizeDependentResources() {
  for (UINT i = 0; i < FrameCount; i++)
  {
    ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
  }

  m_scene->LoadSizeDependentResources(m_device.Get(), m_renderTargets, m_width, m_height);
}

void DX12PBSSample::EquirectangularToCubemap() {
  m_scene->EquirectangularToCubemap(m_commandQueue.Get());

  WaitForGpu(m_commandQueue.Get());
}

void DX12PBSSample::WaitForGpu(ID3D12CommandQueue* pCommandQueue) {
  // Schedule a Signal command in the queue.
  ThrowIfFailed(pCommandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

  // Wait until the fence has been processed.
  ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
  WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

  // Increment the fence value for the current frame.
  m_fenceValues[m_frameIndex]++;
}

void DX12PBSSample::MoveToNextFrame() {
  // Schedule a Signal command in the queue.
  const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
  ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

  // Update the frame index.
  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

  // If the next frame is not ready to be rendered yet, wait until it is ready.
  if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
  {
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
  }
  m_scene->SetFrameIndex(m_frameIndex);

  // Set the fence value for the next frame.
  m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}
