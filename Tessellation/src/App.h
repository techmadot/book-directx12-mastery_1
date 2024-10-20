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
  void PrepareDepthBuffer();
  void PrepareTessellationPlane();
  void PrepareImGui();
  void DestroyImGui();
  ComPtr<ID3D12GraphicsCommandList> MakeCommandList();

  struct Vertex
  {
    DirectX::XMFLOAT3 position;
  };

  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12PipelineState> m_pipelineState;
  ComPtr<ID3D12PipelineState> m_pipelineState2;

  struct DepthBufferInfo
  {
    ComPtr<ID3D12Resource1> image;
    GfxDevice::DescriptorHandle dsvHandle;
  } m_depthBuffer;
   

  ComPtr<ID3D12Resource1> m_vertexBuffer;
  ComPtr<ID3D12Resource1> m_indexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
  D3D12_INDEX_BUFFER_VIEW  m_indexBufferView;
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
    DirectX::XMFLOAT4  tessParams = { 32.0f, 32.0f, 0, 0 }; // x: inner, y: outer
    float    time;
    float    reserved[3];
  } m_sceneParams;

  // テッセレーション分割レベル.
  float m_tessLevelInner = 32.0f;
  float m_tessLevelOuter = 16.0f;

  float m_frameDeltaAccum = 0.0f;
  bool  m_useFillColor = false;
  std::wstring m_title;
};

std::unique_ptr<MyApplication>& GetApplication();
