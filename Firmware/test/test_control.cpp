#include "arachne/actuator.hpp"
#include "support/fakes.hpp"
#include "support/test_runner.hpp"

#include <algorithm>
#include <limits>
#include <set>

using namespace arachne;
using namespace arachne::test;

TEST(all_eighteen_joints_and_six_complete_legs) {
    CHECK(kAllJoints.size() == 18);
    std::set<JointId> ids(kAllJoints.begin(), kAllJoints.end());
    CHECK(ids.size() == 18);
    for (std::size_t leg = 0; leg < 6; ++leg) {
        for (const auto kind : {JointKind::Coxa, JointKind::Femur, JointKind::Tibia}) {
            const auto id = joint_id(static_cast<Leg>(leg), kind);
            CHECK(id && ids.count(*id) == 1);
            CHECK(leg_of(*id) == static_cast<Leg>(leg));
            CHECK(kind_of(*id) == kind);
            CHECK(std::string(joint_name(*id)) != "invalid joint");
        }
    }
    CHECK(!joint_id(static_cast<Leg>(6), JointKind::Coxa));
    CHECK(!joint_id(Leg::LF, static_cast<JointKind>(3)));
    CHECK(!leg_of(static_cast<JointId>(18)));
    CHECK(!kind_of(static_cast<JointId>(255)));
}
TEST(production_configuration_has_no_invented_values) {
    const auto config = unconfigured_robot();
    for (const auto& slot : config.joints) {
        CHECK(slot);
        CHECK(!slot->wiring.controller && !slot->wiring.channel);
        const auto& c = slot->calibration;
        CHECK(c.state == CalibrationState::Uncalibrated);
        CHECK(!c.direction && !c.min_rad && !c.neutral_rad && !c.max_rad);
        CHECK(!c.min_pulse_us && !c.neutral_pulse_us && !c.max_pulse_us);
    }
    CHECK(!validate(config).ok());
    CHECK(!validate(config).contains(ConfigError::MissingJoint));
}
TEST(valid_synthetic_configuration_and_separate_controller_namespaces) {
    CHECK(validate(synthetic_config()).ok()); // channel 0 exists on A and B.
}
TEST(duplicate_channels_include_location_and_message) {
    auto config = synthetic_config();
    config.joints[1]->wiring = config.joints[0]->wiring;
    const auto report = validate(config);
    CHECK(report.contains(ConfigError::DuplicateChannel));
    CHECK(report.count == 1);
    CHECK(report.diagnostics[0].slot == 1);
    CHECK(report.diagnostics[0].joint == JointId::LF_Femur);
    CHECK(std::string(describe(report.diagnostics[0].code)).find("already assigned") != std::string::npos);
}
TEST(invalid_and_missing_channels) {
    for (const int channel : {-1, 16, 256, std::numeric_limits<int>::max()}) {
        auto config = synthetic_config();
        config.joints[0]->wiring.channel = channel;
        CHECK(validate(config).contains(ConfigError::InvalidChannel));
    }
    auto config = synthetic_config();
    config.joints[0]->wiring.channel.reset();
    CHECK(validate(config).contains(ConfigError::MissingChannel));
    config.joints[0]->wiring.channel = 15;
    CHECK(validate(config).ok());
}
TEST(missing_joint_and_empty_configuration) {
    auto config = synthetic_config();
    config.joints[5].reset();
    const auto report = validate(config);
    CHECK(report.count == 1);
    CHECK(report.diagnostics[0].code == ConfigError::MissingJoint);
    CHECK(report.diagnostics[0].joint == JointId::LM_Tibia);
    CHECK(validate(RobotConfiguration{}).count == 18);
}
TEST(duplicate_joint_also_reports_displaced_joint) {
    auto config = synthetic_config();
    config.joints[1]->joint = JointId::LF_Coxa;
    CHECK(validate(config).contains(ConfigError::DuplicateJoint));
    CHECK(validate(config).contains(ConfigError::MissingJoint));
}
TEST(invalid_joint_id_is_rejected_without_indexing_it) {
    auto config = synthetic_config();
    config.joints[0]->joint = static_cast<JointId>(255);
    CHECK(validate(config).contains(ConfigError::InvalidJoint));
    CHECK(validate(config).contains(ConfigError::MissingJoint));
}
TEST(default_joint_record_has_explicitly_unknown_identity) {
    auto config = synthetic_config();
    config.joints[0] = JointConfiguration{};
    CHECK(config.joints[0]->joint == JointId::Unknown);
    CHECK(validate(config).contains(ConfigError::InvalidJoint));
    CHECK(validate(config).contains(ConfigError::MissingJoint));
}
TEST(invalid_and_missing_controller) {
    auto config = synthetic_config();
    config.joints[0]->wiring.controller = static_cast<ControllerId>(255);
    CHECK(validate(config).contains(ConfigError::InvalidController));
    config.joints[0]->wiring.controller.reset();
    CHECK(validate(config).contains(ConfigError::MissingController));
}
TEST(uncalibrated_and_invalid_calibration_state) {
    auto config = synthetic_config();
    config.joints[0]->calibration.state = CalibrationState::Uncalibrated;
    CHECK(validate(config).contains(ConfigError::Uncalibrated));
    config.joints[0]->calibration.state = static_cast<CalibrationState>(255);
    CHECK(validate(config).contains(ConfigError::InvalidCalibrationState));
}
TEST(missing_and_invalid_direction) {
    auto config = synthetic_config();
    config.joints[0]->calibration.direction.reset();
    CHECK(validate(config).contains(ConfigError::MissingDirection));
    config.joints[0]->calibration.direction = static_cast<Direction>(255);
    CHECK(validate(config).contains(ConfigError::InvalidDirection));
}
TEST(every_calibration_value_is_required) {
    for (const auto member : {&Calibration::min_rad, &Calibration::neutral_rad, &Calibration::max_rad,
                              &Calibration::min_pulse_us, &Calibration::neutral_pulse_us, &Calibration::max_pulse_us}) {
        auto config = synthetic_config();
        (config.joints[0]->calibration.*member).reset();
        CHECK(!validate(config).ok());
    }
}
TEST(invalid_min_max_relationships) {
    auto config = synthetic_config();
    auto& c = config.joints[0]->calibration;
    c.min_rad = c.max_rad;
    CHECK(validate(config).contains(ConfigError::InvalidAngleRange));
    c.min_rad = 2.0;
    CHECK(validate(config).contains(ConfigError::InvalidAngleRange));
    c.min_pulse_us = c.max_pulse_us;
    CHECK(validate(config).contains(ConfigError::InvalidPulseRange));
    c.min_pulse_us = 900.0;
    CHECK(validate(config).contains(ConfigError::InvalidPulseRange));
    c.min_pulse_us = 0.0;
    CHECK(validate(config).contains(ConfigError::InvalidPulseRange));
    c.min_pulse_us = -1.0;
    CHECK(validate(config).contains(ConfigError::InvalidPulseRange));
}
TEST(neutral_must_be_strictly_inside_both_ranges) {
    for (const double angle : {-1.0, -0.8, 1.6, 2.0}) {
        auto config = synthetic_config();
        config.joints[0]->calibration.neutral_rad = angle;
        CHECK(validate(config).contains(ConfigError::NeutralAngleOutsideRange));
    }
    for (const double pulse : {100.0, 200.0, 800.0, 900.0}) {
        auto config = synthetic_config();
        config.joints[0]->calibration.neutral_pulse_us = pulse;
        CHECK(validate(config).contains(ConfigError::NeutralPulseOutsideRange));
    }
}
TEST(nonfinite_calibration_rejected_in_every_numeric_field) {
    for (const auto member : {&Calibration::min_rad, &Calibration::neutral_rad, &Calibration::max_rad,
                              &Calibration::min_pulse_us, &Calibration::neutral_pulse_us, &Calibration::max_pulse_us}) {
        for (const double bad : {std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::infinity(),
                                 -std::numeric_limits<double>::infinity()}) {
            auto config = synthetic_config();
            config.joints[0]->calibration.*member = bad;
            CHECK(!validate(config).ok());
        }
    }
}
TEST(overflowing_angle_span_rejected) {
    auto config = synthetic_config();
    config.joints[0]->calibration.min_rad = -std::numeric_limits<double>::max();
    config.joints[0]->calibration.max_rad = std::numeric_limits<double>::max();
    CHECK(validate(config).contains(ConfigError::InvalidAngleRange));
}
TEST(construction_start_and_disabled_commands_emit_nothing) {
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.state() == ActuatorState::Disabled);
    CHECK(output.attempts == 0);
    controller.start();
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::Disabled);
    CHECK(output.attempts == 0);
}
TEST(one_uncalibrated_joint_blocks_the_entire_system) {
    auto config = synthetic_config();
    config.joints[17]->calibration.state = CalibrationState::Uncalibrated;
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(config, output, clock);
    CHECK(controller.enable_simulation() == CommandResult::InvalidConfiguration);
    CHECK(controller.command(JointId::RR_Tibia, 0.2) == CommandResult::Disabled);
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::Disabled);
    CHECK(output.attempts == 0);
}
TEST(explicit_enable_emits_nothing_until_a_command) {
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    CHECK(output.attempts == 0);
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::Accepted);
    CHECK(output.commands.size() == 1);
    NEAR(output.commands[0].pulse_us, 350.0);
}
TEST(mapping_uses_identity_not_slot_order_for_all_joints) {
    auto config = synthetic_config();
    std::reverse(config.joints.begin(), config.joints.end());
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(config, output, clock);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    for (std::size_t i = 0; i < kJointCount; ++i) {
        clock.time = i;
        CHECK(controller.command(kAllJoints[i], 0.2) == CommandResult::Accepted);
        const auto& pulse = output.commands.back();
        CHECK(pulse.joint == kAllJoints[i]);
        CHECK(pulse.controller == (i < 9 ? ControllerId::A : ControllerId::B));
        CHECK(pulse.channel == static_cast<int>(i % 9));
        CHECK(pulse.timestamp_us == i);
    }
}
TEST(normal_mapping_respects_offset_and_asymmetric_endpoints) {
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    for (const double angle : {-0.8, -0.3, 0.2, 0.9, 1.6})
        CHECK(controller.command(JointId::LF_Coxa, angle) == CommandResult::Accepted);
    const std::array<double, 5> expected{200.0, 275.0, 350.0, 575.0, 800.0};
    for (std::size_t i = 0; i < expected.size(); ++i) NEAR(output.commands[i].pulse_us, expected[i]);
}
TEST(inversion_reverses_endpoints_but_preserves_measured_neutral) {
    auto config = synthetic_config();
    config.joints[0]->calibration.direction = Direction::Inverted;
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(config, output, clock);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    for (const double angle : {-0.8, -0.3, 0.2, 0.9, 1.6})
        CHECK(controller.command(JointId::LF_Coxa, angle) == CommandResult::Accepted);
    const std::array<double, 5> expected{800.0, 575.0, 350.0, 275.0, 200.0};
    for (std::size_t i = 0; i < expected.size(); ++i) NEAR(output.commands[i].pulse_us, expected[i]);
}
TEST(invalid_commands_latch_fault_without_output) {
    for (const double bad : {-0.9, 1.7, std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()}) {
        FakeOutput output;
        FakeClock clock;
        ActuatorSystem controller(synthetic_config(), output, clock);
        CHECK(controller.enable_simulation() == CommandResult::Accepted);
        CHECK(controller.command(JointId::LF_Coxa, bad) != CommandResult::Accepted);
        CHECK(controller.state() == ActuatorState::Fault);
        controller.disable();
        controller.start();
        CHECK(controller.enable_simulation() == CommandResult::FaultLatched);
        CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::FaultLatched);
        CHECK(output.attempts == 0);
    }
}
TEST(invalid_command_joint_latches_fault) {
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    CHECK(controller.command(static_cast<JointId>(255), 0.2) == CommandResult::InvalidJoint);
    CHECK(output.attempts == 0);
}
TEST(disable_and_restart_stop_further_commands) {
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::Accepted);
    controller.disable();
    controller.disable();
    CHECK(controller.command(JointId::LF_Coxa, 0.3) == CommandResult::Disabled);
    CHECK(output.attempts == 1);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    controller.start();
    CHECK(controller.command(JointId::LF_Coxa, 0.3) == CommandResult::Disabled);
    CHECK(output.attempts == 1);
}
TEST(configuration_snapshot_cannot_be_changed_by_caller) {
    auto config = synthetic_config();
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(config, output, clock);
    config.joints[0]->wiring.channel = 15;
    config.joints[0]->calibration.neutral_pulse_us = 700.0;
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::Accepted);
    CHECK(output.commands[0].channel == 0);
    NEAR(output.commands[0].pulse_us, 350.0);
}
TEST(output_failure_latches_and_prevents_retries) {
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    output.succeed = false;
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::OutputFailure);
    output.succeed = true;
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::FaultLatched);
    CHECK(controller.enable_simulation() == CommandResult::FaultLatched);
    CHECK(output.attempts == 1 && output.commands.empty());
}
TEST(unavailable_clock_blocks_enable_and_commands) {
    FakeOutput output;
    FakeClock clock;
    clock.time.reset();
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.enable_simulation() == CommandResult::ClockUnavailable);
    CHECK(controller.state() == ActuatorState::Disabled);
    clock.time = 0;
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    clock.time.reset();
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::ClockUnavailable);
    CHECK(controller.state() == ActuatorState::Fault);
    CHECK(output.attempts == 0);
}
TEST(backwards_clock_latches_fault) {
    FakeOutput output;
    FakeClock clock;
    clock.time = 100;
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    clock.time = 99;
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::ClockReversed);
    CHECK(output.attempts == 0);
}
TEST(physical_and_unavailable_outputs_cannot_be_enabled) {
    for (const auto domain : {OutputDomain::Physical, OutputDomain::Unavailable}) {
        FakeOutput output;
        output.output_domain = domain;
        FakeClock clock;
        ActuatorSystem controller(synthetic_config(), output, clock);
        CHECK(controller.enable_simulation() == CommandResult::PhysicalOutputLocked);
        CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::Disabled);
        CHECK(output.attempts == 0);
    }
}
TEST(changing_output_domain_after_enable_is_rejected) {
    FakeOutput output;
    FakeClock clock;
    ActuatorSystem controller(synthetic_config(), output, clock);
    CHECK(controller.enable_simulation() == CommandResult::Accepted);
    output.output_domain = OutputDomain::Physical;
    CHECK(controller.command(JointId::LF_Coxa, 0.2) == CommandResult::PhysicalOutputLocked);
    CHECK(output.attempts == 0);
}
TEST(diagnostics_are_bounded_and_overflow_remains_an_error) {
    ValidationReport report;
    for (std::size_t i = 0; i < 300; ++i)
        report.add(ConfigError::MissingJoint, std::nullopt, std::nullopt);
    CHECK(report.count == report.diagnostics.size());
    CHECK(report.contains(ConfigError::DiagnosticOverflow));
    CHECK(!report.ok());
}

int main() { return tests::run(); }
