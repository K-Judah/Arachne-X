#pragma once

#include "arachne/joints.hpp"

namespace arachne {
enum class ControllerId : std::uint8_t { A, B };
enum class Direction : std::uint8_t { Normal, Inverted };
enum class CalibrationState : std::uint8_t { Uncalibrated, Calibrated };

struct Wiring {
    std::optional<ControllerId> controller;
    std::optional<int> channel;
};
struct Calibration {
    CalibrationState state{CalibrationState::Uncalibrated};
    std::optional<Direction> direction;
    // Logical joint angles in radians. neutral_rad is the measured zero offset
    // in this coordinate system, not an assumed servo midpoint.
    std::optional<double> min_rad, neutral_rad, max_rad;
    std::optional<double> min_pulse_us, neutral_pulse_us, max_pulse_us;
};
struct JointConfiguration {
    JointId joint{JointId::Unknown};
    Wiring wiring{};
    Calibration calibration{};
};
struct RobotConfiguration {
    // Empty slots explicitly represent missing joints. Order is not identity.
    std::array<std::optional<JointConfiguration>, kJointCount> joints{};
};
RobotConfiguration unconfigured_robot();

enum class ConfigError : std::uint8_t {
    MissingJoint, DuplicateJoint, InvalidJoint, MissingController,
    InvalidController, MissingChannel, InvalidChannel, DuplicateChannel,
    Uncalibrated, InvalidCalibrationState, MissingDirection, InvalidDirection,
    MissingAngle, MissingPulse, NonFiniteAngle, NonFinitePulse,
    InvalidAngleRange, NeutralAngleOutsideRange, InvalidPulseRange,
    NeutralPulseOutsideRange, DiagnosticOverflow
};
struct Diagnostic {
    ConfigError code{};
    std::optional<std::uint8_t> slot;
    std::optional<JointId> joint;
};
struct ValidationReport {
    // Bounded storage: no heap allocation in validation or command handling.
    std::array<Diagnostic, 256> diagnostics{};
    std::size_t count{0};
    bool ok() const { return count == 0; }
    bool contains(ConfigError code) const;
    void add(ConfigError code, std::optional<std::size_t> slot,
             std::optional<JointId> joint);
};
ValidationReport validate(const RobotConfiguration& config);
const char* describe(ConfigError error);
} // namespace arachne
