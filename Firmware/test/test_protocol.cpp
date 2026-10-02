#include "support/protocol_fixture.hpp"

#include <fstream>
#include <algorithm>
#include <limits>
#include <random>
#include <sstream>

using namespace arachne;
using namespace arachne::protocol;
using namespace arachne::test;

// Independent test packer permits malformed headers with a correctly recomputed CRC.
static EncodedFrame raw_frame(std::vector<std::uint8_t> raw) {
    const auto crc = crc16(raw.data(), raw.size());
    raw.push_back(static_cast<std::uint8_t>(crc));
    raw.push_back(static_cast<std::uint8_t>(crc >> 8));
    EncodedFrame result;
    std::size_t at = 0;
    while (at < raw.size()) {
        const auto code_at = result.size++;
        std::uint8_t code = 1;
        while (at < raw.size() && raw[at] != 0 && code < 255) {
            result.bytes[result.size++] = raw[at++]; ++code;
        }
        result.bytes[code_at] = code;
        if (at < raw.size() && raw[at] == 0) {
            ++at;
            if (at == raw.size()) result.bytes[result.size++] = 1;
        }
    }
    result.bytes[result.size++] = 0;
    return result;
}

TEST(crc_reference_and_roundtrip_preserve_all_header_fields) {
    const std::string check = "123456789";
    CHECK(crc16(reinterpret_cast<const std::uint8_t*>(check.data()), check.size()) == 0x29b1);
    Frame input;
    input.type = Type::SetJointTarget;
    input.request = 0xfedcba98;
    input.session = 0x123456789abcdef0ULL;
    input.token = 0x87654321;
    input.length = 9;
    input.payload[0] = 17;
    put_double(input.payload.data() + 1, 0.2);
    EncodedFrame bytes;
    CHECK(encode(input, bytes));
    Frame decoded;
    CHECK(decode(bytes.bytes.data(), bytes.size, decoded) == DecodeError::None);
    CHECK(decoded.request == input.request && decoded.session == input.session && decoded.token == input.token);
    CHECK(decoded.type == input.type && decoded.length == input.length && decoded.payload == input.payload);
}
TEST(all_payload_sizes_and_cobs_zero_patterns_roundtrip) {
    for (std::size_t size = 0; size <= kMaxPayload; ++size) {
        for (const auto fill : {0, 1, 255}) {
            Frame frame;
            frame.length = static_cast<std::uint16_t>(size);
            std::fill_n(frame.payload.begin(), size, static_cast<std::uint8_t>(fill));
            EncodedFrame bytes;
            CHECK(encode(frame, bytes));
            CHECK(bytes.size <= kMaxWire);
            Frame result;
            CHECK(decode(bytes.bytes.data(), bytes.size, result) == DecodeError::None);
            CHECK(result.payload == frame.payload && result.length == frame.length);
        }
    }
}
TEST(truncation_bad_crc_version_type_and_length_are_rejected) {
    EncodedFrame frame;
    CHECK(encode(hello_frame(), frame));
    Frame result;
    for (std::size_t size = 0; size < frame.size; ++size)
        CHECK(decode(frame.bytes.data(), size, result) != DecodeError::None);
    frame.bytes[frame.size - 2] ^= 1;
    CHECK(decode(frame.bytes.data(), frame.size, result) != DecodeError::None);
    std::vector<std::uint8_t> raw(kHeaderSize, 0);
    raw[0] = 2; raw[1] = 3;
    auto altered = raw_frame(raw);
    CHECK(decode(altered.bytes.data(), altered.size, result) == DecodeError::Version);
    raw[0] = 1; raw[1] = 99;
    altered = raw_frame(raw);
    CHECK(decode(altered.bytes.data(), altered.size, result) == DecodeError::Type);
    raw[1] = 3; raw[18] = 1;
    altered = raw_frame(raw);
    CHECK(decode(altered.bytes.data(), altered.size, result) == DecodeError::Length);
}
TEST(stream_parser_fragments_overflow_finish_and_resynchronization) {
    Parser parser;
    Frame result;
    CHECK(parser.push(3, result) == DecodeError::Incomplete);
    CHECK(parser.finish() == DecodeError::Truncated);
    bool overflow = false;
    for (std::size_t i = 0; i < kMaxWire * 3; ++i)
        overflow |= parser.push(1, result) == DecodeError::TooLarge;
    CHECK(overflow);
    CHECK(parser.push(0, result) == DecodeError::Incomplete);
    EncodedFrame good;
    CHECK(encode(hello_frame(), good));
    for (std::size_t i = 0; i + 1 < good.size; ++i) CHECK(parser.push(good.bytes[i], result) == DecodeError::Incomplete);
    CHECK(parser.push(0, result) == DecodeError::None);
}
TEST(handshake_version_negotiation_and_disabled_startup) {
    ProtocolFixture f;
    CHECK(f.actuators.state() == ActuatorState::Disabled);
    auto response = f.hello();
    CHECK(response.type == Type::HelloAck && response.payload[0] == 1 && response.payload[21] == 0);
    CHECK(response.session == 100 && response.token != 0);
    CHECK(f.output.attempts == 0);
    auto bad = hello_frame(++f.sequence);
    bad.payload[0] = bad.payload[1] = 2;
    expect_error(f.exchange(bad), Error::UnsupportedVersion);
    CHECK(!f.endpoint.session_active());
}
TEST(unconfigured_timeout_or_revision_prevents_handshake) {
    ProtocolFixture f(synthetic_config(), {});
    expect_error(f.hello(), Error::NotConfigured);
    CHECK(f.output.attempts == 0);
    auto settings = synthetic_settings(); settings.heartbeat_timeout_us = 0;
    ProtocolFixture zero(synthetic_config(), settings);
    expect_error(zero.hello(), Error::NotConfigured);
}
TEST(status_before_handshake_contains_only_commanded_not_measured_state) {
    ProtocolFixture f;
    const auto response = f.exchange(f.next(Type::GetStatus));
    CHECK(response.type == Type::Status && response.length == 249);
    CHECK(response.payload[0] == 0 && response.payload[2] == 0 && response.payload[4] == 0);
    CHECK(response.payload[5] == 3 && response.payload[6] == 18);
    for (std::size_t i = 0; i < 18; ++i) CHECK(response.payload[15 + 13 * i + 4] == 0);
}
TEST(commands_require_handshake_and_explicit_simulation_enable) {
    ProtocolFixture f;
    expect_error(f.exchange(f.target(0, 0.2)), Error::NoSession);
    f.hello();
    expect_error(f.exchange(f.target(0, 0.2)), Error::Disabled);
    CHECK(f.enable().type == Type::Ack);
    CHECK(f.output.attempts == 0);
    const auto response = f.exchange(f.target(0, 0.2));
    CHECK(response.type == Type::Ack && response.payload[2] == 1 && response.payload[4] == 0);
    CHECK(f.output.attempts == 1);
}
TEST(enable_rejects_wrong_configuration_revision) {
    ProtocolFixture f; f.hello();
    auto request = f.next(Type::EnableRequest); request.length = 4;
    put_uint(request.payload.data(), 8, 4);
    expect_error(f.exchange(request), Error::ConfigMismatch);
    CHECK(f.actuators.state() == ActuatorState::Disabled && f.output.attempts == 0);
}
TEST(heartbeat_rotates_token_without_output_or_automatic_enable) {
    ProtocolFixture f; f.hello();
    const auto previous = f.token;
    const auto response = f.exchange(f.next(Type::Heartbeat));
    CHECK(response.type == Type::HeartbeatReply && f.token != previous);
    CHECK(f.actuators.state() == ActuatorState::Disabled && f.output.attempts == 0);
}
TEST(invalid_joint_nan_and_out_of_range_never_reach_output) {
    ProtocolFixture f; f.hello(); f.enable();
    expect_error(f.exchange(f.target(255, 0.2)), Error::InvalidJoint);
    expect_error(f.exchange(f.target(0, std::numeric_limits<double>::quiet_NaN())), Error::InvalidValue);
    expect_error(f.exchange(f.target(0, std::numeric_limits<double>::infinity())), Error::InvalidValue);
    expect_error(f.exchange(f.target(0, 50.0)), Error::OutOfRange);
    CHECK(f.output.attempts == 0);
    CHECK(f.actuators.state() == ActuatorState::SimulationEnabled); // preflight has no side effects
}
TEST(uncalibrated_joint_rejected_before_control) {
    auto config = synthetic_config(); config.joints[0]->calibration.state = CalibrationState::Uncalibrated;
    ProtocolFixture f(config); f.hello();
    expect_error(f.enable(), Error::InvalidConfiguration);
    expect_error(f.exchange(f.target(0, 0.2)), Error::Uncalibrated);
    CHECK(f.output.attempts == 0);
}
TEST(multi_joint_validation_is_atomic_and_rejects_duplicates) {
    ProtocolFixture f; f.hello(); f.enable();
    auto request = f.next(Type::SetMultiJointTarget);
    request.length = 19; request.payload[0] = 2;
    request.payload[1] = 0; put_double(request.payload.data() + 2, 0.2);
    request.payload[10] = 1; put_double(request.payload.data() + 11, 100.0);
    expect_error(f.exchange(request), Error::OutOfRange);
    CHECK(f.output.attempts == 0);
    request.request = ++f.sequence;
    request.payload[10] = 0; put_double(request.payload.data() + 11, 0.2);
    expect_error(f.exchange(request), Error::DuplicateJoint);
    CHECK(f.output.attempts == 0);
    request.request = ++f.sequence; request.payload[10] = 1;
    CHECK(f.exchange(request).type == Type::Ack);
    CHECK(f.output.attempts == 2);
}
TEST(malformed_payloads_do_not_invoke_commands_or_disable) {
    ProtocolFixture f; f.hello(); f.enable();
    for (const auto type : {Type::Disable, Type::EmergencyStop, Type::Heartbeat, Type::SetJointTarget, Type::SetMultiJointTarget}) {
        auto request = f.next(type); request.length = 1; request.payload[0] = 0;
        expect_error(f.exchange(request), Error::BadPayload);
    }
    CHECK(f.output.attempts == 0 && f.actuators.state() == ActuatorState::SimulationEnabled);
}
TEST(disable_is_idempotent_without_session_or_sequence_and_requires_new_handshake) {
    ProtocolFixture f; f.hello(); f.enable();
    auto request = f.next(Type::Disable); request.session = 999; request.request = 0;
    CHECK(f.exchange(request).type == Type::Ack);
    CHECK(f.exchange(request).type == Type::Ack);
    CHECK(f.actuators.state() == ActuatorState::Disabled && !f.endpoint.session_active());
    expect_error(f.enable(), Error::NoSession);
}
TEST(emergency_stop_latches_across_disable_hello_start_and_enable) {
    ProtocolFixture f; f.hello(); f.enable();
    CHECK(f.exchange(f.next(Type::EmergencyStop)).type == Type::Ack);
    CHECK(f.endpoint.fault() == Fault::EmergencyStop && f.actuators.state() == ActuatorState::Fault);
    CHECK(f.exchange(f.next(Type::Disable)).type == Type::Ack);
    f.actuators.start();
    expect_error(f.hello(), Error::Stopped);
    expect_error(f.exchange(f.target(0, 0.2)), Error::Stopped);
    expect_error(f.exchange(f.next(Type::Heartbeat)), Error::Stopped);
    expect_error(f.enable(), Error::Stopped);
    CHECK(f.exchange(f.next(Type::GetStatus)).payload[1] == static_cast<std::uint8_t>(Fault::EmergencyStop));
    CHECK(f.output.attempts == 0);
}
TEST(heartbeat_timeout_disables_invalidates_and_never_resumes) {
    auto settings = synthetic_settings(); settings.heartbeat_timeout_us = 200;
    ProtocolFixture f(synthetic_config(), settings); f.hello(); f.enable();
    f.clock.time = 200; f.endpoint.tick();
    CHECK(f.actuators.state() == ActuatorState::Disabled && !f.endpoint.session_active());
    CHECK(f.endpoint.fault() == Fault::HeartbeatTimeout);
    expect_error(f.exchange(f.next(Type::Heartbeat)), Error::NoSession);
    f.hello();
    CHECK(f.actuators.state() == ActuatorState::Disabled);
}
TEST(heartbeat_never_extends_the_motion_lease) {
    ProtocolFixture f; f.hello(); f.enable();
    f.clock.time = 400;
    CHECK(f.exchange(f.next(Type::Heartbeat)).type == Type::HeartbeatReply);
    f.clock.time = 500; f.endpoint.tick();
    CHECK(f.endpoint.fault() == Fault::CommandTimeout);
    CHECK(f.actuators.state() == ActuatorState::Disabled);
}
TEST(stale_token_and_expired_token_reject_delayed_commands) {
    ProtocolFixture f; f.hello();
    auto old = f.target(0, 0.2);
    f.exchange(f.next(Type::Heartbeat));
    old.request = ++f.sequence;
    expect_error(f.exchange(old), Error::StaleToken);
    f.clock.time = 500;
    expect_error(f.enable(), Error::StaleToken);
    CHECK(f.output.attempts == 0);
}
TEST(replayed_sequences_and_old_sessions_never_repeat_motion) {
    ProtocolFixture f; f.hello(); f.enable();
    auto request = f.target(0, 0.2);
    CHECK(f.exchange(request).type == Type::Ack);
    expect_error(f.exchange(request), Error::BadSequence);
    f.hello();
    request.request = ++f.sequence;
    expect_error(f.exchange(request), Error::BadSession);
    CHECK(f.output.attempts == 1 && f.actuators.state() == ActuatorState::Disabled);
}
TEST(sequence_wrap_is_not_accepted) {
    ProtocolFixture f;
    auto request = hello_frame(std::numeric_limits<std::uint32_t>::max());
    CHECK(f.exchange(request).type == Type::HelloAck);
    expect_error(f.enable(), Error::BadSequence);
}
TEST(clock_loss_or_rollback_disables_session) {
    ProtocolFixture f; f.clock.time = 100; f.hello(); f.enable();
    f.clock.time = 99; f.endpoint.tick();
    CHECK(f.endpoint.fault() == Fault::Clock && f.actuators.state() == ActuatorState::Disabled);
    f.clock.time.reset(); expect_error(f.hello(), Error::NotConfigured);
    CHECK(f.exchange(f.next(Type::GetStatus)).type == Type::Status);
    CHECK(f.exchange(f.next(Type::Disable)).type == Type::Ack);
    CHECK(f.exchange(f.next(Type::EmergencyStop)).type == Type::Ack);
    CHECK(f.actuators.state() == ActuatorState::Fault);
    CHECK(f.endpoint.fault() == Fault::EmergencyStop);
    CHECK(f.output.attempts == 0);
}
TEST(partial_frame_deadline_and_disconnect_disable) {
    ProtocolFixture f; f.hello(); f.enable();
    const std::uint8_t byte = 3;
    CHECK(f.pi.write(&byte, 1)); f.endpoint.poll(f.esp);
    f.clock.time = 100; f.endpoint.tick();
    CHECK(f.endpoint.malformed_count() == 1 && f.actuators.state() == ActuatorState::Disabled);
    f.hello(); f.enable(); f.esp.close(); f.endpoint.poll(f.esp);
    CHECK(f.endpoint.fault() == Fault::Transport && !f.endpoint.session_active());
}
TEST(transport_backpressure_disables_and_bounds_work) {
    ProtocolFixture f; f.hello(); f.enable();
    std::array<std::uint8_t, MemoryTransport::kCapacity> fill{};
    CHECK(f.esp.write(fill.data(), fill.size()));
    EncodedFrame bytes; CHECK(encode(f.next(Type::GetStatus), bytes));
    CHECK(f.pi.write(bytes.bytes.data(), bytes.size)); f.endpoint.poll(f.esp);
    CHECK(f.endpoint.fault() == Fault::Transport && f.actuators.state() == ActuatorState::Disabled);
}
TEST(frame_deadline_is_checked_before_a_late_delimiter_in_one_poll) {
    ProtocolFixture f; f.hello(); f.enable();
    class AdvancingTransport final : public Transport {
    public:
        AdvancingTransport(MemoryTransport& transport, FakeClock& clock) : transport_(transport), clock_(clock) {}
        ReadResult read(std::uint8_t& byte) override {
            const auto result = transport_.read(byte);
            if (result == ReadResult::Byte) clock_.time = *clock_.time + 4;
            return result;
        }
        bool write(const std::uint8_t* bytes, std::size_t size) override { return transport_.write(bytes, size); }
    private:
        MemoryTransport& transport_;
        FakeClock& clock_;
    } slow(f.esp, f.clock);
    EncodedFrame bytes; CHECK(encode(f.target(0, 0.2), bytes));
    CHECK(f.pi.write(bytes.bytes.data(), bytes.size));
    f.endpoint.poll(slow);
    CHECK(f.output.attempts == 0 && !f.endpoint.session_active());
    CHECK(f.actuators.state() == ActuatorState::Disabled);
    CHECK(f.endpoint.malformed_count() > 0);
}
TEST(output_failure_latches_existing_actuator_fault) {
    ProtocolFixture f; f.hello(); f.enable(); f.output.succeed = false;
    expect_error(f.exchange(f.target(0, 0.2)), Error::ActuatorFailure);
    CHECK(f.actuators.state() == ActuatorState::Fault);
    expect_error(f.hello(), Error::Stopped);
    CHECK(f.output.attempts == 1);
}
TEST(status_reports_commanded_position_and_corruption_counter) {
    ProtocolFixture f; f.hello(); f.enable(); f.exchange(f.target(17, 0.2));
    const std::uint8_t bad[] = {2, 1, 0};
    CHECK(f.pi.write(bad, sizeof(bad))); f.endpoint.poll(f.esp);
    CHECK(!drain(f.pi));
    const auto result = f.exchange(f.next(Type::GetStatus));
    CHECK(get_uint(result.payload.data() + 11, 4) == 1);
    CHECK(result.payload[15 + 17 * 13 + 4] == 1);
    NEAR(get_double(result.payload.data() + 15 + 17 * 13 + 5), 0.2);
}
TEST(random_malformed_streams_and_bit_flips_never_emit_output) {
    ProtocolFixture f; f.hello(); f.enable();
    std::mt19937 random(0x41525831);
    for (int n = 0; n < 10000; ++n) {
        std::array<std::uint8_t, kMaxWire> bytes{};
        const auto size = static_cast<std::size_t>(random() % (kMaxWire - 1) + 1);
        for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<std::uint8_t>(random());
        bytes[size] = 0;
        CHECK(f.pi.write(bytes.data(), size + 1)); f.endpoint.poll(f.esp);
        CHECK(!drain(f.pi));
    }
    EncodedFrame good; CHECK(encode(f.target(0, 0.2), good));
    for (std::size_t i = 0; i < good.size - 1; ++i) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            auto bad = good; bad.bytes[i] ^= static_cast<std::uint8_t>(1u << bit);
            CHECK(f.pi.write(bad.bytes.data(), bad.size)); f.endpoint.poll(f.esp);
            CHECK(!drain(f.pi));
        }
    }
    CHECK(f.output.attempts == 0 && f.endpoint.malformed_count() > 0);
}
TEST(shared_golden_wire_vectors) {
    std::ifstream input(ARACHNE_GOLDEN_PATH);
    CHECK(input.good());
    std::string line;
    std::size_t count = 0;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream fields(line);
        std::string name, payload, hex;
        unsigned type; std::uint32_t request, token; std::uint64_t session;
        CHECK(static_cast<bool>(fields >> name >> type >> request >> session >> token >> payload >> hex));
        Frame frame; frame.type = static_cast<Type>(type); frame.request = request; frame.session = session; frame.token = token;
        if (payload != "-") {
            frame.length = static_cast<std::uint16_t>(payload.size() / 2);
            for (std::size_t i = 0; i < frame.length; ++i)
                frame.payload[i] = static_cast<std::uint8_t>(std::stoul(payload.substr(i * 2, 2), nullptr, 16));
        }
        EncodedFrame encoded; CHECK(encode(frame, encoded));
        CHECK(hex.size() == encoded.size * 2);
        for (std::size_t i = 0; i < encoded.size; ++i)
            CHECK(encoded.bytes[i] == std::stoul(hex.substr(i * 2, 2), nullptr, 16));
        ++count;
    }
    CHECK(count >= 3);
}

int main() { return tests::run(); }
