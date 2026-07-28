#include "core/confidence/confidence.hpp"

#include <algorithm>
#include <utility>

namespace zelo::core {

Confidence Confidence::from_signals(std::vector<ConfidenceSignal> signals) {
    Confidence confidence;
    for (auto& signal : signals) {
        if (signal.weight == 0.0) {
            continue;
        }
        confidence.value_ += signal.weight;
        confidence.reasons_.push_back(std::move(signal.reason));
    }
    confidence.value_ = std::clamp(confidence.value_, 0.0, kMaximumValue);
    return confidence;
}

Confidence Confidence::restored(double value, std::vector<std::string> reasons) {
    Confidence confidence;
    confidence.value_ = std::clamp(value, 0.0, kMaximumValue);
    confidence.reasons_ = std::move(reasons);
    return confidence;
}

double Confidence::value() const {
    return value_;
}

const std::vector<std::string>& Confidence::reasons() const {
    return reasons_;
}

}
