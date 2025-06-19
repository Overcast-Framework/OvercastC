#pragma once
#include <iostream>
#include <string>
#include <typeinfo>
#include <memory>
#include <vector>

namespace OCUtils
{
    static inline void ReplaceAll(std::string& str, const std::string& from, const std::string& to)
    {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }

    template<typename T>
    std::string demangle(const T& obj) {
        const char* mangledName = typeid(obj).name();
        return mangledName;
    }
};