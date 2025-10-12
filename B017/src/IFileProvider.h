#pragma once
#include <string>


#include "MemFile.h"

class IFileProvider {
public:
    virtual ~IFileProvider() = default;
    virtual std::vector<uint8_t> get_binary(std::wstring path) = 0;
    virtual std::string get_string(std::wstring path);

};