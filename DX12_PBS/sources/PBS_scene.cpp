#include "PBS_scene.h"

#include <DirectXTex.h>

#include "core/DXSampleHelper.h"
#include "core/DXSample.h"
#include "frame_resource.h"
#include "sample_assets.h"

HRESULT CreateDepthStencilTexture2D(
  ID3D12Device* pDevice,
  UINT width,
  UINT height,
  DXGI_FORMAT typelessFormat,
  DXGI_FORMAT dsvFormat,
  ID3D12Resource** ppResource,
  D3D12_CPU_DESCRIPTOR_HANDLE cpuDsvHandle,  
  D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_DEPTH_WRITE,
  float initDepthValue = 1.0f,
  UINT8 initStencilValue = 0)
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
    ThrowIfFailed(CreateDepthStencilTexture2D(pDevice, width, height, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, &m_depthTexture, dsvCpuHandle));
    NAME_D3D12_OBJECT(m_depthTexture);
  }
}

void PBSScene::Update(double elapsedTime) {
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
  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

  // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
  featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

  if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
  {
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
  }

  // Create a root signature for the equirectangular to cubemap.
  {
    CD3DX12_DESCRIPTOR_RANGE1 ranges[1]{};
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

    CD3DX12_ROOT_PARAMETER1 rootParameters[2]{}; // Performance tip: Order root parameters from most frequently accessed to least frequently accessed.
    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC static_sampler_desc{};
    static_sampler_desc.Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
      0.0f, 0, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
      0.0f, D3D12_FLOAT32_MAX,
      D3D12_SHADER_VISIBILITY_PIXEL, 0);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &static_sampler_desc,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | // Performance tip: Limit root signature access when possible.
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
    ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignatureEquirectangularToCubemap)));
    NAME_D3D12_OBJECT(m_rootSignatureEquirectangularToCubemap);
  }

  // Create a root signature for scene pass
  {
    CD3DX12_DESCRIPTOR_RANGE1 ranges[1]{};
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

    CD3DX12_ROOT_PARAMETER1 rootParameters[2]{}; // Performance tip: Order root parameters from most frequently accessed to least frequently accessed.
    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC static_sampler_desc{};
    static_sampler_desc.Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
      0.0f, 0, D3D12_COMPARISON_FUNC_NEVER, D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
      0.0f, D3D12_FLOAT32_MAX,
      D3D12_SHADER_VISIBILITY_PIXEL, 0);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &static_sampler_desc,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | // Performance tip: Limit root signature access when possible.
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
    ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignatureScenePass)));
    NAME_D3D12_OBJECT(m_rootSignatureScenePass);
  }
}

void PBSScene::CreatePipelineStates(ID3D12Device* pDevice) {
  // Create the equirectangular to cubemap pipeline state.
  {
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    vertexShader = CompileShader(m_pSample->GetAssetFullPath(L"assets/equirectangular_to_cubemap.hlsl").c_str(), nullptr, "VSMain", "vs_5_0");
    pixelShader = CompileShader(m_pSample->GetAssetFullPath(L"assets/equirectangular_to_cubemap.hlsl").c_str(), nullptr, "PSMain", "ps_5_0");

    const D3D12_INPUT_ELEMENT_DESC vertexAttributeDesc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
    inputLayoutDesc.pInputElementDescs = vertexAttributeDesc;
    inputLayoutDesc.NumElements = _countof(vertexAttributeDesc);
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = inputLayoutDesc;
    psoDesc.pRootSignature = m_rootSignatureEquirectangularToCubemap.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    //psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateEquirectangularToCubemap)));
    NAME_D3D12_OBJECT(m_pipelineStateEquirectangularToCubemap);
  }

  // Create the skybox pipeline state for rendering the skybox cubemap derived from equirectangular map.
  {
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    vertexShader = CompileShader(m_pSample->GetAssetFullPath(L"assets/skybox.hlsl").c_str(), nullptr, "VSMain", "vs_5_0");
    pixelShader = CompileShader(m_pSample->GetAssetFullPath(L"assets/skybox.hlsl").c_str(), nullptr, "PSMain", "ps_5_0");

    const D3D12_INPUT_ELEMENT_DESC vertexAttributeDesc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
    inputLayoutDesc.pInputElementDescs = vertexAttributeDesc;
    inputLayoutDesc.NumElements = _countof(vertexAttributeDesc);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = inputLayoutDesc;
    psoDesc.pRootSignature = m_rootSignatureEquirectangularToCubemap.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateSkybox)));
    NAME_D3D12_OBJECT(m_pipelineStateSkybox);
  }

  // Create the pipeline state for generating irradiance map
  {
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    vertexShader = CompileShader(m_pSample->GetAssetFullPath(L"assets/irradiance_convolution.hlsl").c_str(), nullptr, "VSMain", "vs_5_0");
    pixelShader = CompileShader(m_pSample->GetAssetFullPath(L"assets/irradiance_convolution.hlsl").c_str(), nullptr, "PSMain", "ps_5_0");

    const D3D12_INPUT_ELEMENT_DESC vertexAttributeDesc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
    inputLayoutDesc.pInputElementDescs = vertexAttributeDesc;
    inputLayoutDesc.NumElements = _countof(vertexAttributeDesc);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = inputLayoutDesc;
    psoDesc.pRootSignature = m_rootSignatureEquirectangularToCubemap.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    //psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineIrradianceConvolution)));
    NAME_D3D12_OBJECT(m_pipelineIrradianceConvolution);
  }

  // Create the scene pass pipeline.
  {
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    vertexShader = CompileShader(m_pSample->GetAssetFullPath(L"assets/pbr.hlsl").c_str(), nullptr, "VSMain", "vs_5_0");
    pixelShader = CompileShader(m_pSample->GetAssetFullPath(L"assets/pbr.hlsl").c_str(), nullptr, "PSMain", "ps_5_0");

    const D3D12_INPUT_ELEMENT_DESC vertexAttributeDesc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
    inputLayoutDesc.pInputElementDescs = vertexAttributeDesc;
    inputLayoutDesc.NumElements = _countof(vertexAttributeDesc);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = inputLayoutDesc;
    psoDesc.pRootSignature = m_rootSignatureScenePass.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateScenePass)));
    NAME_D3D12_OBJECT(m_pipelineStateScenePass);
  }
}

void PBSScene::CreateFrameResources(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue) {
  for (UINT i = 0; i < m_frameCount; i++) {
    m_frameResources[i] = std::make_unique<FrameResource>(pDevice, pCommandQueue);
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

    D3D12_HEAP_PROPERTIES defaultHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC bufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexDataSize);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &defaultHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &bufferResourceDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&m_vertexBufferCube)));
    NAME_D3D12_OBJECT(m_vertexBufferCube);

    D3D12_HEAP_PROPERTIES uploadHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &uploadHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &bufferResourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&m_vertexBufferCubeUpload)));

    std::unique_ptr<Model::Vertex[]> vertices = cubeModel.GetVertexData();
    // Copy data to the upload heap and then schedule a copy 
        // from the upload heap to the vertex buffer.
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = vertices.get();
    vertexData.RowPitch = vertexDataSize;
    vertexData.SlicePitch = vertexDataSize;

    UpdateSubresources<1>(pCommandList, m_vertexBufferCube.Get(), m_vertexBufferCubeUpload.Get(), 0, 0, 1, &vertexData);

    // Initialize the vertex buffer view.
    m_vertexBufferViewCube.BufferLocation = m_vertexBufferCube->GetGPUVirtualAddress();
    m_vertexBufferViewCube.SizeInBytes = static_cast<UINT>(vertexDataSize);
    m_vertexBufferViewCube.StrideInBytes = static_cast<UINT>(Model::GetVertexStride());
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

    // Describe and create a Texture2D.    
    CD3DX12_RESOURCE_DESC texDesc(
      static_cast<D3D12_RESOURCE_DIMENSION>(metaData.dimension),
      0,
      metaData.width,
      static_cast<UINT>(metaData.height),
      static_cast<UINT16>(metaData.arraySize),
      static_cast<UINT16>(metaData.mipLevels),
      metaData.format,
      1,
      0,
      D3D12_TEXTURE_LAYOUT_UNKNOWN,
      D3D12_RESOURCE_FLAG_NONE);

    D3D12_HEAP_PROPERTIES defaultHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &defaultHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &texDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&m_HDRTexture)));
    NAME_D3D12_OBJECT(m_HDRTexture);

    {
      const UINT subresourceCount = texDesc.DepthOrArraySize * texDesc.MipLevels;
      UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_HDRTexture.Get(), 0, subresourceCount);
      D3D12_HEAP_PROPERTIES uploadHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
      D3D12_RESOURCE_DESC bufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
      ThrowIfFailed(pDevice->CreateCommittedResource(
        &uploadHeapProperty,
        D3D12_HEAP_FLAG_NONE,
        &bufferResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_HDRTextureUpload)));

      // Copy data to the intermediate upload heap and then schedule a copy
      // from the upload heap to the Texture2D.
      D3D12_SUBRESOURCE_DATA textureData = {};
      textureData.pData = image.GetPixels();
      textureData.RowPitch = image.GetImages()->rowPitch;
      textureData.SlicePitch = image.GetImages()->slicePitch;

      UpdateSubresources(pCommandList, m_HDRTexture.Get(), m_HDRTextureUpload.Get(), 0, 0, subresourceCount, &textureData);

      // Performance tip: You can avoid some resource barriers by relying on resource state promotion and decay.
      // Resources accessed on a copy queue will decay back to the COMMON after ExecuteCommandLists()
      // completes on the GPU. Search online for "D3D12 Implicit State Transitions" for more details. 
    }

    // Describe and create an SRV.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = metaData.format;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(metaData.mipLevels);
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    pDevice->CreateShaderResourceView(m_HDRTexture.Get(), &srvDesc, cbvSrvCpuHandle);
    cbvSrvCpuHandle.Offset(m_cbvSrvDescriptorSize);
    cbvSrvGpuHandle.Offset(m_cbvSrvDescriptorSize);


    // *** cubemap ***
    CD3DX12_RESOURCE_DESC cubeMapDesc(
      D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      0,
      kCubeMapWidth,
      kCubeMapHeight,
      kCubeMapArraySize,
      1,
      metaData.format,
      1,
      0,
      D3D12_TEXTURE_LAYOUT_UNKNOWN,
      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    D3D12_CLEAR_VALUE cubeMapClearValue = CD3DX12_CLEAR_VALUE(cubeMapDesc.Format, s_clearColor);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &defaultHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &cubeMapDesc,
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      &cubeMapClearValue,
      IID_PPV_ARGS(&m_cubeMap)));
    NAME_D3D12_OBJECT(m_cubeMap);

    // Describe and create a cubemap SRV.
    D3D12_SHADER_RESOURCE_VIEW_DESC cubeMapSrvDesc = {};
    cubeMapSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    cubeMapSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    cubeMapSrvDesc.Format = cubeMapDesc.Format;
    cubeMapSrvDesc.TextureCube.MipLevels = 1;
    cubeMapSrvDesc.TextureCube.MostDetailedMip = 0;
    cubeMapSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    pDevice->CreateShaderResourceView(m_cubeMap.Get(), &cubeMapSrvDesc, cbvSrvCpuHandle);
    cbvSrvCpuHandle.Offset(m_cbvSrvDescriptorSize);
    cbvSrvGpuHandle.Offset(m_cbvSrvDescriptorSize);

    // Create RTV to each cube face.
    D3D12_RENDER_TARGET_VIEW_DESC cubeMapRTVDesc{};
    cubeMapRTVDesc.Format = cubeMapDesc.Format;
    cubeMapRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    cubeMapRTVDesc.Texture2DArray.MipSlice = 0;
    cubeMapRTVDesc.Texture2DArray.PlaneSlice = 0;
    cubeMapRTVDesc.Texture2DArray.ArraySize = 1;
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvCpuHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameCount, m_rtvDescriptorSize);
    for (UINT16 i = 0; i < kCubeMapArraySize; ++i) {
      cubeMapRTVDesc.Texture2DArray.FirstArraySlice = i;
      pDevice->CreateRenderTargetView(m_cubeMap.Get(), &cubeMapRTVDesc, rtvCpuHandle);
      rtvCpuHandle.Offset(1, m_rtvDescriptorSize);
    }

    // *** irradiance map ***
    CD3DX12_RESOURCE_DESC irradianceMapDesc(
      D3D12_RESOURCE_DIMENSION_TEXTURE2D,
      0,
      kIrradianceMapWidth,
      kIrradianceMapHeight,
      kCubeMapArraySize,
      1,
      metaData.format,
      1,
      0,
      D3D12_TEXTURE_LAYOUT_UNKNOWN,
      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    D3D12_CLEAR_VALUE irradianceMapClearValue = CD3DX12_CLEAR_VALUE(irradianceMapDesc.Format, s_clearColor);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &defaultHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &irradianceMapDesc,
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      &irradianceMapClearValue,
      IID_PPV_ARGS(&m_irradianceMap)));
    NAME_D3D12_OBJECT(m_irradianceMap);

    // Describe and create an irradiance map SRV.
    D3D12_SHADER_RESOURCE_VIEW_DESC irradianceMapSrvDesc = {};
    irradianceMapSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    irradianceMapSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    irradianceMapSrvDesc.Format = irradianceMapDesc.Format;
    irradianceMapSrvDesc.TextureCube.MipLevels = 1;
    irradianceMapSrvDesc.TextureCube.MostDetailedMip = 0;
    irradianceMapSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    pDevice->CreateShaderResourceView(m_irradianceMap.Get(), &irradianceMapSrvDesc, cbvSrvCpuHandle);
    cbvSrvCpuHandle.Offset(m_cbvSrvDescriptorSize);
    cbvSrvGpuHandle.Offset(m_cbvSrvDescriptorSize);

    // Create RTV to each irradiance cube face.
    D3D12_RENDER_TARGET_VIEW_DESC irradianceMapRTVDesc{};
    irradianceMapRTVDesc.Format = irradianceMapDesc.Format;
    irradianceMapRTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    irradianceMapRTVDesc.Texture2DArray.MipSlice = 0;
    irradianceMapRTVDesc.Texture2DArray.PlaneSlice = 0;
    irradianceMapRTVDesc.Texture2DArray.ArraySize = 1;
    for (UINT16 i = 0; i < kCubeMapArraySize; ++i) {
      irradianceMapRTVDesc.Texture2DArray.FirstArraySlice = i;
      pDevice->CreateRenderTargetView(m_irradianceMap.Get(), &irradianceMapRTVDesc, rtvCpuHandle);
      rtvCpuHandle.Offset(1, m_rtvDescriptorSize);
    }
  }

  // Create the sphere vertex and index buffer.
  {
    SphereModel sphereModel(64, 64);
    std::unique_ptr<Model::Vertex[]> vertices = sphereModel.GetVertexData();
    std::unique_ptr<DWORD[]> indices = sphereModel.GetIndexData();

    // *** vertex buffer related ***
    size_t vertexDataSize = sphereModel.GetVertexDataSize();    
    D3D12_HEAP_PROPERTIES defaultHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC bufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexDataSize);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &defaultHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &bufferResourceDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&m_vertexBufferSphere)));
    NAME_D3D12_OBJECT(m_vertexBufferSphere);

    D3D12_HEAP_PROPERTIES uploadHeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &uploadHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &bufferResourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&m_vertexBufferSphereUpload)));
    
    // Copy data to the upload heap and then schedule a copy 
    // from the upload heap to the vertex buffer.
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = vertices.get();
    vertexData.RowPitch = vertexDataSize;
    vertexData.SlicePitch = vertexDataSize;

    UpdateSubresources<1>(pCommandList, m_vertexBufferSphere.Get(), m_vertexBufferSphereUpload.Get(), 0, 0, 1, &vertexData);

    // Initialize the vertex buffer view.
    m_vertexBufferViewSphere.BufferLocation = m_vertexBufferSphere->GetGPUVirtualAddress();
    m_vertexBufferViewSphere.SizeInBytes = static_cast<UINT>(vertexDataSize);
    m_vertexBufferViewSphere.StrideInBytes = static_cast<UINT>(Model::GetVertexStride());

    // *** index buffer related ***
    size_t indexDataSize = sphereModel.GetIndexDataSize();    
    D3D12_RESOURCE_DESC indexBufferResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexDataSize);
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &defaultHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &indexBufferResourceDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&m_indexBufferSphere)));
    NAME_D3D12_OBJECT(m_indexBufferSphere);
    
    ThrowIfFailed(pDevice->CreateCommittedResource(
      &uploadHeapProperty,
      D3D12_HEAP_FLAG_NONE,
      &indexBufferResourceDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&m_indexBufferSphereUpload)));

    // Copy data to the upload heap and then schedule a copy 
    // from the upload heap to the vertex buffer.
    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = indices.get();
    indexData.RowPitch = indexDataSize;
    indexData.SlicePitch = indexDataSize;

    UpdateSubresources<1>(pCommandList, m_indexBufferSphere.Get(), m_indexBufferSphereUpload.Get(), 0, 0, 1, &indexData);

    // Initialize the index buffer view.
    m_indexBufferViewSphere.BufferLocation = m_indexBufferSphere->GetGPUVirtualAddress();
    m_indexBufferViewSphere.SizeInBytes = static_cast<UINT>(indexDataSize);
    m_indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;
  }
}

void PBSScene::UpdateConstantBuffers() {
  const XMMATRIX identityMatrix = XMMatrixIdentity();
  XMStoreFloat4x4(&m_MVPConstantBuffer.model, identityMatrix);

  m_camera.Get3DViewProjMatrices(&m_MVPConstantBuffer.view, &m_MVPConstantBuffer.projection, 60.0f, m_viewport.Width, m_viewport.Height, 0.1f, 10.0f);
}

void PBSScene::CommitConstantBuffers() {
  memcpy(m_pCurrentFrameResource->m_pConstantBufferMVPWO, &m_MVPConstantBuffer, sizeof(m_MVPConstantBuffer));
}

void PBSScene::ScenePass() {
  m_commandList->SetGraphicsRootSignature(m_rootSignatureScenePass.Get());
  m_commandList->SetPipelineState(m_pipelineStateScenePass.Get());

  // Set descriptor heaps.
  ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvHeap.Get() };
  m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

  m_commandList->SetGraphicsRootConstantBufferView(0, m_pCurrentFrameResource->m_constantBufferMVP->GetGPUVirtualAddress());
  CD3DX12_GPU_DESCRIPTOR_HANDLE irradianceMapGpuHandle(m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), 2, m_cbvSrvDescriptorSize);
  m_commandList->SetGraphicsRootDescriptorTable(1, irradianceMapGpuHandle);

  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferViewSphere);
  m_commandList->IASetIndexBuffer(&m_indexBufferViewSphere);
  m_commandList->RSSetViewports(1, &m_viewport);
  m_commandList->RSSetScissorRects(1, &m_scissorRect);
  CD3DX12_CPU_DESCRIPTOR_HANDLE renderTargetCpuHandle(GetCurrentBackBufferRtvCpuHandle());
  m_commandList->OMSetRenderTargets(1, &renderTargetCpuHandle, FALSE, &m_depthDsv);

  UINT indexCount = m_indexBufferViewSphere.SizeInBytes / sizeof(DWORD);
  m_commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
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
