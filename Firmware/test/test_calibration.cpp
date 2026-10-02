#include "support/calibration_fixture.hpp"
#include <algorithm>
#include <limits>
#include <random>

using namespace arachne;
using namespace arachne::test;
using namespace arachne::calibration;

TEST(calibration_defaults_are_unresolved_not_production_numbers) {
    Record record;
    CHECK(record.provenance == Provenance::Unapproved);
    CHECK(!record.revisions.configuration && !record.revisions.geometry && !record.revisions.hardware);
    CHECK(!record.timestamp_unix_seconds && !record.method_id && !record.approval_id);
    CHECK(!validate(record.configuration).ok());
    Blob blob;
    CHECK(encode(record, blob).error == Error::Metadata);
    CHECK(blob.size == 0);
}
TEST(calibration_roundtrip_all_eighteen_joints_and_fields) {
    auto record = synthetic_record();
    record.configuration.joints[17]->calibration.direction = Direction::Inverted;
    record.timestamp_unix_seconds = 123456789;
    record.method_id = 23;
    const auto blob = encoded(record);
    Record restored;
    const auto report = decode(blob.bytes.data(), blob.size, restored);
    CHECK(report.ok() && report.format_valid && report.configuration_valid);
    CHECK(restored.timestamp_unix_seconds == record.timestamp_unix_seconds);
    CHECK(restored.method_id == 23u && !restored.approval_id);
    CHECK(restored.provenance == Provenance::SyntheticTest);
    const auto again = encoded(restored);
    CHECK(again.size == blob.size && again.bytes == blob.bytes);
    for (std::size_t i = 0; i < kJointCount; ++i) {
        CHECK(restored.configuration.joints[i]->joint == kAllJoints[i]);
        NEAR(*restored.configuration.joints[i]->calibration.neutral_rad, 0.2);
    }
}
TEST(calibration_serialization_is_canonical_despite_slot_order) {
    auto record = synthetic_record();
    auto first = encoded(record);
    std::reverse(record.configuration.joints.begin(), record.configuration.joints.end());
    CHECK(encoded(record).bytes == first.bytes);
    CHECK(encoded(record).bytes == encoded(record).bytes);
    CHECK(first.size == 998);
}
TEST(calibration_crc32_standard_reference) {
    const std::string text = "123456789";
    CHECK(crc32(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()) == 0xcbf43926u);
}
TEST(calibration_every_single_bit_corruption_is_rejected_without_partial_output) {
    const auto good = encoded();
    auto output = synthetic_record(); output.revisions.configuration = 999;
    for (std::size_t i = 0; i < good.size; ++i) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            auto bad = good; bad.bytes[i] ^= static_cast<std::uint8_t>(1u << bit);
            CHECK(decode(bad.bytes.data(), bad.size, output).error == Error::Integrity);
            CHECK(output.revisions.configuration == 999);
        }
    }
}
TEST(calibration_every_truncation_extra_bytes_and_null_are_rejected) {
    const auto good = encoded(); Record result;
    for (std::size_t size = 0; size < good.size; ++size)
        CHECK(decode(good.bytes.data(), size, result).error == Error::Length);
    CHECK(decode(good.bytes.data(), good.size + 1, result).error == Error::Length);
    CHECK(decode(nullptr, good.size, result).error == Error::Length);
}
TEST(calibration_unsupported_schema_and_wrong_magic_with_valid_crc) {
    auto blob = encoded(); Record result;
    blob.bytes[4] = 2; repair_crc(blob);
    CHECK(decode(blob.bytes.data(), blob.size, result).error == Error::Version);
    blob = encoded(); blob.bytes[0] = 'Z'; repair_crc(blob);
    CHECK(decode(blob.bytes.data(), blob.size, result).error == Error::Magic);
}
TEST(calibration_missing_joint_and_incomplete_values_reject_serialization) {
    auto record = synthetic_record(); record.configuration.joints[0].reset();
    Blob destination = encoded(); const auto before = destination.bytes;
    auto report = encode(record, destination);
    CHECK(report.configuration_diagnostics.contains(ConfigError::MissingJoint));
    CHECK(destination.bytes == before);
    record = synthetic_record(); record.configuration.joints[0]->calibration.neutral_pulse_us.reset();
    CHECK(encode(record, destination).configuration_diagnostics.contains(ConfigError::MissingPulse));
}
TEST(calibration_duplicate_and_missing_joint_detected_on_load) {
    auto blob = encoded(); Record result;
    blob.bytes[kHeaderSize + kJointSize] = 0; repair_crc(blob);
    const auto report = decode(blob.bytes.data(), blob.size, result);
    CHECK(report.format_valid && !report.configuration_valid);
    CHECK(report.configuration_diagnostics.contains(ConfigError::DuplicateJoint));
    CHECK(report.configuration_diagnostics.contains(ConfigError::MissingJoint));
}
TEST(calibration_duplicate_channels_rejected_on_load) {
    auto blob = encoded(); Record result;
    blob.bytes[kHeaderSize + kJointSize + 2] = 0; repair_crc(blob);
    CHECK(decode(blob.bytes.data(), blob.size, result).configuration_diagnostics.contains(ConfigError::DuplicateChannel));
}
TEST(calibration_bad_identifiers_and_calibration_state_rejected_on_load) {
    const std::array<ConfigError, 5> expected{ConfigError::InvalidJoint, ConfigError::InvalidController,
        ConfigError::InvalidChannel, ConfigError::InvalidDirection, ConfigError::InvalidCalibrationState};
    for (std::size_t offset = 0; offset < expected.size(); ++offset) {
        auto blob = encoded(); Record result;
        blob.bytes[kHeaderSize + offset] = 255; repair_crc(blob);
        CHECK(decode(blob.bytes.data(), blob.size, result).configuration_diagnostics.contains(expected[offset]));
    }
}
TEST(calibration_invalid_ranges_and_neutral_rejected_on_load) {
    // Copy the encoded maximum onto minimum/neutral; valid CRC cannot bless bad values.
    for (const auto field : {0, 1, 3, 4}) {
        auto blob = encoded(); Record result;
        const auto base = kHeaderSize + 5;
        const auto high = field < 3 ? 2 : 5;
        std::copy_n(blob.bytes.begin() + base + high * 8, 8, blob.bytes.begin() + base + field * 8);
        repair_crc(blob);
        CHECK(decode(blob.bytes.data(), blob.size, result).error == Error::InvalidConfiguration);
    }
}
TEST(calibration_nonfinite_and_uncalibrated_values_are_rejected) {
    auto record = synthetic_record(); Blob blob;
    record.configuration.joints[0]->calibration.min_rad = std::numeric_limits<double>::quiet_NaN();
    CHECK(encode(record, blob).configuration_diagnostics.contains(ConfigError::NonFiniteAngle));
    blob = encoded(); blob.bytes[kHeaderSize + 4] = 0; repair_crc(blob); Record result;
    CHECK(decode(blob.bytes.data(), blob.size, result).configuration_diagnostics.contains(ConfigError::Uncalibrated));
}
TEST(calibration_metadata_reserved_flags_count_and_absent_timestamp_checked) {
    for (const auto offset : {20, 21, 22, 23, 24}) {
        auto blob = encoded(); Record result;
        blob.bytes[static_cast<std::size_t>(offset)] = 255; repair_crc(blob);
        CHECK(!decode(blob.bytes.data(), blob.size, result).ok());
    }
    auto record = synthetic_record(); record.timestamp_unix_seconds = 0;
    auto blob = encoded(record); Record result;
    CHECK(decode(blob.bytes.data(), blob.size, result).ok());
    CHECK(result.timestamp_unix_seconds && *result.timestamp_unix_seconds == 0);
}
TEST(calibration_all_revision_mismatches_reject_and_disarm) {
    for (const auto member : {&Revisions::configuration, &Revisions::geometry, &Revisions::hardware}) {
        CalibrationFixture f; f.save();
        auto expected = test_revisions(); expected.*member += 1;
        CHECK(f.safety.enable_simulation() == CommandResult::Accepted);
        f.repository.invalidate(expected);
        CHECK(!f.repository.snapshot());
        CHECK(f.safety.state() == ActuatorState::Disabled);
        CHECK(f.repository.load().error == Error::RevisionMismatch);
        CHECK(!f.repository.snapshot() && f.repository.state() == Lifecycle::Rejected);
    }
}
TEST(calibration_absent_storage_is_explicit_and_safe) {
    CalibrationFixture f;
    CHECK(f.repository.state() == Lifecycle::Absent);
    CHECK(f.repository.load().error == Error::Absent);
    CHECK(!f.repository.snapshot() && f.output.attempts == 0);
}
TEST(calibration_candidate_validated_committed_lifecycle_is_explicit) {
    CalibrationFixture f;
    CHECK(f.repository.stage(synthetic_record()).ok());
    CHECK(f.repository.state() == Lifecycle::Candidate && !f.repository.snapshot());
    Blob ignored; CHECK(f.storage.read_committed(ignored) == ReadStatus::Absent);
    CHECK(f.repository.validate_candidate().ok());
    CHECK(f.repository.state() == Lifecycle::Validated && !f.repository.snapshot());
    CHECK(f.storage.read_committed(ignored) == ReadStatus::Absent);
    CHECK(f.repository.commit().ok());
    CHECK(f.repository.state() == Lifecycle::Committed && f.repository.snapshot());
    CHECK(f.safety.state() == ActuatorState::Disabled && f.output.attempts == 0);
}
TEST(calibration_commit_cannot_skip_validation) {
    CalibrationFixture f;
    CHECK(f.repository.stage(synthetic_record()).ok());
    CHECK(f.repository.commit().error == Error::WrongLifecycle);
    CHECK(!f.repository.snapshot());
    CHECK(f.repository.load().error == Error::Absent);
}
TEST(calibration_unapproved_data_can_be_formatted_but_not_activated) {
    CalibrationFixture f; auto record = synthetic_record(); record.provenance = Provenance::Unapproved;
    auto blob = encoded(record); Record decoded;
    const auto report = decode(blob.bytes.data(), blob.size, decoded);
    CHECK(report.ok() && report.format_valid && report.configuration_valid);
    CHECK(f.repository.stage(record).ok());
    CHECK(f.repository.validate_candidate().error == Error::Unapproved);
    CHECK(!f.repository.snapshot());
}
TEST(calibration_synthetic_data_requires_explicit_host_policy) {
    CalibrationFixture f; f.save();
    Repository production(f.storage, f.safety, test_revisions());
    CHECK(production.load().error == Error::SyntheticForbidden);
    CHECK(!production.snapshot());
    CHECK(f.safety.state() == ActuatorState::Disabled);
}
TEST(calibration_physical_claim_requires_external_method_and_approval_ids) {
    auto record = synthetic_record(); record.provenance = Provenance::PhysicalApprovalClaim;
    Blob blob;
    CHECK(encode(record, blob).error == Error::Metadata);
    record.method_id = 31;
    CHECK(encode(record, blob).error == Error::Metadata);
    record.approval_id = 37; // Simulated external evidence IDs, NOT real approval.
    CHECK(encode(record, blob).ok());
    CalibrationFixture f;
    Repository repository(f.storage, f.safety, test_revisions());
    CHECK(repository.stage(record).ok()); CHECK(repository.validate_candidate().ok()); CHECK(repository.commit().ok());
    CHECK(f.output.attempts == 0 && f.safety.state() == ActuatorState::Disabled);
}
TEST(calibration_invalid_candidate_never_replaces_previous_commit) {
    CalibrationFixture f; f.save();
    auto bad = synthetic_record(); bad.configuration.joints[17]->calibration.neutral_rad = 100;
    CHECK(!f.repository.stage(bad).ok());
    CHECK(!f.repository.snapshot() && f.repository.state() == Lifecycle::Rejected);
    CHECK(f.repository.load().ok());
    NEAR(*f.repository.snapshot()->configuration().joints[17]->calibration.neutral_rad, 0.2);
}
TEST(calibration_every_interrupted_write_preserves_last_known_good) {
    CalibrationFixture f; f.save();
    auto replacement = synthetic_record(); replacement.configuration.joints[0]->calibration.neutral_rad = 0.3;
    for (std::size_t cut = 0; cut < kRecordSize; ++cut) {
        f.storage.interrupt_after_bytes = cut;
        CHECK(f.repository.stage(replacement).error == Error::StorageWrite);
        Repository restarted(f.storage, f.safety, test_revisions(), LoadPolicy::AllowSyntheticForHostTests);
        CHECK(restarted.load().ok());
        CHECK(restarted.interrupted_candidate());
        NEAR(*restarted.snapshot()->configuration().joints[0]->calibration.neutral_rad, 0.2);
    }
    CHECK(f.output.attempts == 0);
}
TEST(calibration_failed_commit_preserves_previous_record_and_recovers) {
    CalibrationFixture f; f.save(); auto replacement = synthetic_record();
    replacement.configuration.joints[0]->calibration.neutral_rad = 0.3;
    CHECK(f.repository.stage(replacement).ok()); CHECK(f.repository.validate_candidate().ok());
    f.storage.fail_commit = true;
    CHECK(f.repository.commit().error == Error::CommitFailed);
    CHECK(!f.repository.snapshot());
    CHECK(f.repository.load().ok());
    NEAR(*f.repository.snapshot()->configuration().joints[0]->calibration.neutral_rad, 0.2);
    CHECK(f.repository.interrupted_candidate());
}
TEST(calibration_candidate_modification_after_validation_is_rejected) {
    CalibrationFixture f; f.save(); CHECK(f.repository.stage(synthetic_record()).ok());
    CHECK(f.repository.validate_candidate().ok()); f.storage.corrupt_candidate(60);
    CHECK(f.repository.commit().error == Error::CandidateChanged);
    CHECK(f.repository.load().ok());
}
TEST(calibration_complete_replacement_is_atomic_and_snapshot_is_immutable) {
    CalibrationFixture f; f.save(); const auto previous = *f.repository.snapshot();
    auto replacement = synthetic_record();
    for (auto& slot : replacement.configuration.joints) slot->calibration.neutral_rad = 0.3;
    CHECK(f.repository.stage(replacement).ok()); CHECK(f.repository.validate_candidate().ok());
    Blob old; CHECK(f.storage.read_committed(old) == ReadStatus::Ok); CHECK(old.bytes == encoded().bytes);
    CHECK(f.repository.commit().ok());
    replacement.configuration.joints[0]->calibration.neutral_rad = 0.4;
    for (std::size_t i = 0; i < kJointCount; ++i) {
        NEAR(*previous.configuration().joints[i]->calibration.neutral_rad, 0.2);
        NEAR(*f.repository.snapshot()->configuration().joints[i]->calibration.neutral_rad, 0.3);
    }
    // Loading never mutates the existing actuator's immutable snapshot.
    NEAR(*f.safety.configuration().joints[0]->calibration.neutral_rad, 0.2);
    ActuatorSystem restored(f.repository.snapshot()->configuration(), f.output, f.clock);
    CHECK(restored.validation().ok() && restored.state() == ActuatorState::Disabled);
    CHECK(f.output.attempts == 0);
}
TEST(calibration_corrupt_committed_data_disarms_and_exposes_no_configuration) {
    CalibrationFixture f; f.save(); CHECK(f.safety.enable_simulation() == CommandResult::Accepted);
    f.storage.corrupt_committed(60);
    CHECK(f.repository.load().error == Error::Integrity);
    CHECK(!f.repository.snapshot() && f.safety.state() == ActuatorState::Disabled);
    CHECK(f.safety.command(JointId::LF_Coxa, 0.2) == CommandResult::Disabled);
    CHECK(f.output.attempts == 0);
}
TEST(calibration_storage_read_failure_disarms) {
    CalibrationFixture f; f.save(); CHECK(f.safety.enable_simulation() == CommandResult::Accepted);
    f.storage.fail_read = true;
    CHECK(f.repository.load().error == Error::StorageRead);
    CHECK(!f.repository.snapshot() && f.safety.state() == ActuatorState::Disabled);
}
TEST(calibration_estop_latch_survives_all_lifecycle_operations) {
    CalibrationFixture f; f.safety.emergency_stop(); f.save();
    CHECK(f.repository.load().ok()); f.repository.invalidate(test_revisions());
    CHECK(f.safety.state() == ActuatorState::Fault);
    CHECK(f.safety.enable_simulation() == CommandResult::FaultLatched);
    CHECK(f.output.attempts == 0);
}
TEST(calibration_reboot_does_not_promote_validated_but_uncommitted_data) {
    CalibrationFixture f; CHECK(f.repository.stage(synthetic_record()).ok());
    CHECK(f.repository.validate_candidate().ok());
    Repository restarted(f.storage, f.safety, test_revisions(), LoadPolicy::AllowSyntheticForHostTests);
    CHECK(restarted.load().error == Error::Absent && restarted.interrupted_candidate());
    CHECK(!restarted.snapshot());
}
TEST(calibration_unknown_expected_revisions_fail_closed) {
    CalibrationFixture f; f.save(); f.repository.invalidate({});
    CHECK(f.repository.load().error == Error::RevisionMismatch);
    CHECK(!f.repository.snapshot());
}
TEST(calibration_random_bytes_and_valid_crc_mutations_do_not_crash) {
    std::mt19937 random(0x43414c31);
    const auto good = encoded();
    for (unsigned n = 0; n < 2000; ++n) {
        Blob input;
        input.size = random() % (kRecordSize + 1);
        for (auto& byte : input.bytes) byte = static_cast<std::uint8_t>(random());
        Record result;
        CHECK(!decode(input.bytes.data(), input.size, result).ok());
        input = good;
        const auto offset = static_cast<std::size_t>(random() % (kRecordSize - 4));
        input.bytes[offset] ^= static_cast<std::uint8_t>(random()); repair_crc(input);
        const auto report = decode(input.bytes.data(), input.size, result);
        if (report.ok()) CHECK(validate(result.configuration).ok());
        else CHECK(!result.revisions.configuration); // rejected output remains untouched
    }
}
int main() { return tests::run(); }
