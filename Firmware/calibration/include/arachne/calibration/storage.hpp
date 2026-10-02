#pragma once
#include "arachne/calibration/record.hpp"

namespace arachne::calibration {
enum class ReadStatus { Ok, Absent, Failed };
// Single owner. Implementations must preserve the last committed bytes on any
// failed stage/commit. A successful commit atomically publishes EXACT expected
// bytes or fails, never a mixture. Read failures must not expose partial output.
// This is a contract for future adapters, not a claim about any flash device.
class Storage {
public:
    virtual ~Storage() = default;
    virtual bool stage(const Blob& candidate) = 0;
    virtual ReadStatus read_candidate(Blob& output) const = 0;
    virtual bool commit(const Blob& expected) = 0;
    virtual ReadStatus read_committed(Blob& output) const = 0;
    virtual bool has_candidate() const = 0;
};
} // namespace arachne::calibration
