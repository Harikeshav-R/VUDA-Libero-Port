#pragma once

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "Platform must be little-endian for exp9ts01_le");
#endif

#pragma pack(push, 1)

// Raw binary timestamp pair record: struct '<QQ' (little endian, packed 16 bytes)
struct OperationRecord {
    uint64_t submit_ns;
    uint64_t completion_ns;
};
static_assert(sizeof(OperationRecord) == 16, "OperationRecord must be exactly 16 bytes");

// 32-byte header: struct '<8sQQQ' (little endian, packed 32 bytes)
// magic: b'EXP9TS01'
struct TraceFileHeader {
    char magic[8];         // b'EXP9TS01'
    uint64_t start_ns;     // 8 bytes LE
    uint64_t deadline_ns;  // 8 bytes LE
    uint64_t record_count; // 8 bytes LE
};
static_assert(sizeof(TraceFileHeader) == 32, "TraceFileHeader must be exactly 32 bytes");

#pragma pack(pop)

template <typename T>
inline void appendMeasurementSample(std::vector<T>& samples,
                                    const typename std::vector<T>::value_type& sample) {
    if (samples.size() >= samples.capacity()) {
        throw std::runtime_error("Measurement sample capacity exhausted");
    }
    samples.push_back(sample);
}

inline void dumpOperationTrace(const std::string& filepath,
                              uint64_t start_ns,
                              uint64_t deadline_ns,
                              const std::vector<OperationRecord>& records) {
    if (filepath.empty()) return;

    TraceFileHeader hdr;
    std::memcpy(hdr.magic, "EXP9TS01", 8);
    hdr.start_ns = start_ns;
    hdr.deadline_ns = deadline_ns;
    hdr.record_count = records.size();

    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("dumpOperationTrace: failed to open trace file for writing: " + filepath);
    }

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (!records.empty()) {
        out.write(reinterpret_cast<const char*>(records.data()), records.size() * sizeof(OperationRecord));
    }
    out.close();
    if (!out.good()) {
        throw std::runtime_error("dumpOperationTrace: failed writing trace data: " + filepath);
    }
}
