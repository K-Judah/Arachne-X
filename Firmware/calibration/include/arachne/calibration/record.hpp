#pragma once
#include "arachne/configuration.hpp"

namespace arachne::calibration {
inline constexpr std::uint16_t kSchemaVersion = 1;
inline constexpr std::size_t kHeaderSize = 40;
inline constexpr std::size_t kJointSize = 53;
inline constexpr std::size_t kRecordSize = kHeaderSize + kJointCount * kJointSize + 4;

// Externally assigned identities; zero explicitly means unresolved, never a default revision.
struct Revisions {
    std::uint32_t configuration{0}, geometry{0}, hardware{0};
};
enum class Provenance : std::uint8_t { Unapproved = 0, SyntheticTest = 1, PhysicalApprovalClaim = 2 };
struct Record {
    Revisions revisions{};
    std::optional<std::uint64_t> timestamp_unix_seconds;
    std::optional<std::uint32_t> method_id, approval_id;
    Provenance provenance{Provenance::Unapproved};
    RobotConfiguration configuration{unconfigured_robot()};
};
struct Blob {
    std::array<std::uint8_t, kRecordSize> bytes{};
    std::size_t size{0};
};
enum class Error {
    None, Length, Magic, Version, Integrity, Metadata, InvalidConfiguration,
    RevisionMismatch, Unapproved, SyntheticForbidden, Absent, StorageRead,
    StorageWrite, Interrupted, CommitFailed, CandidateChanged, WrongLifecycle
};
struct Report {
    Error error{Error::None};
    bool format_valid{false};
    bool configuration_valid{false};
    ValidationReport configuration_diagnostics{};
    bool ok() const { return error == Error::None; }
};
// Output arguments change only on complete success. No implicit activation.
Report encode(const Record& record, Blob& output);
Report decode(const std::uint8_t* bytes, std::size_t size, Record& output);
std::uint32_t crc32(const std::uint8_t* bytes, std::size_t size);
const char* describe(Error error);
} // namespace arachne::calibration
