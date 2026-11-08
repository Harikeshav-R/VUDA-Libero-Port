#include "worker_wire_protocol.h"
#include "operation_trace.h"
#include "measurement_window.h"

#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <limits>

void test_window_decisions() {
    MeasurementWindow window(100, 200);

    assert(window.duration_ns() == 100);

    // Can submit checks: [start, deadline)
    assert(!window.canSubmit(99));
    assert(window.canSubmit(100));
    assert(window.canSubmit(150));
    assert(window.canSubmit(199));
    assert(!window.canSubmit(200));
    assert(!window.canSubmit(250));

    // Contract test cases:
    // With window [100, 200):
    // 1. submit/completion (150, 190) counts (InWindow)
    assert(window.evaluateOperation(150, 190) == WindowDecision::InWindow);

    // 2. (190, 200) is late/drained (completed at deadline)
    assert(window.evaluateOperation(190, 200) == WindowDecision::LateDrained);

    // 3. (190, 210) is late/drained (completed after deadline)
    assert(window.evaluateOperation(190, 210) == WindowDecision::LateDrained);

    // 4. Submit at 200 is disallowed (RejectedAtOrAfterDeadline)
    assert(window.evaluateOperation(200, 210) == WindowDecision::RejectedAtOrAfterDeadline);

    // 5. Submit before 100 is disallowed (RejectedBeforeStart)
    assert(window.evaluateOperation(50, 90) == WindowDecision::RejectedBeforeStart);
    assert(window.evaluateOperation(50, 150) == WindowDecision::RejectedBeforeStart);

    // Boundary edge: submit at 100, completion at 199 -> InWindow
    assert(window.evaluateOperation(100, 199) == WindowDecision::InWindow);
    // Boundary edge: submit at 199, completion at 199 -> InWindow
    assert(window.evaluateOperation(199, 199) == WindowDecision::InWindow);

    // Completion before submit must throw std::invalid_argument
    bool caught_completion_before_submit = false;
    try {
        window.evaluateOperation(150, 140);
    } catch (const std::invalid_argument&) {
        caught_completion_before_submit = true;
    }
    assert(caught_completion_before_submit);

    std::cout << "[test_window_decisions] PASSED" << std::endl;
}

void test_invalid_window() {
    bool caught = false;
    try {
        MeasurementWindow w(200, 100);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        MeasurementWindow w(100, 100);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    // Test fromDurationSeconds invalid/nonfinite/overflow
    caught = false;
    try {
        MeasurementWindow::fromDurationSeconds(100, -5.0);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        MeasurementWindow::fromDurationSeconds(100, 0.0);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        MeasurementWindow::fromDurationSeconds(100, std::numeric_limits<double>::infinity());
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        MeasurementWindow::fromDurationSeconds(100, std::numeric_limits<double>::quiet_NaN());
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        // Overflow test: start_ns very large and large duration
        uint64_t large_start = std::numeric_limits<uint64_t>::max() - 1000;
        MeasurementWindow::fromDurationSeconds(large_start, 10.0);
    } catch (const std::overflow_error&) {
        caught = true;
    }
    assert(caught);

    // Valid conversion
    auto valid_win = MeasurementWindow::fromDurationSeconds(1000, 2.5);
    assert(valid_win.start_ns == 1000);
    assert(valid_win.deadline_ns == 1000 + 2500000000ULL);
    assert(valid_win.duration_ns() == 2500000000ULL);

    std::cout << "[test_invalid_window] PASSED" << std::endl;
}

void test_latency_quantiles() {
    // 10 samples: 1.0, 2.0, ..., 10.0
    // n = 10
    // p = 0.50 -> ceil(0.50 * 10) - 1 = 5 - 1 = 4 -> 5.0
    // p = 0.95 -> ceil(0.95 * 10) - 1 = 10 - 1 = 9 -> 10.0
    // p = 0.99 -> ceil(0.99 * 10) - 1 = 10 - 1 = 9 -> 10.0
    std::vector<double> samples_10 = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    auto stats10 = LatencyStats::calculate(samples_10);
    assert(stats10.count == 10);
    assert(std::fabs(stats10.p50_us - 5.0) < 1e-6);
    assert(std::fabs(stats10.p95_us - 10.0) < 1e-6);
    assert(std::fabs(stats10.p99_us - 10.0) < 1e-6);

    // Single sample:
    std::vector<double> samples_1 = {42.5};
    auto stats1 = LatencyStats::calculate(samples_1);
    assert(stats1.count == 1);
    assert(std::fabs(stats1.p50_us - 42.5) < 1e-6);
    assert(std::fabs(stats1.p95_us - 42.5) < 1e-6);
    assert(std::fabs(stats1.p99_us - 42.5) < 1e-6);

    // 100 samples: 1..100
    std::vector<double> samples_100;
    for (int i = 1; i <= 100; ++i) {
        samples_100.push_back(static_cast<double>(i));
    }
    auto stats100 = LatencyStats::calculate(samples_100);
    assert(stats100.count == 100);
    // p50: ceil(50.0) - 1 = 49 -> index 49 is value 50.0
    assert(std::fabs(stats100.p50_us - 50.0) < 1e-6);
    // p95: ceil(95.0) - 1 = 94 -> index 94 is value 95.0
    assert(std::fabs(stats100.p95_us - 95.0) < 1e-6);
    // p99: ceil(99.0) - 1 = 98 -> index 98 is value 99.0
    assert(std::fabs(stats100.p99_us - 99.0) < 1e-6);

    // Empty samples throws runtime_error
    bool caught = false;
    try {
        std::vector<double> empty;
        LatencyStats::calculate(empty);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);

    std::cout << "[test_latency_quantiles] PASSED" << std::endl;
}

void test_sample_capacity_rejection() {
    std::vector<OperationRecord> records;
    records.reserve(2);
    appendMeasurementSample(records, OperationRecord{100, 120});
    appendMeasurementSample(records, OperationRecord{150, 190});
    const auto capacity = records.capacity();
    bool rejected = false;
    try {
        appendMeasurementSample(records, OperationRecord{199, 210});
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
    assert(records.size() == 2);
    assert(records.capacity() == capacity);
}

void test_serial_pair_actual_submit_and_partial_completion() {
    MeasurementWindow window(100, 200);
    IpcCommand render{};
    render.type = IpcCommand::CMD_RENDER_DONE;
    render.start_ns = 110;
    render.timestamp_ns = 130;
    render.iteration = 1;
    IpcCommand compute{};
    compute.type = IpcCommand::CMD_COMPUTE_DONE;
    compute.start_ns = 140;
    compute.timestamp_ns = 170;
    compute.iteration = 1;
    const auto pair = completedSerialPair(window, render, compute, 1);
    assert(pair && pair->submit_ns == 110 && pair->completion_ns == 170);
    compute.start_ns = 0;
    compute.timestamp_ns = 200;
    compute.iteration = 0;
    assert(!completedSerialPair(window, render, compute, 1));
    compute.start_ns = 140;
    compute.timestamp_ns = 170;
    compute.iteration = 2;
    bool rejected = false;
    try {
        completedSerialPair(window, render, compute, 1);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

int main() {
    std::cout << "Running test_measurement_window..." << std::endl;
    test_window_decisions();
    test_invalid_window();
    test_latency_quantiles();
    test_sample_capacity_rejection();
    test_serial_pair_actual_submit_and_partial_completion();
    std::cout << "All test_measurement_window checks passed successfully." << std::endl;
    return 0;
}
