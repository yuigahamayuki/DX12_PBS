#include "frame_resource.h"

#include "sample_assets.h"

FrameResource::FrameResource(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue) {
  ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
  NAME_D3D12_OBJECT(m_commandAllocator);

  // Create constant buffers.
  {
    // 6: A cube has 6 faces.
    ThrowIfFailed(CreateConstantBuffer(pDevice, sizeof(EquirectangularToCubemapConstantBuffer) * 6, &m_constantBufferEquirectangularToCubemap,
      nullptr, D3D12_RESOURCE_STATE_GENERIC_READ));
    NAME_D3D12_OBJECT(m_constantBufferEquirectangularToCubemap);

    // Map the constant buffers and cache their heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    const CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_constantBufferEquirectangularToCubemap->Map(0, &readRange, &m_pConstantBufferEquirectangularToCubemapWO));
  }
}

FrameResource::~FrameResource() {
}
