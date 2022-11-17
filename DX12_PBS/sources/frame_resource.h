#pragma once

#include "core/DXSampleHelper.h"

using namespace Microsoft::WRL;

class FrameResource {
public:
  ComPtr<ID3D12CommandAllocator> m_commandAllocator;

  ComPtr<ID3D12Resource> m_constantBufferEquirectangularToCubemap;
  void* m_pConstantBufferEquirectangularToCubemapWO = nullptr;

  ComPtr<ID3D12Resource> m_constantBufferMVP;
  void* m_pConstantBufferMVPWO = nullptr;

  ComPtr<ID3D12Resource> m_constantBufferIrradianceConvolution;
  void* m_pConstantBufferIrradianceConvolutionWO = nullptr;

  ComPtr<ID3D12Resource> m_constantBufferPrefilter;
  void* m_pConstantBufferPrefilterWO = nullptr;

  ComPtr<ID3D12Resource> m_constantBufferLightStates;
  void* m_pConstantBufferLightStatesWO = nullptr;

public:
  FrameResource(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue);
  ~FrameResource();

  FrameResource(const FrameResource&) = delete;
  FrameResource& operator=(const FrameResource&) = delete;

};
