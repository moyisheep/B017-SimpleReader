#include "LocalFileProvider.h"

std::vector<uint8_t> LocalFileProvider::get_binary(std::wstring path)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    // 文件不存在或非普通文件
    if (!fs::is_regular_file(path, ec) || ec)
        return {};

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};

    file.seekg(0, std::ios::end);
    const auto len = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> data{};
    data.resize(len);
    file.read(reinterpret_cast<char*>(data.data()), len);

    if (!file)        // 读取失败
        return {};

    return data;        // NRVO / move
}