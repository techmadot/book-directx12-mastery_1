#pragma once
#include "GfxDevice.h"
#include <filesystem>

// ファイルからテクスチャを作成.
// テクスチャは GPU 転送済み、ミップマップ作成ありで作成される.
bool CreateTextureFromFile(
  Microsoft::WRL::ComPtr<ID3D12Resource1>& outImage,
  std::filesystem::path filePath,
  bool generateMips = false,
  D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
  D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE);

// メモリからテクスチャを作成.
// テクスチャは GPU 転送済み、ミップマップ作成ありで作成される.
bool CreateTextureFromMemory(
  Microsoft::WRL::ComPtr<ID3D12Resource1>& outImage,
  const void* srcBuffer, size_t bufferSize,
  bool generateMips = false,
  D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
  D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE);

