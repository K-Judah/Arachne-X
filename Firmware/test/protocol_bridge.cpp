// Host-only test process. Hex lines are a test harness, NOT a selected robot link.
#include "support/protocol_fixture.hpp"
#include <iomanip>
#include <sstream>

using namespace arachne;
using namespace arachne::protocol;
using namespace arachne::test;

int main(int argc, char** argv) {
    auto config = synthetic_config();
    if (argc > 1 && std::string(argv[1]) == "uncalibrated")
        config.joints[0]->calibration.state = CalibrationState::Uncalibrated;
    ProtocolFixture f(config);
    std::string line;
    while (std::getline(std::cin, line)) {
        try {
            std::istringstream input(line); std::string operation; input >> operation;
            if (operation == "DATA") {
                std::string hex; input >> hex;
                if (hex.size() % 2 != 0 || hex.size() > 2 * MemoryTransport::kCapacity) return 2;
                std::vector<std::uint8_t> bytes;
                for (std::size_t i = 0; i < hex.size(); i += 2)
                    bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
                if (!f.pi.write(bytes.data(), bytes.size())) return 3;
                for (std::size_t i = 0; i < 4; ++i) f.endpoint.poll(f.esp);
                std::uint8_t byte = 0; bool any = false;
                while (f.pi.read(byte) == ReadResult::Byte) {
                    any = true;
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
                }
                if (!any) std::cout << '-';
                std::cout << std::dec << std::endl;
            } else if (operation == "TIME") {
                std::uint64_t now; if (!(input >> now)) return 4;
                f.clock.time = now; f.endpoint.tick(); std::cout << "OK" << std::endl;
            } else if (operation == "COUNT") {
                std::cout << f.output.attempts << std::endl;
            } else if (operation == "DROP") {
                f.endpoint.disconnect(); std::cout << "OK" << std::endl;
            } else return 5;
        } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 6; }
    }
    return 0;
}
