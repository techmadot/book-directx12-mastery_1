#pragma once
#include <memory>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

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
  void PrepareTriangle();
  void PrepareImGui();
  void DestroyImGui();
  ComPtr<ID3D12GraphicsCommandList> DrawTriangle();

  struct Vertex
  {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
  };

  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12PipelineState> m_pipelineState;

  ComPtr<ID3D12Resource1> m_vertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
  D3D12_VIEWPORT m_viewport;
  D3D12_RECT m_scissorRect;

  std::wstring m_title;
};

std::unique_ptr<MyApplication>& GetApplication();
