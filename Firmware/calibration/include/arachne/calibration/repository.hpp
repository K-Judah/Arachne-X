#pragma once
#include "arachne/actuator.hpp"
#include "arachne/calibration/storage.hpp"

namespace arachne::calibration {
enum class Lifecycle { Absent, Candidate, Validated, Committed, Rejected };
enum class LoadPolicy { RequirePhysicalApprovalClaim, AllowSyntheticForHostTests };
// Immutable through its public API. A snapshot carries evidence labels, not an
// arming capability; ActuatorSystem still copies and validates its configuration.
class Snapshot {
public:
    const Record& record() const { return record_; }
    const RobotConfiguration& configuration() const { return record_.configuration; }
private:
    explicit Snapshot(const Record& record) : record_(record) {}
    Record record_;
    friend class Repository;
};
class Repository {
public:
    Repository(Storage& storage, ActuatorSystem& safety, Revisions expected,
               LoadPolicy policy = LoadPolicy::RequirePhysicalApprovalClaim);
    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;
    Report stage(const Record& record);
    Report validate_candidate();
    Report commit();
    Report load();
    // Invalidation does not erase history. Old bytes cannot load against new revisions.
    void invalidate(Revisions expected);
    Lifecycle state() const { return state_; }
    const std::optional<Snapshot>& snapshot() const { return snapshot_; }
    bool interrupted_candidate() const { return interrupted_; }
private:
    Report check(const Blob& blob, Record& record) const;
    void disarm_and_clear();
    Report reject(Report report);
    Storage& storage_;
    ActuatorSystem& safety_;
    Revisions expected_;
    const LoadPolicy policy_;
    Lifecycle state_{Lifecycle::Absent};
    std::optional<Snapshot> snapshot_;
    std::optional<Blob> validated_;
    bool interrupted_{false};
};
} // namespace arachne::calibration
