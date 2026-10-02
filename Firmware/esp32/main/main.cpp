#include "arachne/actuator.hpp"

// Deliberately no GPIO, I2C, PWM, serial, tasks, or hardware initialization.
// Returning from app_main is supported by ESP-IDF. Firmware stays inert.
extern "C" void app_main() {
    static arachne::DisabledOutput output;
    static arachne::UnavailableClock clock;
    static const auto config = arachne::unconfigured_robot();
    static arachne::ActuatorSystem actuators(config, output, clock);
    actuators.start();
}
