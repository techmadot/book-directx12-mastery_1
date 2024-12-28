#include "TextureUtility.h"
#include "FileLoader.h"

#include "stb/stb_image.h"
#include "stb/stb_image_resize.h"

#include <numeric>
#include <algorithm>
#include <cassert>

#define USE_STB_LIBRARY

// Microsoft製のライブラリが使用可能な状態の時にはそちらを優先.
#if __has_include("d3dx12/d3dx12.h") && __has_include("DirectXTex.h")
  #include "DirectXTex.h"
  #include "d3dx12.h" // DirectX-Graphics-Samples の HelloTriangleに含まれていたものを使用.
  #undef USE_STB_LIBRARY
#endif

bool CreateTextureFromFile(Microsoft::WRL::ComPtr<ID3D12Resource1>& outImage, std::filesystem::path filePath, bool generateMips, D3D12_RESOURCE_STATES afterState, D3D12_RESOURCE_FLAGS resFlags)
{
  auto& loader = GetFileLoader();
  if (std::vector<char> fileData; loader->Load(filePath, fileData))
  {
    return CreateTextureFromMemory(outImage, fileData.data(), fileData.size(), generateMips, afterState, resFlags);
  }
  return false;
}

#if defined(USE_STB_LIBRARY)
bool CreateTextureFromMemory(Microsoft::WRL::ComPtr<ID3D12Resource1>& outImage, const void* srcBuffer, size_t bufferSize, bool generateMips, D3D12_RESOURCE_STATES afterState, D3D12_RESOURCE_FLAGS resFlags)
{
  auto& gfxDevice = GetGfxDevice();
  using namespace std;

  auto buffer = reinterpret_cast<const stbi_uc*>(srcBuffer);
  int imageWidth = 0, imageHeight = 0, comp;
  int result = stbi_info_from_memory(
    buffer, int(bufferSize), &imageWidth, &imageHeight, &comp);
  if (result == 0)
  {
    return false;
  }

  auto mipmapCount = uint32_t(floor(log2(max(imageWidth, imageHeight))) + 1);
  if (generateMips == false)
  {
    mipmapCount = 1;
  }
  int channels = 4;
  const int pixelBytes = sizeof(uint32_t);
  auto srcImage = stbi_load_from_memory(buffer, int(bufferSize), &imageWidth, &imageHeight, nullptr, channels);

  D3D12_RESOURCE_DESC texDesc{
    .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
    .Alignment = 0,
    .Width = UINT(imageWidth), .Height = UINT(imageHeight), .DepthOrArraySize = 1,
    .MipLevels = UINT16(mipmapCount),
    .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
    .SampleDesc = {.Count = 1, .Quality = 0 },
    .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
    .Flags = resFlags,
  };
  D3D12_HEAP_PROPERTIES heapProps{
    .Type = D3D12_HEAP_TYPE_DEFAULT,
    .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
    .CreationNodeMask = 1, .VisibleNodeMask = 1,
  };
  outImage = gfxDevice->CreateImage2D(texDesc, heapProps, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);

  // ミップマップイメージを作成する.
  std::vector<stbi_uc*> workImages;
  if(generateMips)
  {
    int width = imageWidth, height = imageHeight;
    stbi_uc* resizeSrc = srcImage;
    while (width > 1 && height > 1)
    {
      int mipWidth = std::max(1, width / 2);
      int mipHeight = std::max(1, height / 2);
      int surfaceByteSize = mipWidth * mipHeight * pixelBytes;
      auto mipmapImage = new stbi_uc[surfaceByteSize];
      workImages.push_back(mipmapImage);

      stbir_resize_uint8(resizeSrc, width, height, 0, mipmapImage, mipWidth, mipHeight, 0, channels);
      // 次回に備えてセット.
      resizeSrc = mipmapImage;
      width = mipWidth;
      height = mipHeight;
    }
  }

  // ステージングバッファを作成.
  // まずは必要なバッファサイズを求める.
  UINT64 uploadBufferRequiredSize = 0;
  UINT64 uploadOffset = 0;
  std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(mipmapCount);
  std::vector<UINT> numRows(mipmapCount);
  std::vector<UINT64> rowSizeInByte(mipmapCount);
  auto d3d12Device = gfxDevice->GetD3D12Device();

  d3d12Device->GetCopyableFootprints(
    &texDesc, 0, mipmapCount, uploadOffset, footprints.data(), numRows.data(), rowSizeInByte.data(), &uploadBufferRequiredSize);

  // ステージングバッファの確保を書込み.
  D3D12_RESOURCE_DESC resDesc{
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Alignment = 0,
    .Width = uploadBufferRequiredSize, .Height = 1, .DepthOrArraySize = 1, .MipLevels = 1,
    .Format = DXGI_FORMAT_UNKNOWN,
    .SampleDesc = {.Count = 1, .Quality = 0 },
    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    .Flags = D3D12_RESOURCE_FLAG_NONE,
  };
  auto staging = gfxDevice->CreateBuffer(resDesc, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

  void* dest;
  staging->Map(0, nullptr, &dest);
  BYTE* destBase = reinterpret_cast<BYTE*>(dest);
  for (UINT mip = 0; mip < mipmapCount; ++mip)
  {
    const auto& footprint = footprints[mip];
    auto* dstMip = destBase + footprint.Offset;
    BYTE* srcMip = nullptr;
    if (mip == 0)
    {
      srcMip = reinterpret_cast<BYTE*>(srcImage);
    }
    else
    {
      srcMip = workImages[mip - 1];
    }
    assert(srcMip);

    // コピー.
    for (UINT row = 0; row < numRows[mip]; row++)
    {
      auto dst = dstMip + row * footprint.Footprint.RowPitch;
      auto src = srcMip + row * rowSizeInByte[mip];
      auto size = rowSizeInByte[mip];
      memcpy(dst, src, size);
    }
  }
  staging->Unmap(0, nullptr);

  // 転送
  auto commandList = gfxDevice->CreateCommandList();

  for (UINT mip = 0; mip < mipmapCount; ++mip)
  {
    D3D12_TEXTURE_COPY_LOCATION dstLoc{
      .pResource = outImage.Get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = mip,
    };
    D3D12_TEXTURE_COPY_LOCATION srcLoc{
      .pResource = staging.Get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = footprints[mip],
    };

    commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
  }
  D3D12_RESOURCE_BARRIER barrier{
    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
    .Transition = {
      .pResource = outImage.Get(),
      .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
      .StateAfter = afterState,
    }
  };
  commandList->ResourceBarrier(1, &barrier);
  commandList->Close();
  gfxDevice->Submit(commandList.Get());

  gfxDevice->WaitForGPU();

  stbi_image_free(srcImage);
  for (auto& v : workImages)
  {
    delete[] v;
  }
  staging.Reset();
  return true;
}

#endif

#if !defined(USE_STB_LIBRARY)
bool CreateTextureFromMemory(Microsoft::WRL::ComPtr<ID3D12Resource1>& outImage, const void* srcBuffer, size_t bufferSize, bool generateMips, D3D12_RESOURCE_STATES afterState, D3D12_RESOURCE_FLAGS resFlags)
{
  auto& gfxDevice = GetGfxDevice();
  using namespace std;

  DirectX::ScratchImage image;
  DirectX::TexMetadata  metadata;

  HRESULT hr = DirectX::LoadFromTGAMemory(srcBuffer, bufferSize, &metadata, image);
  if (FAILED(hr))
  {
    auto wicFlags = DirectX::WIC_FLAGS_NONE; 
    wicFlags |= DirectX::WIC_FLAGS_IGNORE_SRGB; // DXGI_FORMAT_R8G8B8A8_UNORM のように+_SRGBなしで処理しておきたいので設定.
    hr = DirectX::LoadFromWICMemory(srcBuffer, bufferSize, wicFlags, &metadata, image);
  }
  // 必要ならDDSなど他の形式をチェック.
  // ....

  if (FAILED(hr))
  {
    return false;
  }

  if (generateMips && metadata.mipLevels == 1)
  {
    // ミップマップを作成する.
    DirectX::ScratchImage mipChain;
    DirectX::GenerateMipMaps(image.GetImages()[0], DirectX::TEX_FILTER_DEFAULT, 0, mipChain);
    image = std::move(mipChain);
    metadata.mipLevels = image.GetMetadata().mipLevels;
  }

  auto d3d12Device = gfxDevice->GetD3D12Device();
  Microsoft::WRL::ComPtr<ID3D12Resource> texture;
  DirectX::CREATETEX_FLAGS createFlags = DirectX::CREATETEX_DEFAULT;
  hr = DirectX::CreateTextureEx(d3d12Device.Get(), metadata, resFlags, createFlags, &texture);
  if (FAILED(hr))
  {
    return false;
  }

  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  DirectX::PrepareUpload(d3d12Device.Get(), image.GetImages(), image.GetImageCount(), metadata, subresources);
  auto uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
  
  // アップロードヒープの準備.
  CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
  auto uploadResDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
  ID3D12Resource1* uploadHeap = nullptr;
  hr = d3d12Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &uploadResDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadHeap));
  if (FAILED(hr))
  {
    return false;
  }

  auto commandList = gfxDevice->CreateCommandList();
  UpdateSubresources(commandList.Get(), texture.Get(), uploadHeap, 0, 0, UINT(subresources.size()), subresources.data());

  // シェーダーリソースへ変更.
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, afterState
  );
  commandList->ResourceBarrier(1, &barrier);
  commandList->Close();

  gfxDevice->Submit(commandList.Get());
  gfxDevice->WaitForGPU();

  texture.As(&outImage);
  return true;
}
#endif
