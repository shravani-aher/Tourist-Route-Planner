#include "Models.h"
#include <cmath>

namespace planner {

bool PreferenceWeights::validate_and_normalize(std::string& error_msg) {
    if (std::isnan(wd) || std::isnan(wt) || std::isnan(ws) || std::isnan(wc) || std::isnan(wp)) {
        error_msg = "Weights cannot be NaN";
        return false;
    }
    if (std::isinf(wd) || std::isinf(wt) || std::isinf(ws) || std::isinf(wc) || std::isinf(wp)) {
        error_msg = "Weights cannot be Infinite";
        return false;
    }
    if (wd < 0.0 || wt < 0.0 || ws < 0.0 || wc < 0.0 || wp < 0.0) {
        error_msg = "Weights cannot be negative";
        return false;
    }

    double sum = wd + wt + ws + wc + wp;
    if (sum <= 1e-9) {
        error_msg = "Sum of weights cannot be zero or negligible";
        return false;
    }

    wd /= sum;
    wt /= sum;
    ws /= sum;
    wc /= sum;
    wp /= sum;
    return true;
}

} // namespace planner
