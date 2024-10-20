#include "Model.h"
#include "GfxDevice.h"
#include "FileLoader.h"

#include "assimp/scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"

#include "assimp/IOSystem.hpp"
#include "assimp/IOStream.hpp"

#include "assimp/GltfMaterial.h" // for alpha mode,...


using namespace DirectX;
namespace
{
  XMFLOAT2 Convert(const aiVector2D& v) { return XMFLOAT2(v.x, v.y); }
  XMFLOAT3 Convert(const aiVector3D& v) { return XMFLOAT3(v.x, v.y, v.z); }
  XMFLOAT3 Convert(const aiColor3D& v) { return XMFLOAT3(v.r, v.g, v.b); }

  XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& from)
  {
    XMFLOAT4X4 to;
    to._11 = from.a1; to._12 = from.b1; to._13 = from.c1; to._14 = from.d1;
    to._21 = from.a2; to._22 = from.b2; to._23 = from.c2; to._24 = from.d2;
    to._31 = from.a3; to._32 = from.b3; to._33 = from.c3; to._34 = from.d3;
    to._41 = from.a4; to._42 = from.b4; to._43 = from.c4; to._44 = from.d4;
    return to;
  }

  D3D12_TEXTURE_ADDRESS_MODE ConvertAddressMode(aiTextureMapMode mode) {
    switch (mode)
    {
    default:
    case aiTextureMapMode_Wrap:
      return D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    case aiTextureMapMode_Clamp:
      return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    case aiTextureMapMode_Mirror:
      return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    }
  };
}
class MemoryIOStream : public Assimp::IOStream
{
private:
  std::vector<char> m_data;
  size_t m_offset;
public:
  MemoryIOStream(std::vector<char>&& fileData) : m_data(std::move(fileData)), m_offset(0)
  {
  }

  size_t Read(void* pvBuffer, size_t pSize, size_t pCount) override
  {
    auto remainBytes = m_data.size() - m_offset;
    auto bytesToRead = pSize * pCount;
    if (bytesToRead > remainBytes)
    {
      bytesToRead = remainBytes;
    }
    memcpy(pvBuffer, m_data.data() + m_offset, bytesToRead);
    m_offset += bytesToRead;
    return bytesToRead / pSize;
  }

  size_t Write(const void* pvBuffer, size_t pSize, size_t pCount) override
  {
    return 0; // サポートしない.
  }

  aiReturn Seek(size_t pOffset, aiOrigin pOrigin) override
  {
    if (pOrigin == aiOrigin_SET)
    {
      m_offset = pOffset;
    }
    if (pOrigin == aiOrigin_CUR)
    {
      m_offset += pOffset;
    }
    if (pOrigin == aiOrigin_END)
    {
      m_offset = m_data.size() - pOffset;
    }

    return aiReturn_SUCCESS;
  }

  size_t Tell() const override
  {
    return m_offset;
  }

  size_t FileSize() const override
  {
    return m_data.size();
  }

  void Flush() override { }
};


class MemoryIOSystem : public Assimp::IOSystem
{
private:
  std::filesystem::path m_basePath;

public:
  MemoryIOSystem(std::filesystem::path basePath)
  {
    m_basePath = basePath;
  }

  bool Exists(const char* file) const override
  {
    return true;
  }

  char getOsSeparator() const override
  {
    return '/';
  }

  Assimp::IOStream* Open(const char* file, const char* mode = "rb") override
  {
    auto filePath = m_basePath / std::string(file);

    std::vector<char> fileData;
    if (!GetFileLoader()->Load(filePath, fileData))
    {
      return nullptr;
    }

    return new MemoryIOStream(std::move(fileData));
  }
  void Close(Assimp::IOStream* fileStream) override
  {
    delete fileStream;
  }
};


bool ModelLoader::Load(std::filesystem::path filePath, std::vector<ModelMesh>& meshes, std::vector<ModelMaterial>& materials, std::vector<ModelEmbeddedTextureData>& embeddedData)
{
  Assimp::Importer importer;
  uint32_t flags = 0;
  flags |= aiProcess_Triangulate;   // 3角形化する.
  flags |= aiProcess_RemoveRedundantMaterials;  // 冗長なマテリアルを削除.
  flags |= aiProcess_GenUVCoords;     // UVを生成.
  flags |= aiProcess_FlipUVs;         // テクスチャ座標系:左上を原点とする.
  flags |= aiProcess_PreTransformVertices;    // モデルデータ内の頂点を変換済みにする.
  flags |= aiProcess_GenSmoothNormals;
  flags |= aiProcess_OptimizeMeshes;

  std::vector<char> fileData;
  if (GetFileLoader()->Load(filePath, fileData) == false)
  {
    return false;
  }
  m_basePath = filePath.parent_path();

  // メモリからのロードのために、カスタムのハンドラを設定しておく.
  // このハンドラは importer 破棄の時に解放される.
  importer.SetIOHandler(new MemoryIOSystem(m_basePath));
  const auto scene = importer.ReadFileFromMemory(fileData.data(), fileData.size(), flags);
  if (scene == nullptr)
  {
    return false;
  }
  if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
  {
    return false;
  }
  for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
  {
    auto& material = materials.emplace_back();
    if (!ReadMaterial(material, scene->mMaterials[i]))
    {
      return false;
    }
  }
  for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
  {
    auto& mesh = meshes.emplace_back();
    if (!ReadMeshes(mesh, scene->mMeshes[i]))
    {
      return false;
    }
  }
  for (uint32_t i = 0; i < scene->mNumTextures; ++i)
  {
    auto& tex = embeddedData.emplace_back();
    if (!ReadEmbeddedTexture(tex, scene->mTextures[i]))
    {
      return false;
    }
  }
  importer.FreeScene();
  return true;
}

bool ModelLoader::ReadMaterial(ModelMaterial& dstMaterial, const aiMaterial* srcMaterial)
{
  XMFLOAT3 diffuse(1.0f, 1.0f, 1.0f);
  XMFLOAT3 specular(1.0f, 1.0f, 1.0f);
  XMFLOAT3 ambient(1.0f, 1.0f, 1.0f);
  if (aiColor3D c; srcMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS)
  {
    diffuse = Convert(c);
  }
  if (aiColor3D c; srcMaterial->Get(AI_MATKEY_COLOR_SPECULAR, c) == AI_SUCCESS)
  {
    specular = Convert(c);
  }
  if (aiColor3D c; srcMaterial->Get(AI_MATKEY_COLOR_AMBIENT, c) == AI_SUCCESS)
  {
    ambient = Convert(c);
  }
  dstMaterial.diffuse = diffuse;
  dstMaterial.specular = specular;
  dstMaterial.ambient = ambient;

  float alpha = 1.0f; // アルファ値の読み込み
  if (srcMaterial->Get(AI_MATKEY_OPACITY, alpha) == AI_SUCCESS)
  {
  } else if (srcMaterial->Get(AI_MATKEY_TRANSPARENCYFACTOR, alpha) == AI_SUCCESS)
  {
    alpha = 1.0f - alpha;
  }
  dstMaterial.alpha = alpha;

  if (aiString alphaMode; srcMaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS)
  {
    std::string mode = alphaMode.C_Str();
    if (mode == "OPAQUE")
    {
      dstMaterial.alphaMode = ModelMaterial::ALPHA_MODE_OPAQUE;
    }
    if (mode == "MASK")
    {
      dstMaterial.alphaMode = ModelMaterial::ALPHA_MODE_MASK;
    }
    if (mode == "ALPHA")
    {
      dstMaterial.alphaMode = ModelMaterial::ALPHA_MODE_BLEND;
    }
  }
  if (dstMaterial.alpha < 1.0f && dstMaterial.alphaMode == ModelMaterial::ALPHA_MODE_OPAQUE)
  {
    dstMaterial.alphaMode = ModelMaterial::ALPHA_MODE_BLEND;
  }

  // テクスチャ関連.
  if (aiString texPath; srcMaterial->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), texPath) == AI_SUCCESS)
  {
    auto fileName = std::string(texPath.C_Str());
    dstMaterial.texDiffuse.filePath = (m_basePath / fileName).string();
    if (fileName[0] == '*')
    {
      dstMaterial.texDiffuse.embeddedIndex = std::atoi(&fileName[1]);
    }

    aiTextureMapMode mapU = aiTextureMapMode_Wrap;
    aiTextureMapMode mapV = aiTextureMapMode_Wrap;
    srcMaterial->Get(AI_MATKEY_MAPPINGMODE_U_DIFFUSE(0), mapU);
    srcMaterial->Get(AI_MATKEY_MAPPINGMODE_V_DIFFUSE(0), mapV);

    dstMaterial.texDiffuse.addressModeU = ConvertAddressMode(mapU);
    dstMaterial.texDiffuse.addressModeV = ConvertAddressMode(mapV);
  }

  return true;
}

bool ModelLoader::ReadMeshes(ModelMesh& dstMesh, const aiMesh* srcMesh)
{
  dstMesh.materialIndex = srcMesh->mMaterialIndex;

  auto vertexCount = srcMesh->mNumVertices;
  dstMesh.positions.resize(vertexCount);
  dstMesh.normals.resize(vertexCount);
  dstMesh.texcoords.resize(vertexCount);
  for (uint32_t i = 0; i < vertexCount; ++i)
  {
    dstMesh.positions[i] = Convert(srcMesh->mVertices[i]);
    dstMesh.normals[i] = Convert(srcMesh->mNormals[i]);

    auto uv = srcMesh->mTextureCoords[0][i];
    dstMesh.texcoords[i] = XMFLOAT2(uv.x, uv.y);
  }

  auto indexCount = srcMesh->mNumFaces * 3;
  dstMesh.indices.reserve(indexCount);
  for (uint32_t i = 0; i < srcMesh->mNumFaces; ++i)
  {
    auto& face = srcMesh->mFaces[i];
    dstMesh.indices.push_back(face.mIndices[0]);
    dstMesh.indices.push_back(face.mIndices[1]);
    dstMesh.indices.push_back(face.mIndices[2]);
  }
  return true;
}

bool ModelLoader::ReadEmbeddedTexture(ModelEmbeddedTextureData& dstEmbeddedTex, const aiTexture* srcTexture)
{
  // バイナリ埋め込みテクスチャのみを対象とする.
  assert(srcTexture->mHeight == 0 && srcTexture->mWidth > 0);
  auto head = reinterpret_cast<const char*>(srcTexture->pcData);
  auto last = head + srcTexture->mWidth;
  dstEmbeddedTex.data.assign(head, last);
  dstEmbeddedTex.name = srcTexture->mFilename.C_Str();
  return true;
}
