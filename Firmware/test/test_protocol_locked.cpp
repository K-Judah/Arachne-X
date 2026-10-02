#include "support/protocol_fixture.hpp"
using namespace arachne;
using namespace arachne::protocol;
using namespace arachne::test;

TEST(protocol_cannot_enable_locked_build_even_with_synthetic_calibration) {
    ProtocolFixture f; CHECK(f.hello().type == Type::HelloAck);
    expect_error(f.enable(), Error::PhysicalLocked);
    expect_error(f.exchange(f.target(0, 0.2)), Error::Disabled);
    CHECK(f.output.attempts == 0 && f.actuators.state() == ActuatorState::Disabled);
    CHECK(f.exchange(f.next(Type::GetStatus)).payload[2] == 0);
}
TEST(locked_build_stop_is_latched_and_disable_still_accepted) {
    ProtocolFixture f;
    CHECK(f.exchange(f.next(Type::EmergencyStop)).type == Type::Ack);
    CHECK(f.exchange(f.next(Type::Disable)).type == Type::Ack);
    expect_error(f.hello(), Error::Stopped);
    CHECK(f.actuators.state() == ActuatorState::Fault && f.output.attempts == 0);
}
int main() { return tests::run(); }
