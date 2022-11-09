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

public:
  FrameResource(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue);
  ~FrameResource();

  FrameResource(const FrameResource&) = delete;
  FrameResource& operator=(const FrameResource&) = delete;

};

inline HRESULT CreateConstantBuffer(
  ID3D12Device* pDevice,
  UINT size,
  ID3D12Resource** ppResource,
  D3D12_CPU_DESCRIPTOR_HANDLE* pCpuCbvHandle = nullptr,
  D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_GENERIC_READ)
{
  try
  {
    *ppResource = nullptr;
    
    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const UINT alignedSize = CalculateConstantBufferByteSize(size);
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &heapProperty,
      D3D12_HEAP_FLAG_NONE,
      &resourceDesc,
      initState,
      nullptr,
      IID_PPV_ARGS(ppResource)));

    if (pCpuCbvHandle)
    {
      // Describe and create the shadow constant buffer view (CBV).
      D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
      cbvDesc.SizeInBytes = alignedSize;
      cbvDesc.BufferLocation = (*ppResource)->GetGPUVirtualAddress();
      pDevice->CreateConstantBufferView(&cbvDesc, *pCpuCbvHandle);
    }
  }
  catch (HrException& e)
  {
    SAFE_RELEASE(*ppResource);
    return e.Error();
  }
  return S_OK;
}