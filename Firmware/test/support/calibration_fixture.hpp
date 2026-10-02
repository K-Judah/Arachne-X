#pragma once
#include "arachne/calibration/repository.hpp"
#include "arachne/calibration/memory_storage.hpp"
#include "fakes.hpp"
#include "test_runner.hpp"

namespace arachne::test {
namespace cal = arachne::calibration;
// All IDs, timestamps, geometry/hardware revisions and numbers here are SYNTHETIC.
inline cal::Revisions test_revisions() { return {7, 11, 13}; }
inline cal::Record synthetic_record() {
    cal::Record record;
    record.revisions = test_revisions();
    record.provenance = cal::Provenance::SyntheticTest;
    record.configuration = synthetic_config();
    return record;
}
struct CalibrationFixture {
    FakeClock clock;
    FakeOutput output;
    ActuatorSystem safety{synthetic_config(), output, clock};
    cal::MemoryStorage storage;
    cal::Repository repository{storage, safety, test_revisions(), cal::LoadPolicy::AllowSyntheticForHostTests};
    void save(const cal::Record& record = synthetic_record()) {
        CHECK(repository.stage(record).ok());
        CHECK(repository.validate_candidate().ok());
        CHECK(repository.commit().ok());
    }
};
inline cal::Blob encoded(const cal::Record& record = synthetic_record()) {
    cal::Blob blob;
    CHECK(cal::encode(record, blob).ok());
    return blob;
}
inline void repair_crc(cal::Blob& blob) {
    const auto crc = cal::crc32(blob.bytes.data(), cal::kRecordSize - 4);
    for (unsigned i = 0; i < 4; ++i)
        blob.bytes[cal::kRecordSize - 4 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
}
} // namespace arachne::test
