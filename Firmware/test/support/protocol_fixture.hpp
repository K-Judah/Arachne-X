#pragma once
#include "arachne/protocol/endpoint.hpp"
#include "fakes.hpp"
#include "test_runner.hpp"

namespace arachne::test {
using namespace arachne::protocol;
inline Settings synthetic_settings() { return {1000, 500, 100, 100, 7}; }
inline Frame hello_frame(std::uint32_t request = 1) {
    Frame frame;
    frame.type = Type::Hello;
    frame.request = request;
    frame.length = 2;
    frame.payload[0] = frame.payload[1] = kVersion;
    return frame;
}
inline std::optional<Frame> drain(MemoryTransport& transport) {
    Parser parser;
    Frame frame;
    std::optional<Frame> result;
    std::uint8_t byte = 0;
    while (transport.read(byte) == ReadResult::Byte) {
        const auto decoded = parser.push(byte, frame);
        if (decoded == DecodeError::None) { CHECK(!result); result = frame; }
        else CHECK(decoded == DecodeError::Incomplete);
    }
    return result;
}
struct ProtocolFixture {
    FakeClock clock;
    FakeOutput output;
    ActuatorSystem actuators;
    Endpoint endpoint;
    MemoryTransport pi, esp;
    std::uint32_t sequence{0}, token{0};
    std::uint64_t session{0};
    explicit ProtocolFixture(RobotConfiguration config = synthetic_config(), Settings settings = synthetic_settings())
        : actuators(config, output, clock), endpoint(actuators, clock, settings) {
        pi.connect(esp); esp.connect(pi);
    }
    Frame next(Type type) {
        Frame request;
        request.type = type;
        request.request = ++sequence;
        request.session = session;
        request.token = token;
        return request;
    }
    Frame exchange(const Frame& request) {
        EncodedFrame encoded;
        CHECK(encode(request, encoded));
        CHECK(pi.write(encoded.bytes.data(), encoded.size));
        endpoint.poll(esp);
        const auto response = drain(pi);
        CHECK(response);
        CHECK(response->request == request.request);
        session = response->session;
        token = response->token;
        return *response;
    }
    Frame hello() { return exchange(hello_frame(++sequence)); }
    Frame enable() {
        auto request = next(Type::EnableRequest);
        request.length = 4;
        put_uint(request.payload.data(), 7, 4);
        return exchange(request);
    }
    Frame target(std::uint8_t joint, double angle) {
        auto request = next(Type::SetJointTarget);
        request.length = 9;
        request.payload[0] = joint;
        put_double(request.payload.data() + 1, angle);
        return request;
    }
};
inline void expect_error(const Frame& frame, Error error) {
    CHECK(frame.type == Type::Nack);
    CHECK(frame.payload[1] == static_cast<std::uint8_t>(error));
    CHECK(frame.payload[4] == 0);
}
} // namespace arachne::test
