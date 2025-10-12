#pragma once

#include <filesystem>
#include <unordered_map>
#include <mutex>

#include "IFileProvider.h"
#include "ZipFileProvider.h"
#include "LocalFileProvider.h"

class EPUBFileProvider : public IFileProvider
{
public:
    EPUBFileProvider();
    ~EPUBFileProvider();
    bool load(std::wstring file_path);
    std::vector<uint8_t>  get_binary(std::wstring path)  override;
    std::string get_string(std::wstring path) override;
    void clear();
private:
    std::unique_ptr<ZipFileProvider> m_zfp;
    std::unique_ptr<LocalFileProvider> m_lfp;
    std::unordered_map<std::wstring, std::vector<uint8_t>> m_bin_cache;
    std::unordered_map<std::wstring, std::string> m_str_cache;
    mutable std::mutex m_mutex;
};