#include "App.h"

#include "GfxDevice.h"
#include "FileLoader.h"
#include "Win32Application.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "imgui/backends/imgui_impl_win32.h"

using namespace Microsoft::WRL;

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
  m_title = L"Hello,Triangle";
}

void MyApplication::Initialize()
{
  auto& gfxDevice = GetGfxDevice();
  GfxDevice::DeviceInitParams initParams;
  initParams.formatDesired = DXGI_FORMAT_R8G8B8A8_UNORM;
  gfxDevice->Initialize(initParams);

  PrepareImGui();
  PrepareTriangle();

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


void MyApplication::PrepareTriangle()
{
  auto& gfxDevice = GetGfxDevice();
  auto& loader = GetFileLoader();

  // 3角形のポリゴンデータを準備する.
  Vertex triangleVertices[] =
  {
    Vertex{ { 0.5f,-0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
    Vertex{ { 0.0f, 0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
    Vertex{ {-0.5f,-0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
  };
  const UINT vertexBufferSize = sizeof(triangleVertices);

  D3D12_HEAP_PROPERTIES uploadHeap{
    .Type = D3D12_HEAP_TYPE_UPLOAD,
    .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
    .CreationNodeMask = 0, .VisibleNodeMask = 0,
  };
  D3D12_RESOURCE_DESC resDesc{
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Alignment = 0,
    .Width = sizeof(triangleVertices),
    .Height = 1,
    .DepthOrArraySize = 1,
    .MipLevels = 1,
    .Format = DXGI_FORMAT_UNKNOWN,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    .Flags = D3D12_RESOURCE_FLAG_NONE
  };
  m_vertexBuffer = gfxDevice->CreateBuffer(resDesc, uploadHeap);

  void* mapped;
  m_vertexBuffer->Map(0, nullptr, &mapped);
  if (mapped)
  {
    memcpy(mapped, triangleVertices, sizeof(triangleVertices));
    m_vertexBuffer->Unmap(0, nullptr);
  }
  // この頂点バッファを示すD3D12_VERTEX_BUFFER_VIEWを作成.
  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.StrideInBytes = sizeof(Vertex);
  m_vertexBufferView.SizeInBytes = vertexBufferSize;

  // 描画のためのパイプラインステートオブジェクトを作成.
  // ルートシグネチャの作成.
  D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{
    .NumParameters = 0,
    .pParameters = nullptr,
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
      .InputSlot = 0,
      .AlignedByteOffset = offsetof(Vertex, position),
      .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0,
    },
    {
      .SemanticName = "COLOR", .SemanticIndex = 0,
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .InputSlot = 0,
      .AlignedByteOffset = offsetof(Vertex, color),
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
  psoDesc.SampleDesc.Count = 1;
  m_pipelineState = gfxDevice->CreateGraphicsPipelineState(psoDesc);
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
  ImGui::Text("Hello Triangle");
  ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);

  static bool m_checkBox;
  if (ImGui::Checkbox("CheckBox", &m_checkBox))
  {
    // 値が変化したとき、実行される.
  }

  static float m_sliderValue = 0.5;
  if (ImGui::SliderFloat("Slider", &m_sliderValue, 0.0f, 1.0f))
  {
    // 値が変化したとき、実行される.
  }
  static float m_materialColor[4] = { 0 };
  ImGui::ColorEdit4("MaterialColor", (float*)&m_materialColor);
  if (ImGui::ColorPicker4("MaterialColor", (float*)&m_materialColor))
  {
    // 値が変化したとき、実行される.
  }

  static int m_selectedIndex = 0;
  if (ImGui::Combo("Combo", &m_selectedIndex, "Item A\0Item B\0Item C\0\0"))
  {
    // 選択項目が変化したとき、実行される.
  }
  ImGui::End();

  auto& gfxDevice = GetGfxDevice();
  gfxDevice->NewFrame();

  // 描画のコマンドを作成.
  auto commandList = DrawTriangle();

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
  m_pipelineState.Reset();
  m_rootSignature.Reset();
  m_vertexBuffer.Reset();

  // ImGui破棄処理.
  DestroyImGui();

  // グラフィックスデバイス関連解放.
  gfxDevice->Shutdown();
}

ComPtr<ID3D12GraphicsCommandList>  MyApplication::DrawTriangle()
{
  auto& gfxDevice = GetGfxDevice();
  auto commandList = gfxDevice->CreateCommandList();

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

  // ルートシグネチャおよびパイプラインステートオブジェクト(PSO)をセット.
  commandList->SetGraphicsRootSignature(m_rootSignature.Get());
  commandList->SetPipelineState(m_pipelineState.Get());
  commandList->RSSetViewports(1, &m_viewport);
  commandList->RSSetScissorRects(1, &m_scissorRect);

  auto rtvHandle = gfxDevice->GetSwapchainBufferDescriptor();
  commandList->OMSetRenderTargets(1, &rtvHandle.hCpu, FALSE, nullptr);

  const float clearColor[] = { 0.75f, 0.9f, 1.0f, 1.0f };
  commandList->ClearRenderTargetView(rtvHandle.hCpu, clearColor, 0, nullptr);
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  commandList->DrawInstanced(3, 1, 0, 0);

  // ImGui による描画.
  ID3D12DescriptorHeap* heaps[] = {
    gfxDevice->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV).Get(),
  };
  commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

  D3D12_RESOURCE_BARRIER barrierToPresent{
    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
    .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
    .Transition = {
      .pResource = renderTarget.Get(),
      .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
      .StateAfter = D3D12_RESOURCE_STATE_PRESENT,
    }
  };
  commandList->ResourceBarrier(1, &barrierToPresent);

  commandList->Close();

  return commandList;
}

