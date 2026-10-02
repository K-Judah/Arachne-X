#include "arachne/actuator.hpp"
#include "support/fakes.hpp"
#include "support/test_runner.hpp"

using namespace arachne;
using namespace arachne::test;
extern "C" void app_main();

TEST(locked_build_rejects_even_complete_fake_configuration) {
    FakeClock clock;
    FakeOutput output;
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.validation().ok());
    controller.start();
    CHECK(controller.enable_simulation() == CommandResult::PhysicalOutputLocked);
    CHECK(controller.state() == ActuatorState::Disabled);
    for (const auto id : kAllJoints) CHECK(controller.command(id, 0.2) == CommandResult::Disabled);
    CHECK(output.attempts == 0);
}
TEST(production_defaults_stay_unconfigured_and_disabled) {
    FakeClock clock;
    FakeOutput output;
    ActuatorSystem controller(unconfigured_robot(), output, clock);
    CHECK(!controller.validation().ok());
    CHECK(controller.enable_simulation() == CommandResult::PhysicalOutputLocked);
    CHECK(output.attempts == 0);
}
TEST(disabled_backend_never_accepts_a_direct_write) {
    DisabledOutput output;
    CHECK(output.domain() == OutputDomain::Unavailable);
    CHECK(!output.write({JointId::LF_Coxa, ControllerId::A, 0, 350.0, 0}));
}
TEST(esp32_entry_point_runs_on_host_without_hardware) {
    app_main();
    app_main();
}

int main() { return tests::run(); }
