#pragma once
#include <memory>
#include <vector>
#include <string>

#define NOMINMAX
#include <d3d12.h>
#include <wrl.h>
#include <dxgi1_6.h>

class GfxDevice
{
public:
  template<class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  struct DeviceInitParams
  {
    DXGI_FORMAT formatDesired = DXGI_FORMAT_R8G8B8A8_UNORM;
  };
  void Initialize(const DeviceInitParams& initParams);
  void Shutdown();

  static const int BackBufferCount = 2;
  struct DescriptorHandle
  {
    D3D12_CPU_DESCRIPTOR_HANDLE hCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE hGpu;
    D3D12_DESCRIPTOR_HEAP_TYPE  type;
  };

  // 現在処理対象フレームインデックスを取得.
  UINT GetFrameIndex() const { return m_frameIndex; }
  DescriptorHandle GetSwapchainBufferDescriptor();
  ComPtr<ID3D12Resource1>     GetSwapchainBufferResource();

  void Submit(ID3D12CommandList* const commandList);
  void Present(UINT syncInterval, UINT flags = 0);
  void NewFrame();
  void WaitForGPU();

  ComPtr<ID3D12Resource1> CreateBuffer(const D3D12_RESOURCE_DESC& resDesc, const D3D12_HEAP_PROPERTIES& heapProps);
  ComPtr<ID3D12RootSignature> CreateRootSignature(ComPtr<ID3DBlob> rootSignatureBlob);
  ComPtr<ID3D12PipelineState> CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc);
  ComPtr<ID3D12GraphicsCommandList> CreateCommandList();

  DXGI_FORMAT GetSwapchainFormat() const { return m_dxgiFormat; }

  // ディスクリプタ関連.
  DescriptorHandle AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type);
  void DeallocateDescriptor(DescriptorHandle descriptor);
  ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type);

  // 内部オブジェクトを使うときに使用する.
  //   主にD3D12の使い方をラップせずに見せたいとき.
  ComPtr<ID3D12Device5> GetD3D12Device() { return m_d3d12Device; }
  ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() { return m_commandQueue; }
  ComPtr<ID3D12CommandAllocator> GetD3D12CommandAllocator(int index);

private:
  void ThrowIfFailed(HRESULT hr, const std::string& errorMsg);
  void SelectDevice();
  void CreateSwapchain(DXGI_FORMAT dxgiFormat);
  void CreateDescriptorHeaps();
  void PrepareRenderTargetView();
  void CreateCommandAllocators();
  void DestroyCommandAllocators();

  ComPtr<ID3D12Device5> m_d3d12Device;
  ComPtr<ID3D12CommandQueue> m_commandQueue;
  ComPtr<IDXGIAdapter4> m_adapter;

  int m_width = 0, m_height = 0;
  DXGI_FORMAT m_dxgiFormat = DXGI_FORMAT_UNKNOWN;
  ComPtr<IDXGIFactory7> m_dxgiFactory;
  ComPtr<IDXGISwapChain4> m_swapchain;

  UINT   m_frameIndex = 0;
  HANDLE m_waitFence;
  ComPtr<ID3D12Fence1> m_frameFence;

  // 描画フレーム情報
  struct FrameInfo
  {
    UINT64 fenceValue = 0;
    ComPtr<ID3D12CommandAllocator> commandAllocator;

    DescriptorHandle rtvDescriptor;       // 描画先のRTV
    ComPtr<ID3D12Resource1> targetBuffer; // 描画先バックバッファ.
  };
  FrameInfo m_frameInfo[BackBufferCount];

  // DescriptorHeap
  struct DescriptorHeapInfo
  {
    ComPtr<ID3D12DescriptorHeap> heap;
    UINT handleSize = 0;
    UINT usedIndex = 0;
    std::vector<DescriptorHandle> freeHandles;
  };
  DescriptorHeapInfo* GetDescriptorHeapInfo(D3D12_DESCRIPTOR_HEAP_TYPE);
  DescriptorHeapInfo m_rtvDescriptorHeap;
  DescriptorHeapInfo m_dsvDescriptorHeap;
  DescriptorHeapInfo m_srvDescriptorHeap;
  DescriptorHeapInfo m_samplerDescriptorHeap;
};

std::unique_ptr<GfxDevice>& GetGfxDevice();
