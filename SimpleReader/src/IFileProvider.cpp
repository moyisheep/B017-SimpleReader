#include "IFileProvider.h"

std::string IFileProvider::get_string(std::wstring path)
{
    auto data = get_binary(path);
    if (!data.empty())
    {
        return std::string(reinterpret_cast<const char*>(data.data()), reinterpret_cast<const char*>(data.data()) + data.size());
    }
    return "";
}