#pragma once
#include <miniz/miniz.h>
#include <filesystem>
#include <mutex>

#include "IFileProvider.h"
#include "a2w_w2a.h"


class ZipFileProvider : public IFileProvider
{
public:
    ZipFileProvider();
    ~ZipFileProvider();
    bool load(const std::wstring& file_path) ;
    std::vector<uint8_t>  get_binary(std::wstring path)  override;
private:
    mz_zip_archive m_zip = {};
    mutable std::mutex m_mutex;
};