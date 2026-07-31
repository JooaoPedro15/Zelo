#include "collectors/detail/text.hpp"

#include <algorithm>
#include <cctype>

#include <windows.h>

namespace zelo::collectors::detail {

std::string to_utf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }

    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                          size, nullptr, nullptr);
    return result;
}

std::wstring to_wide(std::string_view text) {
    if (text.empty()) {
        return {};
    }

    const int size = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                          size);
    return result;
}

std::size_t find_ignoring_case(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return 0;
    }

    const auto found = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                                   [](char left, char right) {
                                       return std::tolower(static_cast<unsigned char>(left)) ==
                                              std::tolower(static_cast<unsigned char>(right));
                                   });
    if (found == haystack.end()) {
        return std::string_view::npos;
    }
    return static_cast<std::size_t>(std::distance(haystack.begin(), found));
}

bool contains_ignoring_case(std::string_view haystack, std::string_view needle) {
    return find_ignoring_case(haystack, needle) != std::string_view::npos;
}

}
