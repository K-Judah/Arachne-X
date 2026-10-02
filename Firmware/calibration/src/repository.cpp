#include "arachne/calibration/repository.hpp"

namespace arachne::calibration {
Repository::Repository(Storage& storage, ActuatorSystem& safety, Revisions expected, LoadPolicy policy)
    : storage_(storage), safety_(safety), expected_(expected), policy_(policy) { safety_.disable(); }
void Repository::disarm_and_clear() { safety_.disable(); snapshot_.reset(); }
Report Repository::reject(Report report) {
    disarm_and_clear(); validated_.reset(); state_ = Lifecycle::Rejected;
    return report;
}
Report Repository::check(const Blob& blob, Record& record) const {
    auto report = decode(blob.bytes.data(), blob.size, record);
    if (!report.ok()) return report;
    const auto& r = record.revisions;
    if (r.configuration != expected_.configuration || r.geometry != expected_.geometry || r.hardware != expected_.hardware)
        report.error = Error::RevisionMismatch;
    else if (record.provenance == Provenance::Unapproved) report.error = Error::Unapproved;
    else if (record.provenance == Provenance::SyntheticTest) {
#ifdef ESP_PLATFORM
        report.error = Error::SyntheticForbidden;
#else
        if (policy_ != LoadPolicy::AllowSyntheticForHostTests) report.error = Error::SyntheticForbidden;
#endif
    }
    return report;
}
Report Repository::stage(const Record& record) {
    disarm_and_clear(); validated_.reset(); interrupted_ = false;
    state_ = Lifecycle::Candidate;
    Blob blob;
    auto report = encode(record, blob);
    if (!report.ok()) return reject(report);
    // Persisting a well-formed unapproved candidate is allowed; publication is not.
    if (!storage_.stage(blob)) { interrupted_ = storage_.has_candidate(); return reject({Error::StorageWrite}); }
    return report;
}
Report Repository::validate_candidate() {
    disarm_and_clear();
    if (state_ != Lifecycle::Candidate) return reject({Error::WrongLifecycle});
    Blob blob;
    const auto read = storage_.read_candidate(blob);
    if (read != ReadStatus::Ok) return reject({read == ReadStatus::Absent ? Error::Absent : Error::StorageRead});
    if (blob.size != kRecordSize) { interrupted_ = true; return reject({Error::Interrupted}); }
    Record record;
    auto report = check(blob, record);
    if (!report.ok()) return reject(report);
    validated_ = blob;
    state_ = Lifecycle::Validated;
    return report;
}
Report Repository::commit() {
    disarm_and_clear();
    if (state_ != Lifecycle::Validated || !validated_) return reject({Error::WrongLifecycle});
    Blob candidate;
    if (storage_.read_candidate(candidate) != ReadStatus::Ok) return reject({Error::StorageRead});
    if (candidate.size != validated_->size || candidate.bytes != validated_->bytes) return reject({Error::CandidateChanged});
    Record record;
    auto report = check(candidate, record);
    if (!report.ok()) return reject(report);
    if (!storage_.commit(candidate)) return reject({Error::CommitFailed});
    // Re-read committed bytes: a storage fault must never produce a snapshot.
    return load();
}
Report Repository::load() {
    disarm_and_clear(); validated_.reset();
    interrupted_ = storage_.has_candidate(); // Never promotes an abandoned candidate.
    Blob committed;
    const auto read = storage_.read_committed(committed);
    if (read == ReadStatus::Absent) { state_ = Lifecycle::Absent; return {Error::Absent}; }
    if (read != ReadStatus::Ok) return reject({Error::StorageRead});
    Record record;
    auto report = check(committed, record);
    if (!report.ok()) return reject(report);
    snapshot_ = Snapshot(record);
    state_ = Lifecycle::Committed;
    return report;
}
void Repository::invalidate(Revisions expected) {
    disarm_and_clear(); validated_.reset(); expected_ = expected;
    interrupted_ = storage_.has_candidate(); state_ = Lifecycle::Absent;
}
} // namespace arachne::calibration
