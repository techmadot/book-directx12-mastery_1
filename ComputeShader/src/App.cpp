#include "App.h"

#include "GfxDevice.h"
#include "FileLoader.h"
#include "Win32Application.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "imgui/backends/imgui_impl_win32.h"

#include "TextureUtility.h"

using namespace Microsoft::WRL;
using namespace DirectX;

// DirectX12 Agility SDK を使う.
extern "C"
{
  __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
  __declspec(dllexport) extern const char8_t* D3D12SDKPath = u8".\\D3D12\\";
}

static std::unique_ptr<MyApplication> gMyApplication;
std::unique_ptr<MyApplication>& GetApplication()
{
  if (gMyApplication == nullptr)
  {
    gMyApplication = std::make_unique<MyApplication>();
  }
  return gMyApplication;
}

MyApplication::MyApplication()
{
  m_title = L"Compute Shader";
}

void MyApplication::Initialize()
{
  auto& gfxDevice = GetGfxDevice();
  GfxDevice::DeviceInitParams initParams;
  initParams.formatDesired = DXGI_FORMAT_R8G8B8A8_UNORM;
  gfxDevice->Initialize(initParams);

  PrepareImGui();

  PrepareSceneConstantBuffer();

  PrepareDrawPipeline();
  PrepareComputePipeline();

  PrepareImageFilterResources();


  // ビューポートおよびシザー領域の設定.
  int width, height;
  Win32Application::GetWindowSize(width, height);
  m_viewport = D3D12_VIEWPORT{
    .TopLeftX = 0.0f, .TopLeftY = 0.0f,
    .Width = float(width),
    .Height = float(height),
    .MinDepth = 0.0f, .MaxDepth = 1.0f,
  };
  m_scissorRect = D3D12_RECT{
    .left = 0, .top = 0,
    .right = width, .bottom = height,
  };

}


void MyApplication::PrepareSceneConstantBuffer()
{
  auto& gfxDevice = GetGfxDevice();
  UINT constantBufferSize = sizeof(SceneParameters);
  constantBufferSize = (constantBufferSize + 255) & ~255u;
  D3D12_RESOURCE_DESC cbResDesc{
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Alignment = 0,
    .Width = constantBufferSize,
    .Height = 1,
    .DepthOrArraySize = 1,
    .MipLevels = 1,
    .Format = DXGI_FORMAT_UNKNOWN,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    .Flags = D3D12_RESOURCE_FLAG_NONE
  };
  for (UINT i = 0; i < GfxDevice::BackBufferCount; ++i)
  {
    auto buffer = gfxDevice->CreateBuffer(cbResDesc, D3D12_HEAP_TYPE_UPLOAD);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
      .BufferLocation = buffer->GetGPUVirtualAddress(),
      .SizeInBytes = constantBufferSize,
    };
    m_constantBuffer[i].buffer = buffer;
    m_constantBuffer[i].descriptorCbv = gfxDevice->CreateConstantBufferView(cbvDesc);
  }
}

void MyApplication::PrepareDrawPipeline()
{
  auto& gfxDevice = GetGfxDevice();
  auto& loader = GetFileLoader();

  // 描画のためのパイプラインステートオブジェクトを作成.
  // ルートシグネチャの作成.
  D3D12_DESCRIPTOR_RANGE rangeSrvRanges[] = {
    {  // t0 テクスチャ.
      .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
      .NumDescriptors = 1,
      .BaseShaderRegister = 0,
      .RegisterSpace = 0,
      .OffsetInDescriptorsFromTableStart = 0,
    }
  };
  D3D12_DESCRIPTOR_RANGE rangeSamplerRanges[] = {
    {  // s0 サンプラー.
      .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
      .NumDescriptors = 1,
      .BaseShaderRegister = 0,
      .RegisterSpace = 0,
      .OffsetInDescriptorsFromTableStart = 0,
    }
  };

  D3D12_ROOT_PARAMETER rootParams[] = {
    {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
      .Constants = {
        .ShaderRegister = 0,
        .RegisterSpace = 0,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    },
    {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
      .DescriptorTable = {
        .NumDescriptorRanges = _countof(rangeSrvRanges),
        .pDescriptorRanges = rangeSrvRanges,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    },
    {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
      .DescriptorTable = {
        .NumDescriptorRanges = _countof(rangeSamplerRanges),
        .pDescriptorRanges = rangeSamplerRanges,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    },
  };

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{
    .NumParameters = _countof(rootParams),
    .pParameters = rootParams,
    .NumStaticSamplers = 0,
    .pStaticSamplers = nullptr,
    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
  };

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
  m_rootSignature = gfxDevice->CreateRootSignature(signature);

  // 頂点データのインプットレイアウト情報を作成.
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    {
      .SemanticName = "POSITION", .SemanticIndex = 0,
      .Format = DXGI_FORMAT_R32G32B32_FLOAT,
      .InputSlot = 0, .AlignedByteOffset = offsetof(Vertex, position),
      .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0,
    },
    {
      .SemanticName = "TEXCOORD", .SemanticIndex = 0,
      .Format = DXGI_FORMAT_R32G32_FLOAT,
      .InputSlot = 0, .AlignedByteOffset = offsetof(Vertex, texcoord),
      .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0,
    },
  };
  D3D12_INPUT_LAYOUT_DESC inputLayout{
    .pInputElementDescs = inputElementDesc,
    .NumElements = _countof(inputElementDesc),
  };
  // シェーダーコードの読み込み.
  std::vector<char> vsdata, psdata;
  loader->Load(L"res/shader/VertexShader.cso", vsdata);
  loader->Load(L"res/shader/PixelShader.cso", psdata);
  D3D12_SHADER_BYTECODE vs{
    .pShaderBytecode = vsdata.data(),
    .BytecodeLength = vsdata.size(),
  };
  D3D12_SHADER_BYTECODE ps{
    .pShaderBytecode = psdata.data(),
    .BytecodeLength = psdata.size(),
  };

  // パイプラインステートオブジェクト作成時に使う各種ステート情報を準備.
  D3D12_BLEND_DESC blendState{
    .AlphaToCoverageEnable = FALSE,
    .IndependentBlendEnable = FALSE,
    .RenderTarget = {
      D3D12_RENDER_TARGET_BLEND_DESC{
        .BlendEnable = FALSE,
        .LogicOpEnable = FALSE,
        .SrcBlend = D3D12_BLEND_ONE,
        .DestBlend = D3D12_BLEND_ZERO,
        .BlendOp = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D12_BLEND_ONE,
        .DestBlendAlpha = D3D12_BLEND_ZERO,
        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
        .LogicOp = D3D12_LOGIC_OP_NOOP,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
      },
    }
  };

  D3D12_RASTERIZER_DESC rasterizerState{
    .FillMode = D3D12_FILL_MODE_SOLID,
    .CullMode = D3D12_CULL_MODE_BACK,
    .FrontCounterClockwise = TRUE,
    .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
    .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
    .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
    .DepthClipEnable = TRUE,
    .MultisampleEnable = FALSE,
    .AntialiasedLineEnable = FALSE,
    .ForcedSampleCount = 0,
    .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
  };
  const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = {
    .StencilFailOp = D3D12_STENCIL_OP_KEEP,
    .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
    .StencilPassOp = D3D12_STENCIL_OP_KEEP,
    .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
  };
  D3D12_DEPTH_STENCIL_DESC depthStencilState{
    .DepthEnable = FALSE,
    .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
    .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
    .StencilEnable = FALSE,
    .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
    .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
    .FrontFace = defaultStencilOp, .BackFace = defaultStencilOp
  };

  // 情報が揃ったのでパイプラインステートオブジェクトを作成する.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.InputLayout = inputLayout;
  psoDesc.pRootSignature = m_rootSignature.Get();
  psoDesc.VS = vs;
  psoDesc.PS = ps;
  psoDesc.RasterizerState = rasterizerState;
  psoDesc.BlendState = blendState;
  psoDesc.DepthStencilState = depthStencilState;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = gfxDevice->GetSwapchainFormat();
  psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  psoDesc.SampleDesc.Count = 1;
  m_drawPipeline = gfxDevice->CreateGraphicsPipelineState(psoDesc);
}

void MyApplication::PrepareComputePipeline()
{
  auto& gfxDevice = GetGfxDevice();
  auto& loader = GetFileLoader();

  // コンピュートシェーダーのためのパイプラインステートオブジェクトを作成.
  // ルートシグネチャの作成.
  D3D12_DESCRIPTOR_RANGE rangeSrvRanges[] = {
    {  // t0 テクスチャ.
      .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
      .NumDescriptors = 1,
      .BaseShaderRegister = 0,
      .RegisterSpace = 0,
      .OffsetInDescriptorsFromTableStart = 0,
    }
  };
  D3D12_DESCRIPTOR_RANGE rangeUavRanges[] = {
    {  // u0 書込先テクスチャ.
      .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
      .NumDescriptors = 1,
      .BaseShaderRegister = 0,
      .RegisterSpace = 0,
      .OffsetInDescriptorsFromTableStart = 0,
    }
  };

  D3D12_ROOT_PARAMETER rootParams[] = {
    {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
      .Constants = {
        .ShaderRegister = 0,
        .RegisterSpace = 0,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    },
    {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
      .DescriptorTable = {
        .NumDescriptorRanges = _countof(rangeSrvRanges),
        .pDescriptorRanges = rangeSrvRanges,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
    },
    {
      .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
      .DescriptorTable = {
        .NumDescriptorRanges = _countof(rangeUavRanges),
        .pDescriptorRanges = rangeUavRanges,
      },
      .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
    },
  };

  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{
    .NumParameters = _countof(rootParams),
    .pParameters = rootParams,
    .NumStaticSamplers = 0,
    .pStaticSamplers = nullptr,
    .Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE
  };

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
  m_rootSignatureCS = gfxDevice->CreateRootSignature(signature);

  // シェーダーコードの読み込み.
  std::vector<char> csdata;
  loader->Load(L"res/shader/ComputeShader.cso", csdata);
  D3D12_SHADER_BYTECODE cs{
    .pShaderBytecode = csdata.data(),
    .BytecodeLength = csdata.size(),
  };

  // 情報が揃ったのでパイプラインステートオブジェクトを作成する.
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{
    .pRootSignature = m_rootSignatureCS.Get(),
    .CS = cs,
    .NodeMask = 1,
    .CachedPSO = { },
    .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
  };

  m_filterPipeline = gfxDevice->CreateComputePipelineState(psoDesc);
}

void MyApplication::PrepareImageFilterResources()
{
  auto& gfxDevice = GetGfxDevice();
  std::filesystem::path filePath = "res/texture/image.png";
  bool generateMips = false;
  CreateTextureFromFile(m_sourceImage, filePath,
    generateMips,
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_FLAG_NONE);
  CreateTextureFromFile(m_filteredImage, filePath,
    generateMips,
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

  auto resDesc = m_sourceImage->GetDesc();
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
    .Format = resDesc.Format,
    .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Texture2D = {
      .MostDetailedMip = 0,
      .MipLevels = resDesc.MipLevels,
      .PlaneSlice = 0,
      .ResourceMinLODClamp = 0,
    }
  };
  m_sourceImageSRV = gfxDevice->CreateShaderResourceView(m_sourceImage, srvDesc);
  m_filteredImageSRV = gfxDevice->CreateShaderResourceView(m_filteredImage, srvDesc);

  // イメージ書込み先となるリソースに対してUAVを作成.
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
    .Format = resDesc.Format,
    .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
    .Texture2D = {
      .MipSlice = 0, .PlaneSlice = 0,
    }
  };
  m_filteredImageUAV = gfxDevice->CreateUnorderedAccessView(m_filteredImage, uavDesc);

  // フィルター実行のためのDispatchサイズを計算.
  auto dispatchAlign = [](auto v) {
    // CSのスレッド数はXY共に16のため.
    return UINT((v + 16 - 1) & ~(16- 1));
  };
  m_filterDispatchSize = {
    .x = dispatchAlign(resDesc.Width),
    .y = dispatchAlign(resDesc.Height)
  };


  float offset = 10.0f;
  Vertex vertices[] = {
    { XMFLOAT3(-480.0f - offset, -135.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
    { XMFLOAT3(   0.0f - offset, -135.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
    { XMFLOAT3(-480.0f - offset,  135.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
    { XMFLOAT3(   0.0f - offset,  135.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },

    { XMFLOAT3(   0.0f + offset, -135.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
    { XMFLOAT3(+480.0f + offset, -135.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
    { XMFLOAT3(   0.0f + offset,  135.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
    { XMFLOAT3(+480.0f + offset,  135.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
  };

  D3D12_RESOURCE_DESC vbResDesc{
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Alignment = 0,
    .Width = sizeof(vertices),
    .Height = 1, .DepthOrArraySize = 1, .MipLevels = 1,
    .Format = DXGI_FORMAT_UNKNOWN,
    .SampleDesc = {.Count = 1, .Quality = 0 },
    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    .Flags = D3D12_RESOURCE_FLAG_NONE,
  };
  m_vertexBuffer = gfxDevice->CreateBuffer(vbResDesc, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, vertices);
  m_vbv.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vbv.SizeInBytes = sizeof(vertices);
  m_vbv.StrideInBytes = sizeof(Vertex);

  D3D12_SAMPLER_DESC samplerDesc{
    .Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
    .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
    .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
    .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
    .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
    .MinLOD = 0, .MaxLOD = D3D12_FLOAT32_MAX,
  };
  m_samplerDescriptor =  gfxDevice->CreateSampler(samplerDesc);
}

void MyApplication::PrepareImGui()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplWin32_Init(Win32Application::GetHwnd());

  auto& gfxDevice = GetGfxDevice();
  auto d3d12Device = gfxDevice->GetD3D12Device();
  // ImGui のフォントデータ用にディスクリプタを割り当てる.
  auto heapCbvSrv = gfxDevice->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  auto fontDescriptor = gfxDevice->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  ImGui_ImplDX12_Init(d3d12Device.Get(),
    gfxDevice->BackBufferCount,
    gfxDevice->GetSwapchainFormat(),
    heapCbvSrv.Get(),
    fontDescriptor.hCpu, fontDescriptor.hGpu
    );
}

void MyApplication::DestroyImGui()
{
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}

void MyApplication::OnUpdate()
{
  // ImGui更新処理.
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  // ImGuiを使用したUIの描画指示.
  ImGui::Begin("Information");
  ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
  ImGui::Combo("Mode", &m_filterMode, "Sepia\0Hue Shift\0\0");
  ImGui::SliderFloat("Offset", &m_hueShift, 0.0f, 1.0f);
  ImGui::End();

  // 行列情報などを更新する.
  XMFLOAT3 eyePos(0, 1.0, 5.0f), target(0, 1.0, 0), upDir(0, 1, 0);
  XMMATRIX mtxView = XMMatrixIdentity();
  XMMATRIX mtxProj = XMMatrixOrthographicOffCenterRH(
    -640.0f, 640.0f, -360.0f, 360.0f, -100.0f, 100.0f);

  XMStoreFloat4x4(&m_sceneParams.mtxView, mtxView);
  XMStoreFloat4x4(&m_sceneParams.mtxProj, mtxProj);
  m_sceneParams.modeParams.x = float(m_filterMode);
  m_sceneParams.modeParams.y = m_hueShift;
  m_frameDeltaAccum += ImGui::GetIO().DeltaTime;

  auto& gfxDevice = GetGfxDevice();
  gfxDevice->NewFrame();

  // 描画のコマンドを作成.
  auto commandList = MakeCommandList();

  // 作成したコマンドを実行.
  gfxDevice->Submit(commandList.Get());
  // 描画した内容を画面へ反映.
  gfxDevice->Present(1);
}

void MyApplication::Shutdown()
{
  auto& gfxDevice = GetGfxDevice();
  gfxDevice->WaitForGPU();

  // リソースを解放.
  m_drawPipeline.Reset();
  m_filterPipeline.Reset();
  m_rootSignature.Reset();
  m_rootSignatureCS.Reset();

  m_vertexBuffer.Reset();
  m_sourceImage.Reset();
  m_filteredImage.Reset();
  for (auto& cb : m_constantBuffer)
  {
    cb.buffer.Reset();
    gfxDevice->DeallocateDescriptor(cb.descriptorCbv);
  }
  gfxDevice->DeallocateDescriptor(m_sourceImageSRV);
  gfxDevice->DeallocateDescriptor(m_filteredImageSRV);
  gfxDevice->DeallocateDescriptor(m_filteredImageUAV);
  gfxDevice->DeallocateDescriptor(m_samplerDescriptor);

  // ImGui破棄処理.
  DestroyImGui();

  // グラフィックスデバイス関連解放.
  gfxDevice->Shutdown();
}

ComPtr<ID3D12GraphicsCommandList>  MyApplication::MakeCommandList()
{
  auto& gfxDevice = GetGfxDevice();
  auto frameIndex = gfxDevice->GetFrameIndex();
  auto commandList = gfxDevice->CreateCommandList();

  // 定数バッファの更新.
  auto cb = m_constantBuffer[frameIndex].buffer;
  void* p;
  cb->Map(0, nullptr, &p);
  memcpy(p, &m_sceneParams, sizeof(m_sceneParams));
  cb->Unmap(0, nullptr);

  ID3D12DescriptorHeap* heaps[] = {
    gfxDevice->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Get(),
    gfxDevice->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER).Get(),
  };
  commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  FilterImage(commandList);

  // 結果を描画する.
  // ルートシグネチャおよびパイプラインステートオブジェクト(PSO)をセット.
  commandList->SetGraphicsRootSignature(m_rootSignature.Get());
  commandList->SetPipelineState(m_drawPipeline.Get());

  commandList->RSSetViewports(1, &m_viewport);
  commandList->RSSetScissorRects(1, &m_scissorRect);

  auto renderTarget = gfxDevice->GetSwapchainBufferResource();
  auto barrierToRT = D3D12_RESOURCE_BARRIER{
    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
    .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
    .Transition = {
      .pResource = renderTarget.Get(),
      .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
      .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
    }
  };

  commandList->ResourceBarrier(1, &barrierToRT);
  auto rtvHandle = gfxDevice->GetSwapchainBufferDescriptor();
  commandList->OMSetRenderTargets(1, &rtvHandle.hCpu, FALSE, nullptr);
  
  const float clearColor[] = { 0.75f, 0.9f, 1.0f, 1.0f };
  commandList->ClearRenderTargetView(rtvHandle.hCpu, clearColor, 0, nullptr);

  auto cbDescriptor = m_constantBuffer[frameIndex].descriptorCbv;
  commandList->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());

  commandList->SetPipelineState(m_drawPipeline.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  commandList->IASetVertexBuffers(0, 1, &m_vbv);
  commandList->SetGraphicsRootDescriptorTable(2, m_samplerDescriptor.hGpu);

  // フィルタ適用前画像を表示.
  commandList->SetGraphicsRootDescriptorTable(1, m_sourceImageSRV.hGpu);
  commandList->DrawInstanced(4, 1, 0, 0);
  // フィルタ適用後画像を表示.
  commandList->SetGraphicsRootDescriptorTable(1, m_filteredImageSRV.hGpu);
  commandList->DrawInstanced(4, 1, 4, 0);

  // ImGui による描画.
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

  // 末尾のリソースバリアをセット.
  //  - スワップチェインを表示可能
  //  - フィルタ処理画像をUAVへ戻す.
  D3D12_RESOURCE_BARRIER barrierFrameEnd[] = {
    {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .Transition = {
        .pResource = renderTarget.Get(),
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
        .StateAfter = D3D12_RESOURCE_STATE_PRESENT,
      },
    },
    {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .Transition = {
        .pResource = m_filteredImage.Get(),
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        .StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      },
    },
    {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .Transition = {
        .pResource = m_sourceImage.Get(),
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        .StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
      },
    },
  };

  commandList->ResourceBarrier(_countof(barrierFrameEnd), barrierFrameEnd);
  commandList->Close();
  return commandList;
}

void MyApplication::FilterImage(ComPtr<ID3D12GraphicsCommandList> commandList)
{
  auto& gfxDevice = GetGfxDevice();
  int frameIndex = gfxDevice->GetFrameIndex();
  auto cb = m_constantBuffer[frameIndex].buffer;

  // フィルター処理をコンピュートシェーダーで行う.
  commandList->SetComputeRootSignature(m_rootSignatureCS.Get());
  commandList->SetPipelineState(m_filterPipeline.Get());
  commandList->SetComputeRootConstantBufferView(0, cb->GetGPUVirtualAddress());
  commandList->SetComputeRootDescriptorTable(1, m_sourceImageSRV.hGpu);
  commandList->SetComputeRootDescriptorTable(2, m_filteredImageUAV.hGpu);
  commandList->Dispatch(m_filterDispatchSize.x, m_filterDispatchSize.y, 1);

  // 変換完了後のバリアを設定.
  D3D12_RESOURCE_BARRIER barrierToSrv[] = {
    {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = {
        .pResource = m_filteredImage.Get(),
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      }
    },
    {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = {
        .pResource = m_sourceImage.Get(),
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      }
    },
  };
  commandList->ResourceBarrier(_countof(barrierToSrv), barrierToSrv);
}

