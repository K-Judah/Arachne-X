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

The latest recorded progress (2026-07-24) reports completed coxa/femur models, an incomplete leg assembly, and tibia work in progress. Firmware, computer vision, Pi control services, and the dashboard are not yet implemented in this repository.

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
