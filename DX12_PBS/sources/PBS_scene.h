#pragma once

#include "core/stdafx.h"
#include "sample_assets.h"
#include "util/Camera.h"

using Microsoft::WRL::ComPtr;

class DXSample;
class FrameResource;

struct InputState {
  bool rightArrowPressed;
  bool leftArrowPressed;
  bool upArrowPressed;
  bool downArrowPressed;
};

class PBSScene {
public:
  PBSScene(UINT frameCount, DXSample* pSample);
  ~PBSScene();

  PBSScene(const PBSScene&) = delete;
  PBSScene& operator=(const PBSScene&) = delete;

  void SetFrameIndex(UINT frameIndex);

  void Initialize(ID3D12Device* pDevice, ID3D12CommandQueue* pDirectCommandQueue, ID3D12GraphicsCommandList* pCommandList, UINT frameIndex);
  void LoadSizeDependentResources(ID3D12Device* pDevice, ComPtr<ID3D12Resource>* ppRenderTargets, UINT width, UINT height);

  void Update(double elapsedTime);
  void KeyDown(UINT8 key);
  void KeyUp(UINT8 key);

  void Render(ID3D12CommandQueue* pCommandQueue);

  void EquirectangularToCubemap(ID3D12CommandQueue* pCommandQueue);

private:
  void InitializeCameraAndLights();

  void CreateDescriptorHeaps(ID3D12Device* pDevice);
  void CreateRootSignatures(ID3D12Device* pDevice);
  void CreatePipelineStates(ID3D12Device* pDevice);
  void CreateFrameResources(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue);
  void CreateCommandLists(ID3D12Device* pDevice);
  void CreateAssetResources(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

  void UpdateConstantBuffers();
  void CommitConstantBuffers();

  void SkyboxPass();

  void BeginFrame();
  void EndFrame();

  UINT GetNumRtvDescriptors() const {
    // 6 faces of cubemap
    return m_frameCount + 6;
  }

  UINT GetNumCbvSrvUavDescriptors() const {
    // 1 hdr texture + 1 cubemap
    return 2;
  }

  inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRtvCpuHandle() const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
  }

  static constexpr float s_clearColor[4] {0.0f, 0.0f, 0.0f, 1.0f};
  static constexpr UINT kCubeMapWidth = 512;
  static constexpr UINT kCubeMapHeight = 512;
  static constexpr UINT16 kCubeMapArraySize = 6;  // a cube has 6 faces

  UINT m_frameCount = 0;

  // Frame Resources
  UINT m_frameIndex = 0;
  std::vector<std::unique_ptr<FrameResource>> m_frameResources;
  FrameResource* m_pCurrentFrameResource = nullptr;
  ModelViewProjectionConstantBuffer m_MVPConstantBuffer;

  // Heap objects.
  ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
  ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap;
  UINT m_rtvDescriptorSize = 0;
  UINT m_cbvSrvDescriptorSize = 0;

  // D3D objects.
  ComPtr<ID3D12RootSignature> m_rootSignatureEquirectangularToCubemap;
  ComPtr<ID3D12PipelineState> m_pipelineStateEquirectangularToCubemap;
  ComPtr<ID3D12PipelineState> m_pipelineStateSkybox;
  ComPtr<ID3D12Resource> m_vertexBufferCube;
  ComPtr<ID3D12Resource> m_vertexBufferCubeUpload;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferViewCube{};
  ComPtr<ID3D12Resource> m_HDRTexture;
  ComPtr<ID3D12Resource> m_HDRTextureUpload;
  ComPtr<ID3D12Resource> m_cubeMap;
  std::vector<ComPtr<ID3D12Resource>> m_renderTargets;
  ComPtr<ID3D12GraphicsCommandList> m_commandList;

  CD3DX12_VIEWPORT m_viewport{};
  CD3DX12_RECT m_scissorRect{};

  DXSample* m_pSample = nullptr;
  Camera m_camera;
  InputState m_keyboardInput;
};