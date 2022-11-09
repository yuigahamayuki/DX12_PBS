#pragma once

#include <memory>

#include "core/DXSample.h"
#include "util/StepTimer.h"

class PBSScene;

class DX12PBSSample : public DXSample {
public:
  DX12PBSSample(UINT width, UINT height, std::wstring name);
  ~DX12PBSSample();

  DX12PBSSample(const DX12PBSSample&) = delete;
  DX12PBSSample& operator=(const DX12PBSSample&) = delete;

  static const UINT FrameCount = 3;

protected:
  void OnInit() override;
  void OnUpdate() override;
  void OnRender() override;
  void OnSizeChanged(UINT width, UINT height, bool minimized) override;
  void OnDestroy() override;
  void OnKeyDown(UINT8 key) override;
  void OnKeyUp(UINT8 key) override;

private:
  void LoadPipeline();
  void LoadAssets();
  void LoadSizeDependentResources();

  void EquirectangularToCubemap();

  void WaitForGpu(ID3D12CommandQueue* pCommandQueue);
  void MoveToNextFrame();

  // D3D objects.
  ComPtr<ID3D12Device> m_device;
  ComPtr<ID3D12CommandQueue> m_commandQueue;
  ComPtr<IDXGISwapChain4> m_swapChain;
  ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
  ComPtr<ID3D12Fence> m_fence;

  // Frame synchronization objects.
  UINT   m_frameIndex = 0;
  HANDLE m_fenceEvent = nullptr;
  UINT64 m_fenceValues[FrameCount]{};

  // Scene rendering resources.
  std::unique_ptr<PBSScene> m_scene;

  StepTimer m_timer;
};