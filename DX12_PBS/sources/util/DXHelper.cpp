#include "DXHelper.h"

#include "../core/DXSampleHelper.h"
#include "../core/d3dx12.h"

namespace util {

void CreateRootSignature(ID3D12Device* pDevice, const std::vector<DescriptorDesc>& descriptorDescs, const std::vector<SamplerDesc>& samplerDescs,
  ID3D12RootSignature** rootSignature, LPCWSTR name) {
  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

  // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

  if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
  {
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
  }

  std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
  std::vector<CD3DX12_DESCRIPTOR_RANGE1> ranges;
  bool denyVertexAccess = true;
  bool denyPixelAccess = true;
  for (const auto& descriptorDesc : descriptorDescs) {
    if (descriptorDesc.visibility == D3D12_SHADER_VISIBILITY_ALL) {
      denyVertexAccess = false;
      denyPixelAccess = false;
    }
    if (descriptorDesc.visibility == D3D12_SHADER_VISIBILITY_VERTEX) {
      denyVertexAccess = false;
    }
    if (descriptorDesc.visibility == D3D12_SHADER_VISIBILITY_PIXEL) {
      denyPixelAccess = false;
    }
    CD3DX12_ROOT_PARAMETER1 parameter;
    switch (descriptorDesc.type) {
    case DescriptorType::kConstantBuffer:
      parameter.InitAsConstantBufferView(descriptorDesc.baseShaderRegister, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, descriptorDesc.visibility);
      break;
    case DescriptorType::kShaderResourceView:
      CD3DX12_DESCRIPTOR_RANGE1 range{};
      range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, descriptorDesc.numDescriptors, descriptorDesc.baseShaderRegister, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
      parameter.InitAsDescriptorTable(1, &range, descriptorDesc.visibility);
      break;
    default:
      break;
    }

    rootParameters.emplace_back(parameter);
  }

  std::vector<CD3DX12_STATIC_SAMPLER_DESC> samplers;
  for (const auto& samplerDesc : samplerDescs) {
    CD3DX12_STATIC_SAMPLER_DESC sampler{};
    sampler.Init(samplerDesc.baseShaderRegister, samplerDesc.filter,
      samplerDesc.addressMode, samplerDesc.addressMode, samplerDesc.addressMode,
      0.0f, 0, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
      0.0f, D3D12_FLOAT32_MAX,
      D3D12_SHADER_VISIBILITY_PIXEL, 0);

    samplers.emplace_back(sampler);
  }

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
  // Performance tip: Limit root signature access when possible.
  D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
  if (denyVertexAccess) {
    flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
  }
  if (denyPixelAccess) {
    flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
  }
  rootSignatureDesc.Init_1_1(static_cast<UINT>(rootParameters.size()), rootParameters.data(), static_cast<UINT>(samplers.size()), samplers.data(), flags);

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
  ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature)));
  SetName(*rootSignature, name);
}

void CreatePipelineState(ID3D12Device* pDevice, DXSample* pSample, LPCWSTR shaderFilePath, const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElementDescs,
  ID3D12RootSignature* rootSignaturePtr, const std::vector<DXGI_FORMAT>& rtvFormats, 
  bool needDepthTest, D3D12_COMPARISON_FUNC depthFunc,
  ID3D12PipelineState** pipelineState, LPCWSTR name,
  bool frontFaceCounterClockwise) {
  ComPtr<ID3DBlob> vertexShader;
  ComPtr<ID3DBlob> pixelShader;
  vertexShader = CompileShader(pSample->GetAssetFullPath(shaderFilePath).c_str(), nullptr, "VSMain", "vs_5_0");
  pixelShader = CompileShader(pSample->GetAssetFullPath(shaderFilePath).c_str(), nullptr, "PSMain", "ps_5_0");

  D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
  inputLayoutDesc.pInputElementDescs = inputElementDescs.data();
  inputLayoutDesc.NumElements = static_cast<UINT>(inputElementDescs.size());

  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = inputLayoutDesc;
  psoDesc.pRootSignature = rootSignaturePtr;
  psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
  psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  if (frontFaceCounterClockwise) {
    psoDesc.RasterizerState.FrontCounterClockwise = true;
  }
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = static_cast<UINT>(rtvFormats.size());
  for (UINT i = 0; i < psoDesc.NumRenderTargets; ++i) {
    psoDesc.RTVFormats[i] = rtvFormats[i];
  }
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  psoDesc.SampleDesc.Count = 1;
  if (needDepthTest) {
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthFunc = depthFunc;
  }

  ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState)));
  SetName(*pipelineState, name);
}

void CreateBufferResourceCore(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  size_t dataSize, ID3D12Resource** buffer, ID3D12Resource** bufferUpload, void* data) {
  D3D12_HEAP_PROPERTIES defaultHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC bufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
  ThrowIfFailed(pDevice->CreateCommittedResource(
    &defaultHeapProperty,
    D3D12_HEAP_FLAG_NONE,
    &bufferResourceDesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(buffer)));  

  D3D12_HEAP_PROPERTIES uploadHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  ThrowIfFailed(pDevice->CreateCommittedResource(
    &uploadHeapProperty,
    D3D12_HEAP_FLAG_NONE,
    &bufferResourceDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(bufferUpload)));

  // Copy data to the upload heap and then schedule a copy 
  // from the upload heap to the vertex buffer.
  D3D12_SUBRESOURCE_DATA subresourceData = {};
  subresourceData.pData = data;
  subresourceData.RowPitch = dataSize;
  subresourceData.SlicePitch = dataSize;

  UpdateSubresources<1>(pCommandList, *buffer, *bufferUpload, 0, 0, 1, &subresourceData);
}

void CreateVertexBufferResource(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  size_t vertexDataSize, ID3D12Resource** vertexBuffer, LPCWSTR name, ID3D12Resource** vertexBufferUpload, void* vertexData, 
  D3D12_VERTEX_BUFFER_VIEW& vertexBufferView, UINT vertexStride) {
  CreateBufferResourceCore(pDevice, pCommandList,
    vertexDataSize, vertexBuffer, vertexBufferUpload, vertexData);

  SetName(*vertexBuffer, name);

  // Initialize the vertex buffer view.
  vertexBufferView.BufferLocation = (*vertexBuffer)->GetGPUVirtualAddress();
  vertexBufferView.SizeInBytes = static_cast<UINT>(vertexDataSize);
  vertexBufferView.StrideInBytes = vertexStride;
}

void CreateIndexBufferResource(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, 
  size_t indexDataSize, ID3D12Resource** indexBuffer, LPCWSTR name, ID3D12Resource** indexBufferUpload, void* indexData,
  D3D12_INDEX_BUFFER_VIEW& indexBufferView, DXGI_FORMAT indexFormat) {
  CreateBufferResourceCore(pDevice, pCommandList,
    indexDataSize, indexBuffer, indexBufferUpload, indexData);

  SetName(*indexBuffer, name);

  // Initialize the index buffer view.
  indexBufferView.BufferLocation = (*indexBuffer)->GetGPUVirtualAddress();
  indexBufferView.SizeInBytes = static_cast<UINT>(indexDataSize);
  indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}

void CreateTextureResourceCore(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  D3D12_RESOURCE_DIMENSION dimension, size_t width, UINT height, UINT16 depthOrArraySize, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
  ID3D12Resource** texture, D3D12_RESOURCE_STATES initialState,
  bool needUpload, ID3D12Resource** textureUpload, void* textureData, size_t rowPitch, size_t slicePitch,
  bool asSRV, D3D12_SRV_DIMENSION srvViewDimension, D3D12_CPU_DESCRIPTOR_HANDLE srvCPUHandle) {
  CD3DX12_RESOURCE_DESC texDescOrigin(
    dimension,
    0,
    width,
    height,
    depthOrArraySize,
    mipLevels,
    format,
    1,
    0,
    D3D12_TEXTURE_LAYOUT_UNKNOWN,
    flags);

  D3D12_HEAP_PROPERTIES defaultHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  ThrowIfFailed(pDevice->CreateCommittedResource(
    &defaultHeapProperty,
    D3D12_HEAP_FLAG_NONE,
    &texDescOrigin,
    initialState,
    nullptr,
    IID_PPV_ARGS(texture)));

  auto texDesc = (*texture)->GetDesc();

  if (needUpload) {
    const UINT subresourceCount = texDesc.DepthOrArraySize * texDesc.MipLevels;
    UINT64 uploadBufferSize = GetRequiredIntermediateSize(*texture, 0, subresourceCount);
    D3D12_HEAP_PROPERTIES uploadHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC bufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &uploadHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &bufferResourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(textureUpload)));

    // Copy data to the intermediate upload heap and then schedule a copy
    // from the upload heap to the Texture2D.
    D3D12_SUBRESOURCE_DATA textureSubresourceData = {};
    textureSubresourceData.pData = textureData;
    textureSubresourceData.RowPitch = rowPitch;
    textureSubresourceData.SlicePitch = slicePitch;

    UpdateSubresources(pCommandList, *texture, *textureUpload, 0, 0, subresourceCount, &textureSubresourceData);

    // Performance tip: You can avoid some resource barriers by relying on resource state promotion and decay.
    // Resources accessed on a copy queue will decay back to the COMMON after ExecuteCommandLists()
    // completes on the GPU. Search online for "D3D12 Implicit State Transitions" for more details. 
  }

  if (asSRV) {
    // Describe and create an SRV.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = srvViewDimension;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = format;
    switch (srvViewDimension) {
    case D3D12_SRV_DIMENSION_TEXTURE2D:
      srvDesc.Texture2D.MipLevels = static_cast<UINT>(mipLevels);
      srvDesc.Texture2D.MostDetailedMip = 0;
      srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
      break;
    case D3D12_SRV_DIMENSION_TEXTURECUBE:
      srvDesc.TextureCube.MipLevels = static_cast<UINT>(mipLevels);
      srvDesc.TextureCube.MostDetailedMip = 0;
      srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    default:
      break;
    }
    
    pDevice->CreateShaderResourceView(*texture, &srvDesc, srvCPUHandle);
  }
}

void Create2DTextureResource(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  size_t width, UINT height, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
  ID3D12Resource** texture, LPCWSTR name, D3D12_RESOURCE_STATES initialState,
  bool needUpload, ID3D12Resource** textureUpload, void* textureData, size_t rowPitch, size_t slicePitch,
  bool asSRV, const D3D12_CPU_DESCRIPTOR_HANDLE* srvCPUHandle) {
  CreateTextureResourceCore(pDevice, pCommandList,
    D3D12_RESOURCE_DIMENSION_TEXTURE2D, width, height, 1, mipLevels, format, flags,
    texture, initialState,
    needUpload, textureUpload, textureData, rowPitch, slicePitch,
    asSRV, D3D12_SRV_DIMENSION_TEXTURE2D, *srvCPUHandle);

  SetName(*texture, name);
}

void CreateCubeTextureResource(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  size_t width, UINT height, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
  ID3D12Resource** texture, LPCWSTR name, D3D12_RESOURCE_STATES initialState,
  bool needUpload, ID3D12Resource** textureUpload, void* textureData, size_t rowPitch, size_t slicePitch,
  bool asSRV, const D3D12_CPU_DESCRIPTOR_HANDLE* srvCPUHandle,
  bool asRTV, const D3D12_CPU_DESCRIPTOR_HANDLE* startRtvCPUHandle, UINT rtvDescriptorSize) {
  constexpr UINT16 kCubeMapArraySize = 6;
  CreateTextureResourceCore(pDevice, pCommandList,
    D3D12_RESOURCE_DIMENSION_TEXTURE2D, width, height, kCubeMapArraySize, mipLevels, format, flags,
    texture, initialState,
    needUpload, textureUpload, textureData, rowPitch, slicePitch,
    asSRV, D3D12_SRV_DIMENSION_TEXTURECUBE, *srvCPUHandle);

  if (asRTV) {
    // Create RTV to each cube face.
    D3D12_RENDER_TARGET_VIEW_DESC cubeMapRTVDesc{};
    cubeMapRTVDesc.Format = format;
    cubeMapRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    cubeMapRTVDesc.Texture2DArray.MipSlice = 0;
    cubeMapRTVDesc.Texture2DArray.PlaneSlice = 0;
    cubeMapRTVDesc.Texture2DArray.ArraySize = 1;
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCPUHandle(*startRtvCPUHandle);
    for (UINT16 i = 0; i < kCubeMapArraySize; ++i) {
      cubeMapRTVDesc.Texture2DArray.FirstArraySlice = i;
      pDevice->CreateRenderTargetView(*texture, &cubeMapRTVDesc, rtvCPUHandle);
      rtvCPUHandle.Offset(1, rtvDescriptorSize);
    }
  }

  SetName(*texture, name);
}

HRESULT CreateDepthStencilTexture2D(
  ID3D12Device* pDevice,
  UINT width, UINT height,
  DXGI_FORMAT typelessFormat, DXGI_FORMAT dsvFormat,
  ID3D12Resource** ppResource, D3D12_CPU_DESCRIPTOR_HANDLE cpuDsvHandle,
  D3D12_RESOURCE_STATES initState,
  float initDepthValue, UINT8 initStencilValue)
{
  try
  {
    *ppResource = nullptr;

    CD3DX12_RESOURCE_DESC texDesc(
      D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      0,
      width,
      height,
      1,
      1,
      typelessFormat,
      1,
      0,
      D3D12_TEXTURE_LAYOUT_UNKNOWN,
      D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    CD3DX12_HEAP_PROPERTIES defaultHeapProperty(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_CLEAR_VALUE clearValue(dsvFormat, initDepthValue, initStencilValue);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &defaultHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &texDesc,
      initState,
      &clearValue, // Performance tip: Tell the runtime at resource creation the desired clear value.
      IID_PPV_ARGS(ppResource)));

    // Create a depth stencil view (DSV).
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = dsvFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    pDevice->CreateDepthStencilView(*ppResource, &dsvDesc, cpuDsvHandle);
  }
  catch (HrException& e)
  {
    SAFE_RELEASE(*ppResource);
    return e.Error();
  }
  return S_OK;
}

HRESULT CreateConstantBuffer(
  ID3D12Device* pDevice,
  UINT size,
  ID3D12Resource** ppResource,
  D3D12_CPU_DESCRIPTOR_HANDLE* pCpuCbvHandle,
  D3D12_RESOURCE_STATES initState)
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

}  // namespace util
