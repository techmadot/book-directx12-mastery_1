#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <filesystem>

#include "assimp/scene.h"
#include "GfxDevice.h"
#include <DirectXMath.h>

struct ModelMesh
{
  std::vector<DirectX::XMFLOAT3> positions;
  std::vector<DirectX::XMFLOAT3> normals;
  std::vector<DirectX::XMFLOAT2> texcoords;
  std::vector<uint32_t> indices;

  uint32_t materialIndex;
};

struct ModelTexture
{
  std::string filePath;
  D3D12_TEXTURE_ADDRESS_MODE addressModeU;
  D3D12_TEXTURE_ADDRESS_MODE addressModeV;
  int embeddedIndex = -1;
};
struct ModelEmbeddedTextureData
{
  std::string name;
  std::vector<char> data;
};

struct ModelMaterial
{
  DirectX::XMFLOAT3 diffuse;
  DirectX::XMFLOAT3 specular;
  DirectX::XMFLOAT3 ambient;

  enum AlphaMode {
    ALPHA_MODE_OPAQUE = 0,
    ALPHA_MODE_MASK,
    ALPHA_MODE_BLEND,
  };
  AlphaMode alphaMode = ALPHA_MODE_OPAQUE;
  float alpha = 1.0f;

  ModelTexture texDiffuse;
};


class ModelLoader
{
public:
  bool Load(std::filesystem::path filePath,
    std::vector<ModelMesh>& meshes,
    std::vector<ModelMaterial>& materials,
    std::vector<ModelEmbeddedTextureData>& embeddedData);

private:
  bool ReadMaterial(ModelMaterial& dstMaterial, const aiMaterial* srcMaterial);
  bool ReadMeshes(ModelMesh& dstMesh, const aiMesh* srcMesh);
  bool ReadEmbeddedTexture(ModelEmbeddedTextureData& dstEmbeddedTex, const aiTexture* srcTexture);
  std::filesystem::path m_basePath;
};