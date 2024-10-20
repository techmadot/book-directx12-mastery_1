#include "FileLoader.h"
#include <fstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static std::unique_ptr<FileLoader> gFileLoader = nullptr;

std::unique_ptr<FileLoader>& GetFileLoader()
{
  if (gFileLoader == nullptr)
  {
    gFileLoader = std::make_unique<FileLoader>();
  }
  return gFileLoader;
}

bool FileLoader::Load(std::filesystem::path filePath, std::vector<char>& fileData)
{
  if (std::filesystem::exists(filePath))
  {
    std::ifstream infile(filePath, std::ios::binary);
    if (infile)
    {
      auto size = infile.seekg(0, std::ios::end).tellg();
      fileData.resize(size);
      infile.seekg(0, std::ios::beg).read(fileData.data(), size);
      return true;
    }
  }
  // exe 直接実行されたときの対策.
  filePath = std::filesystem::path("../../") / filePath;
  if (std::filesystem::exists(filePath))
  {
    std::ifstream infile(filePath, std::ios::binary);
    if (infile)
    {
      auto size = infile.seekg(0, std::ios::end).tellg();
      fileData.resize(size);
      infile.seekg(0, std::ios::beg).read(fileData.data(), size);
      return true;
    }
  }
#if _DEBUG
  // Not Found
  DebugBreak();
#endif
  return false;
}

