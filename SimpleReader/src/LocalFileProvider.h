#pragma once
#include <filesystem>
#include <fstream>

#include "IFileProvider.h"


class LocalFileProvider : public IFileProvider
{
public:

    std::vector<uint8_t> get_binary(std::wstring path)  override;
};
