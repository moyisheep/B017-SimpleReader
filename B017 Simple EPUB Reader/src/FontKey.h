#pragma once
#include <string>

struct FontKey {
    std::wstring family;
    int          weight;
    bool         italic;
    int          size;          // px
    bool operator==(const FontKey& o) const noexcept = default;
};
namespace std {
    template<>
    struct hash<FontKey> {
        size_t operator()(const FontKey& k) const noexcept {
            std::wstring txt;
            txt += k.family;
            txt += std::to_wstring(k.weight);
            txt += std::to_wstring(k.italic);
            txt += std::to_wstring(k.size);
            return std::hash<std::wstring>{}(txt);
        }
    };
}
