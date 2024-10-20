#pragma once
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "GfxDevice.h"
#include "Model.h"

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
  void PrepareSceneConstantBuffer();
  void PrepareModelDrawPipeline();
  void PrepareModelData();
  void PrepareImGui();
  void DestroyImGui();
  ComPtr<ID3D12GraphicsCommandList> MakeCommandList();

  void DrawModel(ComPtr<ID3D12GraphicsCommandList> commandList);

  struct Vertex
  {
    DirectX::XMFLOAT3 position;
  };

  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12PipelineState> m_drawOpaquePipeline;
  ComPtr<ID3D12PipelineState> m_drawBlendPipeline;

  struct DepthBufferInfo
  {
    ComPtr<ID3D12Resource1> image;
    GfxDevice::DescriptorHandle dsvHandle;
  } m_depthBuffer;
   
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
    DirectX::XMFLOAT4  lightDir = { 0.0f, -1.0f,-1.0f, 0 };  // 光が進む方向(World空間).
    DirectX::XMFLOAT3  eyePosition;
    float    time;
  } m_sceneParams;

  struct PolygonMesh
  {
    D3D12_VERTEX_BUFFER_VIEW vbViews[3];
    D3D12_INDEX_BUFFER_VIEW  ibv;

    ComPtr<ID3D12Resource1>  position;
    ComPtr<ID3D12Resource1>  normal;
    ComPtr<ID3D12Resource1>  texcoord0;
    ComPtr<ID3D12Resource1>  indices;

    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t materialIndex;
  };
  struct MeshMaterial
  {
    ModelMaterial::AlphaMode alphaMode;

    DirectX::XMFLOAT4 diffuse{};  // xyz:色, w:アルファ.
    DirectX::XMFLOAT4 specular{};
    DirectX::XMFLOAT4 ambient{};

    GfxDevice::DescriptorHandle srvDiffuse;
    GfxDevice::DescriptorHandle samplerDiffuse;
  };

  // 定数バッファに書き込む構造体.
  struct DrawParameters
  {
    DirectX::XMFLOAT4X4 mtxWorld;
    DirectX::XMFLOAT4   baseColor; // diffuse + alpha
    DirectX::XMFLOAT4   specular;  // specular
    DirectX::XMFLOAT4   ambient;   // ambient

    uint32_t  mode;
    uint32_t  padd0;
    uint32_t  padd1;
    uint32_t  padd2;
  };

  struct TextureInfo
  {
    std::string filePath;
    ComPtr<ID3D12Resource1> texResource;
    GfxDevice::DescriptorHandle srvDescriptor;
  };
  struct DrawInfo
  {
    ComPtr<ID3D12Resource1> modelMeshConstantBuffer[GfxDevice::BackBufferCount];

    int meshIndex = -1;
    int materialIndex = -1;
  };

  struct ModelData
  {
    std::vector<PolygonMesh> meshes;
    std::vector<MeshMaterial> materials;
    std::vector<DrawInfo> drawInfos;
    std::vector<TextureInfo> textureList;
    std::vector<TextureInfo> embeddedTextures;
    DirectX::XMMATRIX mtxWorld;
  } m_model;
  std::vector<TextureInfo>::const_iterator FindModelTexture(const std::string& filePath, const ModelData& model);

  DirectX::XMFLOAT4 m_globalSpecular = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 30.0f);
  DirectX::XMFLOAT4 m_globalAmbient = DirectX::XMFLOAT4(0.15f, 0.15f, 0.15f, 0.0f);
  bool  m_overwrite = false;

  float m_frameDeltaAccum = 0.0f;
  std::wstring m_title;
};

std::unique_ptr<MyApplication>& GetApplication();
