#include "arachne/calibration/record.hpp"
#include <cstring>
#include <limits>

namespace arachne::calibration {
namespace {
void put(std::uint8_t* p, std::uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i) p[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::uint64_t get(const std::uint8_t* p, unsigned size) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < size; ++i) value |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    return value;
}
void put_angle(std::uint8_t* p, double value) {
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
    std::uint64_t bits;
    std::memcpy(&bits, &value, 8);
    put(p, bits, 8);
}
double get_angle(const std::uint8_t* p) {
    const auto bits = get(p, 8);
    double value;
    std::memcpy(&value, &bits, 8);
    return value;
}
constexpr std::array<std::optional<double> Calibration::*, 6> fields{
    &Calibration::min_rad, &Calibration::neutral_rad, &Calibration::max_rad,
    &Calibration::min_pulse_us, &Calibration::neutral_pulse_us, &Calibration::max_pulse_us
};
bool metadata_valid(const Record& record) {
    const auto& r = record.revisions;
    if (!r.configuration || !r.geometry || !r.hardware ||
        (record.method_id && !*record.method_id) || (record.approval_id && !*record.approval_id)) return false;
    switch (record.provenance) {
    case Provenance::Unapproved:
    case Provenance::SyntheticTest: return !record.approval_id;
    case Provenance::PhysicalApprovalClaim: return record.approval_id && record.method_id;
    }
    return false;
}
Report inspect(const Record& record) {
    Report report;
    if (!metadata_valid(record)) { report.error = Error::Metadata; return report; }
    report.format_valid = true;
    report.configuration_diagnostics = validate(record.configuration);
    report.configuration_valid = report.configuration_diagnostics.ok();
    if (!report.configuration_valid) report.error = Error::InvalidConfiguration;
    return report;
}
} // namespace
std::uint32_t crc32(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t value = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) {
        value ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1u) ? 0xedb88320u : 0u);
    }
    return value ^ 0xffffffffu;
}
Report encode(const Record& record, Blob& output) {
    auto report = inspect(record);
    if (!report.ok()) return report;
    Blob result;
    result.size = kRecordSize;
    auto* p = result.bytes.data();
    p[0] = 'A'; p[1] = 'X'; p[2] = 'C'; p[3] = 'R';
    put(p + 4, kSchemaVersion, 2); put(p + 6, kRecordSize, 2);
    put(p + 8, record.revisions.configuration, 4);
    put(p + 12, record.revisions.geometry, 4);
    put(p + 16, record.revisions.hardware, 4);
    p[20] = record.timestamp_unix_seconds ? 1 : 0;
    p[21] = static_cast<std::uint8_t>(record.provenance);
    p[22] = static_cast<std::uint8_t>(kJointCount);
    put(p + 24, record.timestamp_unix_seconds.value_or(0), 8);
    put(p + 32, record.method_id.value_or(0), 4);
    put(p + 36, record.approval_id.value_or(0), 4);
    // Canonical order by identity, independent of caller's configuration slots.
    for (const auto& slot : record.configuration.joints) {
        const auto& joint = *slot;
        auto* row = p + kHeaderSize + static_cast<std::size_t>(joint.joint) * kJointSize;
        row[0] = static_cast<std::uint8_t>(joint.joint);
        row[1] = static_cast<std::uint8_t>(*joint.wiring.controller);
        row[2] = static_cast<std::uint8_t>(*joint.wiring.channel);
        row[3] = static_cast<std::uint8_t>(*joint.calibration.direction);
        row[4] = static_cast<std::uint8_t>(joint.calibration.state);
        for (std::size_t i = 0; i < fields.size(); ++i)
            put_angle(row + 5 + 8 * i, *(joint.calibration.*fields[i]));
    }
    put(p + kRecordSize - 4, crc32(p, kRecordSize - 4), 4);
    output = result;
    return report;
}
Report decode(const std::uint8_t* bytes, std::size_t size, Record& output) {
    if (!bytes || size != kRecordSize) return {Error::Length};
    if (crc32(bytes, size - 4) != get(bytes + size - 4, 4)) return {Error::Integrity};
    if (bytes[0] != 'A' || bytes[1] != 'X' || bytes[2] != 'C' || bytes[3] != 'R') return {Error::Magic};
    if (get(bytes + 4, 2) != kSchemaVersion) return {Error::Version};
    if (get(bytes + 6, 2) != size || bytes[22] != kJointCount) return {Error::Length};
    if (bytes[20] > 1 || bytes[23] != 0 || (!bytes[20] && get(bytes + 24, 8))) return {Error::Metadata};
    Record result;
    result.revisions = {static_cast<std::uint32_t>(get(bytes + 8, 4)),
                        static_cast<std::uint32_t>(get(bytes + 12, 4)),
                        static_cast<std::uint32_t>(get(bytes + 16, 4))};
    if (bytes[20]) result.timestamp_unix_seconds = get(bytes + 24, 8);
    result.provenance = static_cast<Provenance>(bytes[21]);
    const auto method = static_cast<std::uint32_t>(get(bytes + 32, 4));
    const auto approval = static_cast<std::uint32_t>(get(bytes + 36, 4));
    if (method) result.method_id = method;
    if (approval) result.approval_id = approval;
    for (std::size_t i = 0; i < kJointCount; ++i) {
        const auto* row = bytes + kHeaderSize + i * kJointSize;
        auto& joint = *result.configuration.joints[i];
        joint.joint = static_cast<JointId>(row[0]);
        joint.wiring = {static_cast<ControllerId>(row[1]), row[2]};
        joint.calibration.direction = static_cast<Direction>(row[3]);
        joint.calibration.state = static_cast<CalibrationState>(row[4]);
        for (std::size_t n = 0; n < fields.size(); ++n)
            joint.calibration.*fields[n] = get_angle(row + 5 + 8 * n);
    }
    auto report = inspect(result);
    if (report.ok()) output = result;
    return report;
}
const char* describe(Error error) {
    switch (error) {
    case Error::None: return "valid record";
    case Error::Length: return "record is incomplete or has an invalid size/joint count";
    case Error::Magic: return "not an Arachne-X calibration record";
    case Error::Version: return "unsupported calibration schema";
    case Error::Integrity: return "calibration CRC mismatch";
    case Error::Metadata: return "invalid or unresolved calibration metadata";
    case Error::InvalidConfiguration: return "configuration rejected; inspect joint diagnostics";
    case Error::RevisionMismatch: return "configuration, geometry or hardware revision mismatch";
    case Error::Unapproved: return "no external physical approval claim";
    case Error::SyntheticForbidden: return "synthetic calibration requires explicit host-test policy";
    case Error::Absent: return "no committed calibration";
    case Error::StorageRead: return "storage read failed";
    case Error::StorageWrite: return "candidate write failed; previous commit preserved";
    case Error::Interrupted: return "candidate write was interrupted";
    case Error::CommitFailed: return "atomic commit failed; previous commit preserved";
    case Error::CandidateChanged: return "candidate changed after validation";
    case Error::WrongLifecycle: return "operation is not valid in this lifecycle state";
    }
    return "unknown calibration error";
}
} // namespace arachne::calibration
