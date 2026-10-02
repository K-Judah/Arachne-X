# Arachne-X

Arachne-X is a six-legged autonomous spider robot being developed as a long-term robotics and AI project.

## Goals

- Autonomous navigation
- AI vision
- Remote control
- Object detection
- Person tracking
- Computer vision
- Robotics research

## Hardware

- Raspberry Pi 4B
- ESP32-WROOM-32U based development board (exact board variant TBD)
- 2 PCA9685 servo controllers
- 18 MG996R servos: 6 legs with 3 DOF per leg
- Camera-based computer vision (camera model TBD)
- IMU and ToF sensors are planned options; selection and integration TBD

## Current Status

🚧 Single-leg prototype in development.

The July 24 progress log is historical: subsequent remote work added tibia and chassis models and updated coxa/femur and leg assembly assets. Their physical fit, range of motion and calibration still require verification. A [host-testable ESP32 firmware foundation](Firmware/README.md) provides joint configuration, validation and simulated actuator control. [Command/telemetry protocol v1](protocol/PROTOCOL.md) and a [Python reference client](Raspberry_Pi/README.md) now support simulated Pi/ESP32 exchanges. **Physical servo output is intentionally unavailable.** The repository also contains [OpenCV vision prototypes](Computer_Vision/README.md) and a [dashboard draft](Dashboard/dashboard.html). These are not connected to protocol v1; the dashboard uses a separate HTTP /cmd assumption, and vision steering labels are display output. Full Pi control services and gait/IK remain unimplemented.

## Software Planning

- [Proposed software architecture](docs/SOFTWARE_ARCHITECTURE.md)
- [Repository audit and development plan](docs/DEVELOPMENT_PLAN.md)
- [Instructions for coding agents](AGENTS.md)

The hardware list above is the current planning baseline. Existing master specifications and mechanical documents retain historical DS3218/DevKit V1 references, and the old component list specifies one PCA9685. Those references need reconciliation with the current baseline; they do not establish compatibility with the evolving CAD. Mechanical dimensions, wiring, calibration and power-system values remain TBD until documented and validated.

## Team

### Judah
Project Founder & Lead Systems Engineer

Responsible for:
- System Architecture
- Mechanical Design
- Hardware Integration
- Project Management

### Bilaal
Co-Developer

Responsible for:
- Software Development
- Firmware
- AI
- Dashboard Development
