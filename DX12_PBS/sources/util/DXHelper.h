#pragma once

#include "../core/stdafx.h"
#include "../core/DXSample.h"

using Microsoft::WRL::ComPtr;

namespace util {

enum class DescriptorType {
  kConstantBuffer,
  kShaderResourceView,
};

struct DescriptorDesc {
  DescriptorDesc() = default;
  DescriptorDesc(DescriptorType _type, D3D12_SHADER_VISIBILITY _visibility, UINT _numDescriptors, UINT _baseShaderRegister)
    : type(_type), visibility(_visibility), numDescriptors(_numDescriptors), baseShaderRegister(_baseShaderRegister) {

  }

  DescriptorType type = DescriptorType::kConstantBuffer;
  D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
  UINT numDescriptors = 1;
  UINT baseShaderRegister = 0;
};

struct SamplerDesc {
  SamplerDesc() = default;
  SamplerDesc(D3D12_FILTER _filter, D3D12_TEXTURE_ADDRESS_MODE _addressMode, UINT _baseShaderRegister)
    : filter(_filter), addressMode(_addressMode), baseShaderRegister(_baseShaderRegister) {

  }

  D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  D3D12_TEXTURE_ADDRESS_MODE addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  UINT baseShaderRegister = 0;
};

void CreateRootSignature(ID3D12Device* pDevice, const std::vector<DescriptorDesc>& descriptorDescs, const std::vector<SamplerDesc>& samplerDescs,
  ID3D12RootSignature** rootSignature, LPCWSTR name);

void CreatePipelineState(ID3D12Device* pDevice, DXSample* pSample, LPCWSTR shaderFilePath, const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElementDescs,
  ID3D12RootSignature* rootSignaturePtr, const std::vector<DXGI_FORMAT>& rtvFormats,
  bool needDepthTest, D3D12_COMPARISON_FUNC depthFunc,
  ID3D12PipelineState** pipelineState, LPCWSTR name,
  bool frontFaceCounterClockwise = false);

void CreateBufferResourceCore(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  size_t dataSize, ID3D12Resource** buffer, ID3D12Resource** bufferUpload, void* data);

void CreateVertexBufferResource(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  size_t vertexDataSize, ID3D12Resource** vertexBuffer, LPCWSTR name, ID3D12Resource** vertexBufferUpload, void* vertexData,
  D3D12_VERTEX_BUFFER_VIEW& vertexBufferView, UINT vertexStride);

void CreateIndexBufferResource(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  size_t indexDataSize, ID3D12Resource** indexBuffer, LPCWSTR name, ID3D12Resource** indexBufferUpload, void* indexData,
  D3D12_INDEX_BUFFER_VIEW& indexBufferView, DXGI_FORMAT indexFormat);

void CreateTextureResourceCore(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  D3D12_RESOURCE_DIMENSION dimension, size_t width, UINT height, UINT16 depthOrArraySize, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
  ID3D12Resource** texture, D3D12_RESOURCE_STATES initialState,
  bool needUpload, ID3D12Resource** textureUpload, void* textureData, size_t rowPitch, size_t slicePitch,
  bool asSRV, D3D12_SRV_DIMENSION srvViewDimension, D3D12_CPU_DESCRIPTOR_HANDLE srvCPUHandle);

// single 2D texture
void Create2DTextureResource(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  size_t width, UINT height, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
  ID3D12Resource** texture, LPCWSTR name, D3D12_RESOURCE_STATES initialState,
  bool needUpload, ID3D12Resource** textureUpload, void* textureData, size_t rowPitch, size_t slicePitch,
  bool asSRV, const D3D12_CPU_DESCRIPTOR_HANDLE* srvCPUHandle);

// single cubemap texture
void CreateCubeTextureResource(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
  size_t width, UINT height, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
  ID3D12Resource** texture, LPCWSTR name, D3D12_RESOURCE_STATES initialState,
  bool needUpload, ID3D12Resource** textureUpload, void* textureData, size_t rowPitch, size_t slicePitch,
  bool asSRV, const D3D12_CPU_DESCRIPTOR_HANDLE* srvCPUHandle,
  bool asRTV, const D3D12_CPU_DESCRIPTOR_HANDLE* startRtvCPUHandle, UINT rtvDescriptorSize);

HRESULT CreateDepthStencilTexture2D(
  ID3D12Device* pDevice,
  UINT width, UINT height,
  DXGI_FORMAT typelessFormat, DXGI_FORMAT dsvFormat,
  ID3D12Resource** ppResource, D3D12_CPU_DESCRIPTOR_HANDLE cpuDsvHandle,
  D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_DEPTH_WRITE,
  float initDepthValue = 1.0f, UINT8 initStencilValue = 0);

HRESULT CreateConstantBuffer(
  ID3D12Device* pDevice,
  UINT size,
  ID3D12Resource** ppResource,
  D3D12_CPU_DESCRIPTOR_HANDLE* pCpuCbvHandle = nullptr,
  D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_GENERIC_READ);
}  // namespace util