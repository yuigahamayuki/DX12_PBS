#include "frame_resource.h"

#include "sample_assets.h"
#include "util/DXHelper.h"

FrameResource::FrameResource(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue) {
  ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
  NAME_D3D12_OBJECT(m_commandAllocator);

  // Create constant buffers.
  {
    // A cube has 6 faces.
    constexpr UINT8 kCubeMapArraySize = 6;
    
    ThrowIfFailed(util::CreateConstantBuffer(pDevice, sizeof(ViewProjectionConstantBuffer) * kCubeMapArraySize, &m_constantBufferEquirectangularToCubemap,
      nullptr, D3D12_RESOURCE_STATE_GENERIC_READ));
    NAME_D3D12_OBJECT(m_constantBufferEquirectangularToCubemap);

    ThrowIfFailed(util::CreateConstantBuffer(pDevice, sizeof(ModelViewProjectionConstantBuffer), &m_constantBufferMVP,
      nullptr, D3D12_RESOURCE_STATE_GENERIC_READ));
    NAME_D3D12_OBJECT(m_constantBufferMVP);

    // 6: A cube has 6 faces.
    ThrowIfFailed(util::CreateConstantBuffer(pDevice, sizeof(ViewProjectionConstantBuffer) * kCubeMapArraySize, &m_constantBufferIrradianceConvolution,
      nullptr, D3D12_RESOURCE_STATE_GENERIC_READ));
    NAME_D3D12_OBJECT(m_constantBufferIrradianceConvolution);

    // constant buffer for light states
    ThrowIfFailed(util::CreateConstantBuffer(pDevice, sizeof(LightStatesConstantBuffer), &m_constantBufferLightStates,
      nullptr, D3D12_RESOURCE_STATE_GENERIC_READ));
    NAME_D3D12_OBJECT(m_constantBufferLightStates);

    // Map the constant buffers and cache their heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    const CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_constantBufferEquirectangularToCubemap->Map(0, &readRange, &m_pConstantBufferEquirectangularToCubemapWO));
    ThrowIfFailed(m_constantBufferMVP->Map(0, &readRange, &m_pConstantBufferMVPWO));
    ThrowIfFailed(m_constantBufferIrradianceConvolution->Map(0, &readRange, &m_pConstantBufferIrradianceConvolutionWO));
    ThrowIfFailed(m_constantBufferLightStates->Map(0, &readRange, &m_pConstantBufferLightStatesWO));
  }
}

FrameResource::~FrameResource() {
}
