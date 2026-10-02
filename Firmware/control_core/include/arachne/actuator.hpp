#pragma once

#include "arachne/interfaces.hpp"

namespace arachne {
enum class ActuatorState { Disabled, SimulationEnabled, Fault };
enum class CommandResult {
    Accepted, Disabled, FaultLatched, PhysicalOutputLocked, InvalidConfiguration,
    InvalidJoint, InvalidTarget, OutOfRange, ClockUnavailable, ClockReversed, OutputFailure
};

// Single-owner/single-thread interface. No background task or automatic motion.
// Owns an immutable snapshot: callers cannot mutate calibration after validation.
class ActuatorSystem {
public:
    ActuatorSystem(const RobotConfiguration& config, ServoOutput& output, const Clock& clock);
    ActuatorSystem(const ActuatorSystem&) = delete;
    ActuatorSystem& operator=(const ActuatorSystem&) = delete;
    void start();
    CommandResult enable_simulation();
    void disable();
    CommandResult command(JointId joint, double angle_rad);
    ActuatorState state() const { return state_; }
    const ValidationReport& validation() const { return report_; }

private:
    CommandResult fault(CommandResult reason);
    const RobotConfiguration config_;
    ServoOutput& output_;
    const Clock& clock_;
    const ValidationReport report_;
    ActuatorState state_{ActuatorState::Disabled};
    std::optional<std::uint64_t> last_time_;
};
} // namespace arachne
