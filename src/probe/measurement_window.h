#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <ctime>

enum class WindowDecision {
    InWindow,
    LateDrained,
    RejectedBeforeStart,
    RejectedAtOrAfterDeadline
};

inline uint64_t getMonotonicTimeNs() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        throw std::runtime_error("clock_gettime(CLOCK_MONOTONIC) failed");
    }
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<uint64_t>(ts.tv_nsec);
}

struct MeasurementWindow {
    uint64_t start_ns = 0;
    uint64_t deadline_ns = 0;

    MeasurementWindow() = default;
    MeasurementWindow(uint64_t start, uint64_t deadline)
        : start_ns(start), deadline_ns(deadline) {
        if (deadline_ns <= start_ns) {
            throw std::invalid_argument("MeasurementWindow: deadline_ns must be greater than start_ns");
        }
    }

    // Safely construct from start_ns and duration_seconds, checking for finite/positive/overflow
    static MeasurementWindow fromDurationSeconds(uint64_t start_ns, double duration_seconds) {
        if (!std::isfinite(duration_seconds) || duration_seconds <= 0.0) {
            throw std::invalid_argument("MeasurementWindow: duration_seconds must be finite and positive");
        }
        double duration_ns_d = duration_seconds * 1e9;
        if (duration_ns_d > static_cast<double>(UINT64_MAX - start_ns) || duration_ns_d > 1e18) {
            throw std::overflow_error("MeasurementWindow: duration_seconds causes integer overflow in nanoseconds");
        }
        uint64_t duration_ns = static_cast<uint64_t>(duration_ns_d);
        if (duration_ns == 0) {
            throw std::invalid_argument("MeasurementWindow: duration_ns rounded to 0");
        }
        return MeasurementWindow(start_ns, start_ns + duration_ns);
    }

    uint64_t duration_ns() const {
        return deadline_ns - start_ns;
    }

    bool canSubmit(uint64_t submit_ns) const {
        return (submit_ns >= start_ns && submit_ns < deadline_ns);
    }

    WindowDecision evaluateOperation(uint64_t submit_ns, uint64_t completion_ns) const {
        if (completion_ns < submit_ns) {
            throw std::invalid_argument("MeasurementWindow: completion_ns cannot be strictly before submit_ns");
        }
        if (submit_ns < start_ns) {
            return WindowDecision::RejectedBeforeStart;
        }
        if (submit_ns >= deadline_ns) {
            return WindowDecision::RejectedAtOrAfterDeadline;
        }
        // submit_ns in [start_ns, deadline_ns)
        if (completion_ns < deadline_ns) {
            return WindowDecision::InWindow;
        }
        return WindowDecision::LateDrained;
    }
};

struct LatencyStats {
    uint64_t count = 0;
    double p50_us = 0.0;
    double p95_us = 0.0;
    double p99_us = 0.0;

    // Nearest-rank method: ceil(p * N) - 1, indexed from 0.
    // Throws std::runtime_error if samples is empty.
    static LatencyStats calculate(const std::vector<double>& sorted_samples_us) {
        if (sorted_samples_us.empty()) {
            throw std::runtime_error("LatencyStats: cannot calculate quantiles on empty samples");
        }
        LatencyStats stats;
        stats.count = static_cast<uint64_t>(sorted_samples_us.size());
        size_t n = sorted_samples_us.size();

        auto nearest_rank = [n, &sorted_samples_us](double p) -> double {
            if (p <= 0.0) return sorted_samples_us.front();
            if (p >= 1.0) return sorted_samples_us.back();
            size_t rank = static_cast<size_t>(std::ceil(p * static_cast<double>(n)));
            size_t idx = (rank > 0) ? rank - 1 : 0;
            if (idx >= n) idx = n - 1;
            return sorted_samples_us[idx];
        };

        stats.p50_us = nearest_rank(0.50);
        stats.p95_us = nearest_rank(0.95);
        stats.p99_us = nearest_rank(0.99);
        return stats;
    }
};
