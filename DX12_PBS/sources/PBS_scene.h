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
  bool wKeyPressed;
  bool sKeyPressed;
  bool aKeyPressed;
  bool dKeyPressed;
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

  void GPUWorkForInitialization(ID3D12CommandQueue* pCommandQueue);

private:
  void InitializeCameraAndLights();

  void EquirectangularToCubemap();
  void ConvolveIrradianceMap();

  void CreateDescriptorHeaps(ID3D12Device* pDevice);
  void CreateRootSignatures(ID3D12Device* pDevice);
  void CreatePipelineStates(ID3D12Device* pDevice);
  void CreateFrameResources(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue);
  void CreateCommandLists(ID3D12Device* pDevice);
  void CreateAssetResources(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

  void UpdateConstantBuffers();
  void CommitConstantBuffers();

  void ScenePass();
  void SkyboxPass();

  void BeginFrame();
  void EndFrame();

  UINT GetNumRtvDescriptors() const {
    // 1st 6: 6 faces of skybox cubemap
    // 2nd 6: 6 faces of irradiance cubemap
    return m_frameCount + kCubeMapArraySize + kCubeMapArraySize;
  }

  UINT GetNumCbvSrvUavDescriptors() const {
    // 1 hdr texture + 1 skybox cubemap + 1 irradiance map
    return 1 + 1 + 1;
  }

  inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRtvCpuHandle() const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
  }

  static constexpr float s_clearColor[4] {0.0f, 0.0f, 0.0f, 1.0f};
  static constexpr UINT kCubeMapWidth = 512;
  static constexpr UINT kCubeMapHeight = 512;
  static constexpr UINT16 kCubeMapArraySize = 6;  // a cube has 6 faces
  static constexpr UINT kIrradianceMapWidth = 32;
  static constexpr UINT kIrradianceMapHeight = 32;

  UINT m_frameCount = 0;

  // Frame Resources
  UINT m_frameIndex = 0;
  std::vector<std::unique_ptr<FrameResource>> m_frameResources;
  FrameResource* m_pCurrentFrameResource = nullptr;
  ModelViewProjectionConstantBuffer m_MVPConstantBuffer;
  LightStatesConstantBuffer m_lights;

  // Heap objects.
  ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
  ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
  ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap;
  UINT m_rtvDescriptorSize = 0;
  UINT m_cbvSrvDescriptorSize = 0;

  // D3D objects.
  ComPtr<ID3D12RootSignature> m_rootSignatureEquirectangularToCubemap;
  ComPtr<ID3D12PipelineState> m_pipelineStateEquirectangularToCubemap;
  ComPtr<ID3D12PipelineState> m_pipelineStateSkybox;
  ComPtr<ID3D12PipelineState> m_pipelineIrradianceConvolution;
  ComPtr<ID3D12RootSignature> m_rootSignatureScenePass;
  ComPtr<ID3D12PipelineState> m_pipelineStateScenePass;
  ComPtr<ID3D12Resource> m_vertexBufferCube;
  ComPtr<ID3D12Resource> m_vertexBufferCubeUpload;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferViewCube{};
  ComPtr<ID3D12Resource> m_HDRTexture;
  ComPtr<ID3D12Resource> m_HDRTextureUpload;
  ComPtr<ID3D12Resource> m_cubeMap;
  ComPtr<ID3D12Resource> m_irradianceMap;
  ComPtr<ID3D12Resource> m_vertexBufferSphere;
  ComPtr<ID3D12Resource> m_vertexBufferSphereUpload;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferViewSphere{};
  ComPtr<ID3D12Resource> m_indexBufferSphere;
  ComPtr<ID3D12Resource> m_indexBufferSphereUpload;
  D3D12_INDEX_BUFFER_VIEW m_indexBufferViewSphere{};
  ComPtr<ID3D12Resource> m_instanceBufferSphere;
  ComPtr<ID3D12Resource> m_instanceBufferSphereUpload;
  D3D12_VERTEX_BUFFER_VIEW m_instanceBufferViewSphere;
  std::vector<ComPtr<ID3D12Resource>> m_renderTargets;
  ComPtr<ID3D12Resource> m_depthTexture;
  D3D12_CPU_DESCRIPTOR_HANDLE m_depthDsv;
  ComPtr<ID3D12GraphicsCommandList> m_commandList;

  CD3DX12_VIEWPORT m_viewport{};
  CD3DX12_RECT m_scissorRect{};

  DXSample* m_pSample = nullptr;
  Camera m_camera;
  InputState m_keyboardInput;
  UINT m_instanceCountSphere = 0;
};