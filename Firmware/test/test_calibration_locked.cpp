#include "support/calibration_fixture.hpp"
using namespace arachne;
using namespace arachne::test;
using namespace arachne::calibration;

TEST(loaded_calibration_cannot_unlock_physical_output) {
    CalibrationFixture f; f.save();
    ActuatorSystem loaded(f.repository.snapshot()->configuration(), f.output, f.clock);
    CHECK(loaded.validation().ok());
    CHECK(loaded.enable_simulation() == CommandResult::PhysicalOutputLocked);
    CHECK(loaded.command(JointId::LF_Coxa, 0.2) == CommandResult::Disabled);
    CHECK(f.output.attempts == 0);
}
TEST(approval_claim_does_not_unlock_locked_build_or_clear_estop) {
    CalibrationFixture f; auto record = synthetic_record();
    record.provenance = Provenance::PhysicalApprovalClaim;
    record.method_id = 31; record.approval_id = 37; // synthetic approval simulation only
    Repository repository(f.storage, f.safety, test_revisions());
    CHECK(repository.stage(record).ok()); CHECK(repository.validate_candidate().ok()); CHECK(repository.commit().ok());
    CHECK(f.safety.enable_simulation() == CommandResult::PhysicalOutputLocked);
    f.safety.emergency_stop(); CHECK(repository.load().ok());
    CHECK(f.safety.state() == ActuatorState::Fault && f.output.attempts == 0);
}
int main() { return tests::run(); }
