#include "arachne/configuration.hpp"

#include <cmath>

namespace arachne {
RobotConfiguration unconfigured_robot() {
    RobotConfiguration result;
    for (std::size_t i = 0; i < kJointCount; ++i) {
        result.joints[i] = JointConfiguration{kAllJoints[i]};
    }
    return result;
}

bool ValidationReport::contains(ConfigError code) const {
    for (std::size_t i = 0; i < count; ++i) {
        if (diagnostics[i].code == code) return true;
    }
    return false;
}
void ValidationReport::add(ConfigError code, std::optional<std::size_t> slot,
                           std::optional<JointId> joint) {
    const auto compact_slot = slot ? std::optional<std::uint8_t>{static_cast<std::uint8_t>(*slot)}
                                   : std::nullopt;
    if (count < diagnostics.size()) diagnostics[count++] = {code, compact_slot, joint};
    else diagnostics.back() = {ConfigError::DiagnosticOverflow, std::nullopt, std::nullopt};
}

ValidationReport validate(const RobotConfiguration& config) {
    ValidationReport result;
    std::array<bool, kJointCount> seen{};
    std::array<std::array<bool, 16>, 2> channels{}; // PCA9685 register channel count.
    for (std::size_t i = 0; i < config.joints.size(); ++i) {
        if (!config.joints[i]) continue;
        const auto& entry = *config.joints[i];
        const auto add = [&](ConfigError code) { result.add(code, i, entry.joint); };
        if (!valid(entry.joint)) add(ConfigError::InvalidJoint);
        else {
            const auto index = static_cast<std::size_t>(entry.joint);
            if (seen[index]) add(ConfigError::DuplicateJoint);
            seen[index] = true;
        }
        const auto& w = entry.wiring;
        const bool controller_ok = w.controller &&
            (*w.controller == ControllerId::A || *w.controller == ControllerId::B);
        const bool channel_ok = w.channel && *w.channel >= 0 && *w.channel < 16;
        if (!w.controller) add(ConfigError::MissingController);
        else if (!controller_ok) add(ConfigError::InvalidController);
        if (!w.channel) add(ConfigError::MissingChannel);
        else if (!channel_ok) add(ConfigError::InvalidChannel);
        if (controller_ok && channel_ok) {
            auto& occupied = channels[static_cast<std::size_t>(*w.controller)]
                                     [static_cast<std::size_t>(*w.channel)];
            if (occupied) add(ConfigError::DuplicateChannel);
            occupied = true;
        }
        const auto& c = entry.calibration;
        if (c.state == CalibrationState::Uncalibrated) add(ConfigError::Uncalibrated);
        else if (c.state != CalibrationState::Calibrated) add(ConfigError::InvalidCalibrationState);
        if (!c.direction) add(ConfigError::MissingDirection);
        else if (*c.direction != Direction::Normal && *c.direction != Direction::Inverted)
            add(ConfigError::InvalidDirection);

        const auto check_triplet = [&](const std::optional<double>& low,
                                       const std::optional<double>& neutral,
                                       const std::optional<double>& high, bool pulse) {
            if (!low || !neutral || !high) {
                add(pulse ? ConfigError::MissingPulse : ConfigError::MissingAngle);
                return;
            }
            if (!std::isfinite(*low) || !std::isfinite(*neutral) || !std::isfinite(*high)) {
                add(pulse ? ConfigError::NonFinitePulse : ConfigError::NonFiniteAngle);
                return;
            }
            // Reject overflow as well as reversed/equal endpoints.
            if (*low >= *high || !std::isfinite(*high - *low) || (pulse && *low <= 0))
                add(pulse ? ConfigError::InvalidPulseRange : ConfigError::InvalidAngleRange);
            // Strict interior is needed for the two interpolation segments.
            if (*neutral <= *low || *neutral >= *high)
                add(pulse ? ConfigError::NeutralPulseOutsideRange : ConfigError::NeutralAngleOutsideRange);
        };
        check_triplet(c.min_rad, c.neutral_rad, c.max_rad, false);
        check_triplet(c.min_pulse_us, c.neutral_pulse_us, c.max_pulse_us, true);
    }
    for (std::size_t i = 0; i < seen.size(); ++i) {
        if (!seen[i]) result.add(ConfigError::MissingJoint, std::nullopt, kAllJoints[i]);
    }
    return result;
}

const char* describe(ConfigError error) {
    switch (error) {
    case ConfigError::MissingJoint: return "logical joint is missing";
    case ConfigError::DuplicateJoint: return "logical joint appears more than once";
    case ConfigError::InvalidJoint: return "joint identifier is invalid";
    case ConfigError::MissingController: return "controller assignment is TBD";
    case ConfigError::InvalidController: return "controller must be A or B";
    case ConfigError::MissingChannel: return "channel assignment is TBD";
    case ConfigError::InvalidChannel: return "PCA9685 channel must be in [0, 15]";
    case ConfigError::DuplicateChannel: return "controller/channel pair is already assigned";
    case ConfigError::Uncalibrated: return "joint has not been calibrated";
    case ConfigError::InvalidCalibrationState: return "calibration state is invalid";
    case ConfigError::MissingDirection: return "servo direction is TBD";
    case ConfigError::InvalidDirection: return "servo direction is invalid";
    case ConfigError::MissingAngle: return "angle bounds or neutral offset are TBD";
    case ConfigError::MissingPulse: return "pulse bounds or neutral pulse are TBD";
    case ConfigError::NonFiniteAngle: return "angle calibration contains NaN or infinity";
    case ConfigError::NonFinitePulse: return "pulse calibration contains NaN or infinity";
    case ConfigError::InvalidAngleRange: return "angle minimum must be below maximum with finite span";
    case ConfigError::NeutralAngleOutsideRange: return "neutral angle must be strictly inside bounds";
    case ConfigError::InvalidPulseRange: return "pulse bounds must be positive, ordered and have finite span";
    case ConfigError::NeutralPulseOutsideRange: return "neutral pulse must be strictly inside bounds";
    case ConfigError::DiagnosticOverflow: return "diagnostic capacity exceeded; configuration rejected";
    }
    return "unknown configuration error";
}
} // namespace arachne
