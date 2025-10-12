#include "ZipFileProvider.h"

namespace fs = std::filesystem;
ZipFileProvider::ZipFileProvider() {}
ZipFileProvider::~ZipFileProvider()
{
    mz_zip_reader_end(&m_zip);           // 1. 先关闭旧 zip
}
bool ZipFileProvider::load(const std::wstring& file_path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!fs::exists(file_path))
    {
        OutputDebugStringW(L"[ZipProvider] 文件不存在\n");
        return false;
    }


    mz_zip_reader_end(&m_zip);           // 1. 先关闭旧 zip
    memset(&m_zip, 0, sizeof(m_zip));

    if (!mz_zip_reader_init_file(&m_zip, w2a(file_path).c_str(), 0))
    {
        OutputDebugStringW((L"[ZipProvider] zip 打开失败：" +
            std::to_wstring(mz_zip_get_last_error(&m_zip)) + L"\n").c_str());
        return false;
    }

    return true;
}
std::vector<uint8_t> ZipFileProvider::get_binary(std::wstring path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint8_t> data{};
    std::string narrow_name = w2a(path);
    size_t uncomp_size = 0;
    void* p = mz_zip_reader_extract_file_to_heap(
        const_cast<mz_zip_archive*>(&m_zip),
        narrow_name.c_str(),
        &uncomp_size, 0);

    if (p) {
        data.assign(static_cast<uint8_t*>(p),
            static_cast<uint8_t*>(p) + uncomp_size);
        mz_free(p);
    }
    return data;
}



