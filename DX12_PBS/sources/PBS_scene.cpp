#include "PBS_scene.h"

#include <DirectXTex.h>

#include "core/DXSampleHelper.h"
#include "core/DXSample.h"
#include "frame_resource.h"
#include "sample_assets.h"
#include "util/DXHelper.h"

namespace {

struct SphereInstance {
  float translation[3]{};
  float pbrProperties[3]{};  // r: metallic, g: roughness, b: ao
};

std::unique_ptr<SphereInstance[]> GetSphereInstanceData(UINT& instanceCount) {
  const int nrRows = 7;
  const int nrColumns = 7;
  const float spacing = 2.5f;
  instanceCount = static_cast<UINT>(nrRows * nrColumns);

  std::vector<SphereInstance> instances;

  for (int row = 0; row < nrRows; ++row) {
    float metallic = (float)row / (float)nrRows;
    for (int col = 0; col < nrColumns; ++col) {
      float roughness = (float)col / (float)nrColumns;
      SphereInstance instance;
      instance.translation[0] = (col - (nrColumns / 2)) * spacing;
      instance.translation[1] = (row - (nrRows / 2)) * spacing;
      instance.translation[2] = 0.0f;
      instance.pbrProperties[0] = metallic;
      instance.pbrProperties[1] = roughness;
      instances.emplace_back(instance);
    }
  }

  instanceCount = static_cast<UINT>(instances.size());
  std::unique_ptr<SphereInstance[]> instances_ptr = std::make_unique<SphereInstance[]>(instances.size());
  memcpy(instances_ptr.get(), instances.data(), sizeof(SphereInstance) * instances.size());

  return instances_ptr;
}

}  // namespace



PBSScene::PBSScene(UINT frameCount, DXSample* pSample) :
  m_frameCount(frameCount),
  m_pSample(pSample) {
  m_frameResources.resize(frameCount);
  m_renderTargets.resize(frameCount);

  InitializeCameraAndLights();
}

PBSScene::~PBSScene() {
}

void PBSScene::SetFrameIndex(UINT frameIndex) {
  m_frameIndex = frameIndex;
  m_pCurrentFrameResource = m_frameResources[m_frameIndex].get();
}

void PBSScene::Initialize(ID3D12Device* pDevice, ID3D12CommandQueue* pDirectCommandQueue, ID3D12GraphicsCommandList* pCommandList, UINT frameIndex) {
  CreateDescriptorHeaps(pDevice);
  CreateRootSignatures(pDevice);
  CreatePipelineStates(pDevice);
  CreateFrameResources(pDevice, pDirectCommandQueue);
  CreateCommandLists(pDevice);

  CreateAssetResources(pDevice, pCommandList);

  SetFrameIndex(frameIndex);
}

void PBSScene::LoadSizeDependentResources(ID3D12Device* pDevice, ComPtr<ID3D12Resource>* ppRenderTargets, UINT width, UINT height) {
  m_viewport.Width = static_cast<float>(width);
  m_viewport.Height = static_cast<float>(height);
  m_viewport.MinDepth = 0.0f;
  m_viewport.MaxDepth = 1.0f;

  m_scissorRect.left = 0;
  m_scissorRect.top = 0;
  m_scissorRect.right = static_cast<LONG>(width);
  m_scissorRect.bottom = static_cast<LONG>(height);

  // Create render target views (RTVs).
  {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCpuHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < m_frameCount; i++)
    {
      m_renderTargets[i] = ppRenderTargets[i];
      pDevice->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvCpuHandle);
      rtvCpuHandle.Offset(1, m_rtvDescriptorSize);
      NAME_D3D12_OBJECT_INDEXED(m_renderTargets, i);
    }
  }

  // Create the depth stencil view.
  {
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvCpuHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_depthDsv = dsvCpuHandle;
    ThrowIfFailed(util::CreateDepthStencilTexture2D(pDevice, width, height, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, &m_depthTexture, dsvCpuHandle));
    NAME_D3D12_OBJECT(m_depthTexture);
  }
}

void PBSScene::Update(double elapsedTime) {
  const float moveDistance = 5.0f * static_cast<float>(elapsedTime);
  if (m_keyboardInput.wKeyPressed || m_keyboardInput.sKeyPressed || m_keyboardInput.aKeyPressed || m_keyboardInput.dKeyPressed) {
    m_camera.Move(m_keyboardInput.wKeyPressed, m_keyboardInput.sKeyPressed, m_keyboardInput.aKeyPressed, m_keyboardInput.dKeyPressed, moveDistance);
  }
  
  const float angleChange = 2.0f * static_cast<float>(elapsedTime);
  if (m_keyboardInput.leftArrowPressed)
    m_camera.RotateAroundYAxis(-angleChange);
  if (m_keyboardInput.rightArrowPressed)
    m_camera.RotateAroundYAxis(angleChange);
  if (m_keyboardInput.upArrowPressed)
    m_camera.RotatePitch(-angleChange);
  if (m_keyboardInput.downArrowPressed)
    m_camera.RotatePitch(angleChange);  

  UpdateConstantBuffers();
  CommitConstantBuffers();
}

void PBSScene::KeyDown(UINT8 key) {
  switch (key) {
  case VK_LEFT:
    m_keyboardInput.leftArrowPressed = true;
    break;
  case VK_RIGHT:
    m_keyboardInput.rightArrowPressed = true;
    break;
  case VK_UP:
    m_keyboardInput.upArrowPressed = true;
    break;
  case VK_DOWN:
    m_keyboardInput.downArrowPressed = true;
    break;
  case 'W':
    m_keyboardInput.wKeyPressed = true;
    break;
  case 'S':
    m_keyboardInput.sKeyPressed = true;
    break;
  case 'A':
    m_keyboardInput.aKeyPressed = true;
    break;
  case 'D':
    m_keyboardInput.dKeyPressed = true;
    break;
  default:
    break;
  }
}

void PBSScene::KeyUp(UINT8 key) {
  switch (key) {
  case VK_LEFT:
    m_keyboardInput.leftArrowPressed = false;
    break;
  case VK_RIGHT:
    m_keyboardInput.rightArrowPressed = false;
    break;
  case VK_UP:
    m_keyboardInput.upArrowPressed = false;
    break;
  case VK_DOWN:
    m_keyboardInput.downArrowPressed = false;
    break;
  case 'W':
    m_keyboardInput.wKeyPressed = false;
    break;
  case 'S':
    m_keyboardInput.sKeyPressed = false;
    break;
  case 'A':
    m_keyboardInput.aKeyPressed = false;
    break;
  case 'D':
    m_keyboardInput.dKeyPressed = false;
    break;
  default:
    break;
  }
}

void PBSScene::Render(ID3D12CommandQueue* pCommandQueue) {
  BeginFrame();

  ScenePass();

  SkyboxPass();

  EndFrame();

  ThrowIfFailed(m_commandList->Close());
  ID3D12CommandList* command_lists[] = { m_commandList.Get() };
  pCommandQueue->ExecuteCommandLists(_countof(command_lists), command_lists);
}

void PBSScene::GPUWorkForInitialization(ID3D12CommandQueue* pCommandQueue) {
  EquirectangularToCubemap();
  ConvolveIrradianceMap();

  ThrowIfFailed(m_commandList->Close());
  ID3D12CommandList* command_lists[] = { m_commandList.Get() };
  pCommandQueue->ExecuteCommandLists(_countof(command_lists), command_lists);
}

void PBSScene::EquirectangularToCubemap() {
  m_pCurrentFrameResource->m_commandAllocator->Reset();
  ThrowIfFailed(m_commandList->Reset(m_pCurrentFrameResource->m_commandAllocator.Get(), m_pipelineStateEquirectangularToCubemap.Get()));

  // Set descriptor heaps.
  ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
  m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

  m_commandList->SetGraphicsRootSignature(m_rootSignatureEquirectangularToCubemap.Get());
  m_commandList->SetGraphicsRootDescriptorTable(1, m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart());

  m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferViewCube);
  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  CD3DX12_VIEWPORT viewport{ 0.f, 0.f, static_cast<float>(kCubeMapWidth), static_cast<float>(kCubeMapHeight) };
  CD3DX12_RECT scissorRect{ 0, 0, static_cast<LONG>(kCubeMapWidth), static_cast<LONG>(kCubeMapHeight) };
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  Camera camera;
  XMVECTOR eye = XMVectorSet(0.0f, 0.0, 0.0, 1.0f);
  float cameraTargets[] = {
  1.0f, 0.0f, 0.0f,
  -1.0f, 0.0f, 0.0f,
  0.0f, -1.0f, 0.0f,  // cubemap +Y, but target is (0, -1, 0), because world y->-1, v->0 (equirectangular_to_cubemap.hlsl), samples the upper region of texture
  0.0f, 1.0f, 0.0f,   // same as above
  0.0f, 0.0f, 1.0f,
  0.0f, 0.0f, -1.0f
  };
  float cameraUps[] = {
    0.0f, -1.0f,  0.0f,
    0.0f, -1.0f,  0.0f,
    0.0f,  0.0f,  -1.0f,
    0.0f,  0.0f, 1.0f,
    0.0f, -1.0f,  0.0f,
    0.0f, -1.0f,  0.0f
  };
  std::vector<ViewProjectionConstantBuffer> constantBuffers(6);
  for (UINT16 i = 0; i < kCubeMapArraySize; ++i) {
    XMVECTOR at = XMVectorSet(cameraTargets[3 * i], cameraTargets[3 * i + 1], cameraTargets[3 * i + 2], 0.0f);
    XMVECTOR up = XMVectorSet(cameraUps[3 * i], cameraUps[3 * i + 1], cameraUps[3 * i + 2], 1.0f);
    camera.Set(eye, at, up);
    camera.Get3DViewProjMatrices(&constantBuffers[i].view, &constantBuffers[i].projection,
      90.0f, static_cast<float>(kCubeMapWidth), static_cast<float>(kCubeMapHeight), 0.1f, 10.0f);
  }
  size_t constantBufferSize = sizeof(ViewProjectionConstantBuffer);
  memcpy(m_pCurrentFrameResource->m_pConstantBufferEquirectangularToCubemapWO, constantBuffers.data(), constantBufferSize * 6);
  CD3DX12_CPU_DESCRIPTOR_HANDLE cubeMapRTVHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameCount, m_rtvDescriptorSize);
  for (UINT16 i = 0; i < kCubeMapArraySize; ++i) {
    m_commandList->OMSetRenderTargets(1, &cubeMapRTVHandle, false, nullptr);
    cubeMapRTVHandle.Offset(1, m_rtvDescriptorSize);

    m_commandList->SetGraphicsRootConstantBufferView(0, 
      m_pCurrentFrameResource->m_constantBufferEquirectangularToCubemap->GetGPUVirtualAddress() + i * constantBufferSize);

    m_commandList->DrawInstanced(36, 1, 0, 0);
  }

  D3D12_RESOURCE_BARRIER cubeMapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_cubeMap.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  m_commandList->ResourceBarrier(1, &cubeMapBarrier);

  //ThrowIfFailed(m_commandList->Close());
  //ID3D12CommandList* command_lists[] = { m_commandList.Get() };
  //pCommandQueue->ExecuteCommandLists(_countof(command_lists), command_lists);
}

void PBSScene::ConvolveIrradianceMap() {
  m_commandList->SetPipelineState(m_pipelineIrradianceConvolution.Get());

  CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxGpuHandle(m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
  skyboxGpuHandle.Offset(m_cbvSrvDescriptorSize);
  m_commandList->SetGraphicsRootDescriptorTable(1, skyboxGpuHandle);

  // Some states are the same as equirectangular to cubemap's, so omit api calls such as IASetVertexBuffers
  CD3DX12_VIEWPORT viewport{ 0.f, 0.f, static_cast<float>(kIrradianceMapWidth), static_cast<float>(kIrradianceMapHeight) };
  CD3DX12_RECT scissorRect{ 0, 0, static_cast<LONG>(kIrradianceMapWidth), static_cast<LONG>(kIrradianceMapHeight) };
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  Camera camera;
  XMVECTOR eye = XMVectorSet(0.0f, 0.0, 0.0, 1.0f);
  float cameraTargets[] = {
  1.0f, 0.0f, 0.0f,
  -1.0f, 0.0f, 0.0f,
  0.0f, 1.0f, 0.0f,
  0.0f, -1.0f, 0.0f,
  0.0f, 0.0f, 1.0f,
  0.0f, 0.0f, -1.0f
  };
  float cameraUps[] = {
    0.0f, -1.0f,  0.0f,
    0.0f, -1.0f,  0.0f,
    0.0f,  0.0f,  1.0f,  // TODO: (0, 0, 1) or (0, 0, -1)
    0.0f,  0.0f, -1.0f,  // TODO: (0, 0, -1) or (0, 0, 1)
    0.0f, -1.0f,  0.0f,
    0.0f, -1.0f,  0.0f
  };
  std::vector<ViewProjectionConstantBuffer> constantBuffers(6);
  for (UINT16 i = 0; i < kCubeMapArraySize; ++i) {
    XMVECTOR at = XMVectorSet(cameraTargets[3 * i], cameraTargets[3 * i + 1], cameraTargets[3 * i + 2], 0.0f);
    XMVECTOR up = XMVectorSet(cameraUps[3 * i], cameraUps[3 * i + 1], cameraUps[3 * i + 2], 1.0f);
    camera.Set(eye, at, up);
    camera.Get3DViewProjMatrices(&constantBuffers[i].view, &constantBuffers[i].projection,
      90.0f, static_cast<float>(kCubeMapWidth), static_cast<float>(kCubeMapHeight), 0.1f, 10.0f);
  }
  size_t constantBufferSize = sizeof(ViewProjectionConstantBuffer);
  memcpy(m_pCurrentFrameResource->m_pConstantBufferIrradianceConvolutionWO, constantBuffers.data(), constantBufferSize * 6);
  CD3DX12_CPU_DESCRIPTOR_HANDLE irradianceMapRTVHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameCount + kCubeMapArraySize, m_rtvDescriptorSize);
  for (UINT16 i = 0; i < kCubeMapArraySize; ++i) {
    m_commandList->OMSetRenderTargets(1, &irradianceMapRTVHandle, false, nullptr);
    irradianceMapRTVHandle.Offset(1, m_rtvDescriptorSize);
    
    m_commandList->SetGraphicsRootConstantBufferView(0,
      m_pCurrentFrameResource->m_constantBufferIrradianceConvolution->GetGPUVirtualAddress() + i * constantBufferSize);

    m_commandList->DrawInstanced(36, 1, 0, 0);
  }

  D3D12_RESOURCE_BARRIER irradianceMapBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_irradianceMap.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  m_commandList->ResourceBarrier(1, &irradianceMapBarrier);
}

void PBSScene::InitializeCameraAndLights() {
  XMVECTOR eye = XMVectorSet(0.0f, 0.0f, 3.0f, 1.0f);
  XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
  XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
  m_camera.Set(eye, at, up);

  std::vector<LightState> lightStates;
  lightStates.emplace_back(-10.0f,  10.0f, 10.0f, 300.0f, 300.0f, 300.0f);
  lightStates.emplace_back( 10.0f,  10.0f, 10.0f, 300.0f, 300.0f, 300.0f);
  lightStates.emplace_back(-10.0f, -10.0f, 10.0f, 300.0f, 300.0f, 300.0f);
  lightStates.emplace_back( 10.0f, -10.0f, 10.0f, 300.0f, 300.0f, 300.0f);

  memcpy(&m_lights.lights[0], lightStates.data(), sizeof(LightState) * kNumLights);
}

void PBSScene::CreateDescriptorHeaps(ID3D12Device* pDevice) {
  // Describe and create a render target view (RTV) descriptor heap.
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.NumDescriptors = GetNumRtvDescriptors();
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
  NAME_D3D12_OBJECT(m_rtvHeap);

  // Describe and create a depth stencil view (DSV) descriptor heap.
  D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
  dsvHeapDesc.NumDescriptors = 1;
  dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  ThrowIfFailed(pDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
  NAME_D3D12_OBJECT(m_dsvHeap);

  // Describe and create a shader resource view (SRV) and constant 
  // buffer view (CBV) descriptor heap.  
  D3D12_DESCRIPTOR_HEAP_DESC cbvSrvHeapDesc = {};
  cbvSrvHeapDesc.NumDescriptors = GetNumCbvSrvUavDescriptors();
  cbvSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbvSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ThrowIfFailed(pDevice->CreateDescriptorHeap(&cbvSrvHeapDesc, IID_PPV_ARGS(&m_cbvSrvHeap)));
  NAME_D3D12_OBJECT(m_cbvSrvHeap);

  // Get descriptor sizes for the current device.
  m_rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  m_cbvSrvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void PBSScene::CreateRootSignatures(ID3D12Device* pDevice) {
  std::vector<util::SamplerDesc> samplerDescs;
  samplerDescs.emplace_back(util::SamplerDesc());

  // Create a root signature for the equirectangular to cubemap.
  {
    std::vector<util::DescriptorDesc> descriptorDescs;
    descriptorDescs.emplace_back(util::DescriptorType::kConstantBuffer, D3D12_SHADER_VISIBILITY_VERTEX, 1, 0);
    descriptorDescs.emplace_back(util::DescriptorType::kShaderResourceView, D3D12_SHADER_VISIBILITY_PIXEL, 1, 0);

    util::CreateRootSignature(pDevice, descriptorDescs, samplerDescs, &m_rootSignatureEquirectangularToCubemap, L"m_rootSignatureEquirectangularToCubemap");
  }

  // Create a root signature for scene pass
  {
    std::vector<util::DescriptorDesc> descriptorDescs;
    descriptorDescs.emplace_back(util::DescriptorType::kConstantBuffer, D3D12_SHADER_VISIBILITY_ALL, 1, 0);
    descriptorDescs.emplace_back(util::DescriptorType::kConstantBuffer, D3D12_SHADER_VISIBILITY_PIXEL, 1, 1);
    descriptorDescs.emplace_back(util::DescriptorType::kShaderResourceView, D3D12_SHADER_VISIBILITY_PIXEL, 1, 0);

    util::CreateRootSignature(pDevice, descriptorDescs, samplerDescs, &m_rootSignatureScenePass, L"m_rootSignatureScenePass");
  }
}

void PBSScene::CreatePipelineStates(ID3D12Device* pDevice) {
  const D3D12_INPUT_ELEMENT_DESC standardVertexAttributeDesc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
  };

  std::vector<D3D12_INPUT_ELEMENT_DESC> standardInputElementDescs(_countof(standardVertexAttributeDesc));
  memcpy(standardInputElementDescs.data(), standardVertexAttributeDesc, sizeof(D3D12_INPUT_ELEMENT_DESC) * _countof(standardVertexAttributeDesc));

  const D3D12_INPUT_ELEMENT_DESC instanceVertexAttributeDesc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},

      {"INSTANCEPOS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
      {"INSTANCEPBRPROPERTIES", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1}
  };

  std::vector<D3D12_INPUT_ELEMENT_DESC> instanceInputElementDescs(_countof(instanceVertexAttributeDesc));
  memcpy(instanceInputElementDescs.data(), instanceVertexAttributeDesc, sizeof(D3D12_INPUT_ELEMENT_DESC) * _countof(instanceVertexAttributeDesc));

  std::vector<DXGI_FORMAT> unormRtvFormats(1);
  unormRtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

  std::vector<DXGI_FORMAT> floatRtvFormats(1);
  floatRtvFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
  // Create the equirectangular to cubemap pipeline state.
  {
    util::CreatePipelineState(pDevice, m_pSample, L"assets/equirectangular_to_cubemap.hlsl", standardInputElementDescs,
      m_rootSignatureEquirectangularToCubemap.Get(), floatRtvFormats,
      false, D3D12_COMPARISON_FUNC_LESS,
      &m_pipelineStateEquirectangularToCubemap, L"m_pipelineStateEquirectangularToCubemap");
  }

  // Create the skybox pipeline state for rendering the skybox cubemap derived from equirectangular map.
  {
    util::CreatePipelineState(pDevice, m_pSample, L"assets/skybox.hlsl", standardInputElementDescs,
      m_rootSignatureEquirectangularToCubemap.Get(), unormRtvFormats,
      true, D3D12_COMPARISON_FUNC_LESS_EQUAL,
      &m_pipelineStateSkybox, L"m_pipelineStateSkybox");
  }

  // Create the pipeline state for generating irradiance map
  {
    util::CreatePipelineState(pDevice, m_pSample, L"assets/irradiance_convolution.hlsl", standardInputElementDescs,
      m_rootSignatureEquirectangularToCubemap.Get(), floatRtvFormats,
      false, D3D12_COMPARISON_FUNC_LESS,
      &m_pipelineIrradianceConvolution, L"m_pipelineIrradianceConvolution");
  }

  // Create the scene pass pipeline.
  {
    util::CreatePipelineState(pDevice, m_pSample, L"assets/pbr.hlsl", instanceInputElementDescs,
      m_rootSignatureScenePass.Get(), unormRtvFormats,
      true, D3D12_COMPARISON_FUNC_LESS,
      &m_pipelineStateScenePass, L"m_pipelineStateScenePass");
  }
}

void PBSScene::CreateFrameResources(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue) {
  for (UINT i = 0; i < m_frameCount; i++) {
    m_frameResources[i] = std::make_unique<FrameResource>(pDevice, pCommandQueue);

    memcpy(m_frameResources[i]->m_pConstantBufferLightStatesWO, &m_lights, sizeof(m_lights));
  }
}

void PBSScene::CreateCommandLists(ID3D12Device* pDevice) {
  // Temporarily use a frame resource's command allocator to create command lists.
  ID3D12CommandAllocator* pCommandAllocator = m_frameResources[0]->m_commandAllocator.Get();
  ThrowIfFailed(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator, nullptr, IID_PPV_ARGS(&m_commandList)));
  ThrowIfFailed(m_commandList->Close());
  NAME_D3D12_OBJECT(m_commandList);
}

void PBSScene::CreateAssetResources(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList) {
  // Create the cube vertex buffer
  {
    CubeModel cubeModel;
    size_t vertexDataSize = cubeModel.GetVertexDataSize();
    std::unique_ptr<Model::Vertex[]> vertices = cubeModel.GetVertexData();

    util::CreateVertexBufferResource(pDevice, pCommandList,
      vertexDataSize, &m_vertexBufferCube, L"m_vertexBufferCube", &m_vertexBufferCubeUpload, vertices.get(),
      m_vertexBufferViewCube, static_cast<UINT>(Model::GetVertexStride()));
  }

  // Create HDR texture, cubemap, irradiance map resource.
  {
    // Get a handle to the start of the descriptor heap.
    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvCpuHandle(m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvGpuHandle(m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart());

    // Load HDR image file.
    TexMetadata metaData;
    ScratchImage image;
    DirectX::LoadFromHDRFile(m_pSample->GetAssetFullPath(L"assets/Newport_Loft_Ref.hdr").c_str(), &metaData, image);

    // *** HRD texture ***
    util::Create2DTextureResource(pDevice, pCommandList,
      metaData.width, static_cast<UINT>(metaData.height), static_cast<UINT16>(metaData.mipLevels), metaData.format, D3D12_RESOURCE_FLAG_NONE,
      &m_HDRTexture, L"m_HDRTexture", D3D12_RESOURCE_STATE_COPY_DEST,
      true, &m_HDRTextureUpload, image.GetPixels(), image.GetImages()->rowPitch, image.GetImages()->slicePitch,
      true, &cbvSrvCpuHandle);
    cbvSrvCpuHandle.Offset(m_cbvSrvDescriptorSize);
    cbvSrvGpuHandle.Offset(m_cbvSrvDescriptorSize);

    // *** cubemap(skybox) ***
    CD3DX12_CPU_DESCRIPTOR_HANDLE cubemapStartRtvCpuHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameCount, m_rtvDescriptorSize);
    util::CreateCubeTextureResource(pDevice, pCommandList,
      kCubeMapWidth, kCubeMapHeight, 1, metaData.format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
      &m_cubeMap, L"m_cubeMap", D3D12_RESOURCE_STATE_RENDER_TARGET,
      false, nullptr, nullptr, 0, 0,
      true, &cbvSrvCpuHandle,
      true, &cubemapStartRtvCpuHandle, m_rtvDescriptorSize);
    cbvSrvCpuHandle.Offset(m_cbvSrvDescriptorSize);
    cbvSrvGpuHandle.Offset(m_cbvSrvDescriptorSize);

    // *** irradiance map ***
    CD3DX12_CPU_DESCRIPTOR_HANDLE irradianceMapStartRtvCpuHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameCount + kCubeMapArraySize, m_rtvDescriptorSize);
    util::CreateCubeTextureResource(pDevice, pCommandList,
      kIrradianceMapWidth, kIrradianceMapHeight, 1, metaData.format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
      &m_irradianceMap, L"m_irradianceMap", D3D12_RESOURCE_STATE_RENDER_TARGET,
      false, nullptr, nullptr, 0, 0,
      true, &cbvSrvCpuHandle,
      true, &irradianceMapStartRtvCpuHandle, m_rtvDescriptorSize);
    cbvSrvCpuHandle.Offset(m_cbvSrvDescriptorSize);
    cbvSrvGpuHandle.Offset(m_cbvSrvDescriptorSize);
  }

  // Create the sphere vertex, index, and instance buffer.
  {
    SphereModel sphereModel(64, 64);

    // *** vertex buffer ***
    std::unique_ptr<Model::Vertex[]> vertices = sphereModel.GetVertexData();
    size_t vertexDataSize = sphereModel.GetVertexDataSize();
    util::CreateVertexBufferResource(pDevice, pCommandList,
      vertexDataSize, &m_vertexBufferSphere, L"m_vertexBufferSphere", &m_vertexBufferSphereUpload, vertices.get(),
      m_vertexBufferViewSphere, static_cast<UINT>(Model::GetVertexStride()));

    // *** index buffer ***
    std::unique_ptr<DWORD[]> indices = sphereModel.GetIndexData();
    size_t indexDataSize = sphereModel.GetIndexDataSize();
    util::CreateIndexBufferResource(pDevice, pCommandList,
      indexDataSize, &m_indexBufferSphere, L"m_indexBufferSphere", &m_indexBufferSphereUpload, indices.get(),
      m_indexBufferViewSphere, DXGI_FORMAT_R32_UINT);

    // *** instance buffer ***
    std::unique_ptr<SphereInstance[]> instances = GetSphereInstanceData(m_instanceCountSphere);
    size_t instanceDataSize = sizeof(SphereInstance) * m_instanceCountSphere;
    util::CreateVertexBufferResource(pDevice, pCommandList,
      instanceDataSize, &m_instanceBufferSphere, L"m_instanceBufferSphere", &m_instanceBufferSphereUpload, instances.get(),
      m_instanceBufferViewSphere, static_cast<UINT>(sizeof(SphereInstance)));
  }
}

void PBSScene::UpdateConstantBuffers() {
  const XMMATRIX identityMatrix = XMMatrixIdentity();
  XMStoreFloat4x4(&m_sceneConstantBuffer.model, identityMatrix);

  m_camera.Get3DViewProjMatrices(&m_sceneConstantBuffer.view, &m_sceneConstantBuffer.projection, 60.0f, m_viewport.Width, m_viewport.Height, 0.1f, 100.0f);

  XMStoreFloat4(&m_sceneConstantBuffer.camPos, m_camera.mEye);
}

void PBSScene::CommitConstantBuffers() {
  memcpy(m_pCurrentFrameResource->m_pConstantBufferMVPWO, &m_sceneConstantBuffer, sizeof(m_sceneConstantBuffer));  
}

void PBSScene::ScenePass() {
  m_commandList->SetGraphicsRootSignature(m_rootSignatureScenePass.Get());
  m_commandList->SetPipelineState(m_pipelineStateScenePass.Get());

  // Set descriptor heaps.
  ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
  m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

  m_commandList->SetGraphicsRootConstantBufferView(0, m_pCurrentFrameResource->m_constantBufferMVP->GetGPUVirtualAddress());
  m_commandList->SetGraphicsRootConstantBufferView(1, m_pCurrentFrameResource->m_constantBufferLightStates->GetGPUVirtualAddress());
  CD3DX12_GPU_DESCRIPTOR_HANDLE irradianceMapGpuHandle(m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), 2, m_cbvSrvDescriptorSize);
  m_commandList->SetGraphicsRootDescriptorTable(2, irradianceMapGpuHandle);

  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[2] = { m_vertexBufferViewSphere , m_instanceBufferViewSphere };
  m_commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
  m_commandList->IASetIndexBuffer(&m_indexBufferViewSphere);
  m_commandList->RSSetViewports(1, &m_viewport);
  m_commandList->RSSetScissorRects(1, &m_scissorRect);
  CD3DX12_CPU_DESCRIPTOR_HANDLE renderTargetCpuHandle(GetCurrentBackBufferRtvCpuHandle());
  m_commandList->OMSetRenderTargets(1, &renderTargetCpuHandle, FALSE, &m_depthDsv);

  UINT indexCount = m_indexBufferViewSphere.SizeInBytes / sizeof(DWORD);
  m_commandList->DrawIndexedInstanced(indexCount, m_instanceCountSphere, 0, 0, 0);
}

void PBSScene::SkyboxPass() {
  m_commandList->SetGraphicsRootSignature(m_rootSignatureEquirectangularToCubemap.Get());
  m_commandList->SetPipelineState(m_pipelineStateSkybox.Get());

  // Set descriptor heaps.
  ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
  m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

  m_commandList->SetGraphicsRootConstantBufferView(0, m_pCurrentFrameResource->m_constantBufferMVP->GetGPUVirtualAddress());
  CD3DX12_GPU_DESCRIPTOR_HANDLE skyboxGpuHandle(m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
  skyboxGpuHandle.Offset(m_cbvSrvDescriptorSize);
  m_commandList->SetGraphicsRootDescriptorTable(1, skyboxGpuHandle);

  m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferViewCube);
  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_commandList->RSSetViewports(1, &m_viewport);
  m_commandList->RSSetScissorRects(1, &m_scissorRect);
  CD3DX12_CPU_DESCRIPTOR_HANDLE renderTargetCpuHandle(GetCurrentBackBufferRtvCpuHandle());
  m_commandList->OMSetRenderTargets(1, &renderTargetCpuHandle, FALSE, &m_depthDsv);

  m_commandList->DrawInstanced(36, 1, 0, 0);
}

void PBSScene::BeginFrame() {
  m_pCurrentFrameResource->m_commandAllocator->Reset();
  // Reset the command list.
  ThrowIfFailed(m_commandList->Reset(m_pCurrentFrameResource->m_commandAllocator.Get(), nullptr));
  // Transition back-buffer to a writable state for rendering.
  D3D12_RESOURCE_BARRIER backBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  m_commandList->ResourceBarrier(1, &backBufferBarrier);
  const FLOAT clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
  m_commandList->ClearRenderTargetView(GetCurrentBackBufferRtvCpuHandle(), clearColor, 0, nullptr);
  m_commandList->ClearDepthStencilView(m_depthDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void PBSScene::EndFrame() {
  // Transition back-buffer to a writable state for rendering.
  D3D12_RESOURCE_BARRIER backBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  m_commandList->ResourceBarrier(1, &backBufferBarrier);
}
