#pragma once

#include "arachne/configuration.hpp"

namespace arachne {
enum class OutputDomain { Physical, Simulation, Unavailable };
struct PulseCommand {
    JointId joint;
    ControllerId controller;
    int channel;
    double pulse_us;
    std::uint64_t timestamp_us;
};
class ServoOutput {
public:
    virtual ~ServoOutput() = default;
    // Unknown implementations default to physical and are rejected by this phase.
    virtual OutputDomain domain() const { return OutputDomain::Physical; }
    virtual bool write(const PulseCommand& command) = 0;
};
class Clock {
public:
    virtual ~Clock() = default;
    virtual std::optional<std::uint64_t> now_us() const = 0;
};
class DisabledOutput final : public ServoOutput {
public:
    OutputDomain domain() const override { return OutputDomain::Unavailable; }
    bool write(const PulseCommand&) override { return false; }
};
class UnavailableClock final : public Clock {
public:
    std::optional<std::uint64_t> now_us() const override { return std::nullopt; }
};
} // namespace arachne
