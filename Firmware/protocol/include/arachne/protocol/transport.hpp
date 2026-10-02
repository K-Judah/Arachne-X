#pragma once
#include "arachne/protocol/wire.hpp"

namespace arachne::protocol {
enum class ReadResult { Empty, Byte, Closed };
class Transport {
public:
    virtual ~Transport() = default;
    virtual ReadResult read(std::uint8_t& byte) = 0; // nonblocking
    virtual bool write(const std::uint8_t* bytes, std::size_t size) = 0; // all or fail
};

// Bounded single-thread in-memory duplex link, with no physical implementation.
class MemoryTransport final : public Transport {
public:
    static constexpr std::size_t kCapacity = 4 * kMaxWire;
    void connect(MemoryTransport& peer) { peer_ = &peer; }
    void close() { closed_ = true; }
    ReadResult read(std::uint8_t& byte) override {
        if (closed_) return ReadResult::Closed;
        if (size_ == 0) return ReadResult::Empty;
        byte = buffer_[head_];
        head_ = (head_ + 1) % kCapacity;
        --size_;
        return ReadResult::Byte;
    }
    bool write(const std::uint8_t* bytes, std::size_t size) override {
        if (closed_ || !peer_ || peer_->closed_ || size > kCapacity - peer_->size_) return false;
        for (std::size_t i = 0; i < size; ++i)
            peer_->buffer_[(peer_->head_ + peer_->size_ + i) % kCapacity] = bytes[i];
        peer_->size_ += size;
        return true;
    }
private:
    MemoryTransport* peer_{nullptr};
    std::array<std::uint8_t, kCapacity> buffer_{};
    std::size_t head_{0}, size_{0};
    bool closed_{false};
};
} // namespace arachne::protocol
