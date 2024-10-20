#pragma once
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "GfxDevice.h"

class MyApplication 
{
  template<class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;
public:
  MyApplication();
  std::wstring GetTitle() const { return m_title; }

  void Initialize();
  void OnUpdate();
  void Shutdown();

private:
  void PrepareSceneConstantBuffer();
  void PrepareDrawPipeline();
  void PrepareComputePipeline();

  void PrepareImageFilterResources();

  void PrepareImGui();
  void DestroyImGui();
  ComPtr<ID3D12GraphicsCommandList> MakeCommandList();

  void FilterImage(ComPtr<ID3D12GraphicsCommandList> commandList);

  struct Vertex
  {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT2 texcoord;
  };

  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12RootSignature> m_rootSignatureCS;
  ComPtr<ID3D12PipelineState> m_drawPipeline;
  ComPtr<ID3D12PipelineState> m_filterPipeline;

  ComPtr<ID3D12Resource1> m_sourceImage;
  ComPtr<ID3D12Resource1> m_filteredImage;
  GfxDevice::DescriptorHandle m_sourceImageSRV;
  GfxDevice::DescriptorHandle m_filteredImageSRV;
  GfxDevice::DescriptorHandle m_filteredImageUAV;

  struct FilterDispatchSize
  {
    UINT x, y;
  } m_filterDispatchSize;

  D3D12_VERTEX_BUFFER_VIEW m_vbv;
  ComPtr<ID3D12Resource1> m_vertexBuffer;
  GfxDevice::DescriptorHandle m_samplerDescriptor;

  D3D12_VIEWPORT m_viewport;
  D3D12_RECT m_scissorRect;

  struct ConstantBufferInfo
  {
    ComPtr<ID3D12Resource1> buffer;
    GfxDevice::DescriptorHandle descriptorCbv;
  } m_constantBuffer[GfxDevice::BackBufferCount];

  // コンスタントバッファに送るために1要素16バイトアライメントとった状態にしておく.
  struct SceneParameters
  {
    DirectX::XMFLOAT4X4 mtxView;
    DirectX::XMFLOAT4X4 mtxProj;
    DirectX::XMFLOAT4 modeParams;
  } m_sceneParams;

  int m_filterMode = 0;
  float m_hueShift = 0.5f;

  float m_frameDeltaAccum = 0.0f;
  std::wstring m_title;
};

std::unique_ptr<MyApplication>& GetApplication();
