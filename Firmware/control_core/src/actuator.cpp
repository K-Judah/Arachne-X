#include "arachne/actuator.hpp"

#include <cmath>

#if defined(ESP_PLATFORM) && defined(ARACHNE_HOST_SIMULATION)
#error "Host simulation enablement must never be compiled into ESP32 firmware"
#endif

namespace arachne {
ActuatorSystem::ActuatorSystem(const RobotConfiguration& config, ServoOutput& output,
                               const Clock& clock)
    : config_(config), output_(output), clock_(clock), report_(validate(config_)) {}

void ActuatorSystem::start() { disable(); }
void ActuatorSystem::disable() {
    // Faults cannot be cleared by start/disable/enable; construct a new session.
    if (state_ != ActuatorState::Fault) state_ = ActuatorState::Disabled;
    last_time_.reset();
}
CommandResult ActuatorSystem::enable_simulation() {
    if (state_ == ActuatorState::Fault) return CommandResult::FaultLatched;
    disable();
#if !defined(ARACHNE_HOST_SIMULATION)
    return CommandResult::PhysicalOutputLocked;
#else
    if (output_.domain() != OutputDomain::Simulation) return CommandResult::PhysicalOutputLocked;
    if (!report_.ok()) return CommandResult::InvalidConfiguration;
    const auto now = clock_.now_us();
    if (!now) return CommandResult::ClockUnavailable;
    last_time_ = now;
    state_ = ActuatorState::SimulationEnabled;
    return CommandResult::Accepted;
#endif
}
CommandResult ActuatorSystem::fault(CommandResult reason) {
    state_ = ActuatorState::Fault;
    return reason;
}
CommandResult ActuatorSystem::command(JointId joint, double angle_rad) {
    if (state_ == ActuatorState::Fault) return CommandResult::FaultLatched;
    if (state_ != ActuatorState::SimulationEnabled) return CommandResult::Disabled;
#if !defined(ARACHNE_HOST_SIMULATION)
    (void)joint;
    (void)angle_rad;
    return fault(CommandResult::PhysicalOutputLocked);
#else
    if (output_.domain() != OutputDomain::Simulation) return fault(CommandResult::PhysicalOutputLocked);
    if (!report_.ok()) return fault(CommandResult::InvalidConfiguration);
    if (!valid(joint)) return fault(CommandResult::InvalidJoint);
    if (!std::isfinite(angle_rad)) return fault(CommandResult::InvalidTarget);
    const JointConfiguration* entry = nullptr;
    for (const auto& slot : config_.joints) {
        if (slot && slot->joint == joint) { entry = &*slot; break; }
    }
    if (!entry || entry->calibration.state != CalibrationState::Calibrated)
        return fault(CommandResult::InvalidConfiguration);
    const auto& c = entry->calibration;
    if (angle_rad < *c.min_rad || angle_rad > *c.max_rad) return fault(CommandResult::OutOfRange);

    const bool normal = *c.direction == Direction::Normal;
    const double low_pulse = normal ? *c.min_pulse_us : *c.max_pulse_us;
    const double high_pulse = normal ? *c.max_pulse_us : *c.min_pulse_us;
    double pulse;
    if (angle_rad <= *c.neutral_rad) {
        const double t = (angle_rad - *c.min_rad) / (*c.neutral_rad - *c.min_rad);
        pulse = low_pulse + t * (*c.neutral_pulse_us - low_pulse);
    } else {
        const double t = (angle_rad - *c.neutral_rad) / (*c.max_rad - *c.neutral_rad);
        pulse = *c.neutral_pulse_us + t * (high_pulse - *c.neutral_pulse_us);
    }
    if (!std::isfinite(pulse) || pulse < *c.min_pulse_us || pulse > *c.max_pulse_us)
        return fault(CommandResult::InvalidTarget);
    const auto now = clock_.now_us();
    if (!now) return fault(CommandResult::ClockUnavailable);
    if (last_time_ && *now < *last_time_) return fault(CommandResult::ClockReversed);
    last_time_ = now;
    if (!output_.write({joint, *entry->wiring.controller, *entry->wiring.channel, pulse, *now}))
        return fault(CommandResult::OutputFailure);
    return CommandResult::Accepted;
#endif
}
} // namespace arachne
