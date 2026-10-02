#pragma once
#ifdef ESP_PLATFORM
#error "Simulated calibration storage must not be included in ESP32 firmware"
#endif
#include "arachne/calibration/storage.hpp"
#include <algorithm>

namespace arachne::calibration {
// Test-only in-memory persistence across Repository lifetimes, not process exit.
// Fault injection never touches real storage or hardware.
class MemoryStorage final : public Storage {
public:
    std::optional<std::size_t> interrupt_after_bytes;
    bool fail_commit{false}, fail_read{false};
    bool stage(const Blob& candidate) override {
        if (candidate.size > kRecordSize) return false;
        candidate_ = Blob{};
        candidate_->size = std::min(candidate.size, interrupt_after_bytes.value_or(candidate.size));
        std::copy_n(candidate.bytes.begin(), candidate_->size, candidate_->bytes.begin());
        return candidate_->size == candidate.size;
    }
    ReadStatus read_candidate(Blob& output) const override { return read(candidate_, output); }
    ReadStatus read_committed(Blob& output) const override { return read(committed_, output); }
    bool commit(const Blob& expected) override {
        if (fail_commit || !candidate_ || expected.size > kRecordSize ||
            candidate_->size != expected.size || candidate_->bytes != expected.bytes) return false;
        committed_ = *candidate_; // single-thread atomic publication in this simulation
        candidate_.reset();
        return true;
    }
    bool has_candidate() const override { return candidate_.has_value(); }
    void corrupt_candidate(std::size_t offset) { if (candidate_ && offset < candidate_->size) candidate_->bytes[offset] ^= 1; }
    void corrupt_committed(std::size_t offset) { if (committed_ && offset < committed_->size) committed_->bytes[offset] ^= 1; }
private:
    ReadStatus read(const std::optional<Blob>& source, Blob& output) const {
        if (fail_read) return ReadStatus::Failed;
        if (!source) return ReadStatus::Absent;
        output = *source;
        return ReadStatus::Ok;
    }
    std::optional<Blob> candidate_, committed_;
};
} // namespace arachne::calibration
