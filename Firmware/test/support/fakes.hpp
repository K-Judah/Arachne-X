#pragma once
#ifdef ESP_PLATFORM
#error "Test fakes and synthetic calibration must never be included in firmware"
#endif

#include "arachne/interfaces.hpp"
#include <vector>

namespace arachne::test {
class FakeClock final : public Clock {
public:
    std::optional<std::uint64_t> time{0};
    std::optional<std::uint64_t> now_us() const override { return time; }
};
class FakeOutput final : public ServoOutput {
public:
    OutputDomain output_domain{OutputDomain::Simulation};
    bool succeed{true};
    std::size_t attempts{0};
    std::vector<PulseCommand> commands;
    OutputDomain domain() const override { return output_domain; }
    bool write(const PulseCommand& command) override {
        ++attempts;
        if (!succeed) return false;
        commands.push_back(command);
        return true;
    }
};

// SYNTHETIC arithmetic fixture, NOT MG996R limits, wiring, or calibration.
// Deliberately asymmetric to expose neutral-offset and inversion bugs.
inline RobotConfiguration synthetic_config() {
    auto config = unconfigured_robot();
    for (std::size_t i = 0; i < kJointCount; ++i) {
        auto& entry = *config.joints[i];
        entry.wiring = {i < 9 ? ControllerId::A : ControllerId::B, static_cast<int>(i % 9)};
        entry.calibration = {CalibrationState::Calibrated, Direction::Normal,
                             -0.8, 0.2, 1.6, 200.0, 350.0, 800.0};
    }
    return config;
}
} // namespace arachne::test
