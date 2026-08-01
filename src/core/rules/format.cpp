#include "core/rules/format.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace cleaner::core {

namespace {

std::string with_decimal_comma(std::string text) {
    std::replace(text.begin(), text.end(), '.', ',');
    return text;
}

}

std::string format_bytes(std::uint64_t bytes) {
    constexpr std::array kUnits{"bytes", "KB", "MB", "GB", "TB"};
    constexpr double kStep = 1024.0;

    auto value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= kStep && unit + 1 < kUnits.size()) {
        value /= kStep;
        ++unit;
    }

    std::ostringstream text;
    text.precision(unit == 0 ? 0 : 1);
    text << std::fixed << value << " " << kUnits.at(unit);
    return with_decimal_comma(text.str());
}

std::string format_percentage(double ratio) {
    std::ostringstream text;
    text.precision(0);
    text << std::fixed << ratio * 100.0 << "%";
    return text.str();
}

}
