#include "EPUBFileProvider.h"

namespace fs = std::filesystem;

void EPUBFileProvider::clear()
{
    m_bin_cache = {};
    m_str_cache = {};
}
EPUBFileProvider::EPUBFileProvider()
{
    m_lfp = std::make_unique<LocalFileProvider>();
    m_zfp = std::make_unique<ZipFileProvider>();
}

EPUBFileProvider::~EPUBFileProvider()
{
    clear();
}

bool EPUBFileProvider::load(std::wstring path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    clear();
    return m_zfp->load(path);
}

std::vector<uint8_t> EPUBFileProvider::get_binary(std::wstring path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_bin_cache.contains(path))
    {
        return m_bin_cache[path];
    }
    
    std::vector<uint8_t> data{};
    if(fs::exists(path))
    {
        data =  m_lfp->get_binary(path);
    }
    else
    {
        data =  m_zfp->get_binary(path);
    }
    if(!data.empty())
    {
        m_bin_cache.emplace(path, data);
    }
    return data;
}

std::string EPUBFileProvider::get_string(std::wstring path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_str_cache.contains(path))
    {
        return m_str_cache[path];
    }
    auto data = get_binary(path);
    if (!data.empty())
    {
        std::string xml(reinterpret_cast<const char*>(data.data()), reinterpret_cast<const char*>(data.data()) + data.size());
        m_str_cache.emplace(path, xml);
        return xml;
    }
    return "";
 
}
