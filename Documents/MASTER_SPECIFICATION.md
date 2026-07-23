# Arachne-X

Arachne-X is a six-legged autonomous spider robot designed for robotics, embedded systems, AI, computer vision, and autonomous navigation research.

The project is being developed by a Mechatronics Engineering student as a long-term engineering portfolio and learning platform.

## Vision

Build a spider robot capable of:

- Walking autonomously
- Remote wireless operation
- Real-time video streaming
- Person detection
- Object detection
- Target tracking
- Environmental sensing
- Future autonomous patrol missions

## Hardware

### Processing

- Raspberry Pi 4B
- ESP32 DevKit V1

### Motion

- DS3218 Servos
- PCA9685 Servo Drivers

### Vision

- Raspberry Pi Camera Module 3

### Power

- LiPo Battery System

### Structure

- PETG 3D Printed Frame
- M3 Fasteners

## Leg Architecture

Each leg consists of:

1. Coxa
2. Femur
3. Tibia

Each joint is actuated by a DS3218 servo.

Total:

6 Legs
18 Servos

Phone / Laptop
        │
        ▼
Web Dashboard
        │
        ▼
Raspberry Pi 4B
        │
        ▼
ESP32
        │
        ▼
PCA9685
        │
        ▼
Servos

Dashboard & Control System
Overview

Arachne-X will be operated through a custom web-based command center accessible from a phone, tablet, or computer over Wi-Fi.

The dashboard serves as the primary interface between the operator and the robot, providing real-time control, visual feedback, telemetry monitoring, and autonomous mission management.

The dashboard is inspired by modern military, aerospace, and autonomous robotics control systems.

Live Camera Feed
Purpose

Provide real-time visual awareness of the robot's surroundings.

Features
Low-latency video stream from Raspberry Pi Camera Module 3
Full-screen viewing mode
Adjustable video quality
Daylight and low-light operation
Future support for night vision camera modes
Display Information
Camera status
Frame rate (FPS)
Resolution
Connection quality
Person Detection
Purpose

Identify and highlight humans within the camera's field of view.

Features
Real-time detection using AI computer vision
Bounding box overlay around detected people
Confidence percentage display
Multiple-person tracking
Detection logging
Example Display
PERSON DETECTED
Confidence: 98.4%
Distance: Estimated
Tracking: Active
Future Expansion
Facial recognition experiments
Known vs unknown person identification
Follow-person mode
Object Detection
Purpose

Detect and classify objects within the environment.

Features
Real-time object recognition
Bounding box overlays
Confidence scores
Multiple object support
Example Objects
Chairs
Tables
Bags
Vehicles
Animals
Electronics
Obstacles
Future Expansion
Custom object training
Mission-specific object detection
Threat identification
Battery Monitoring
Purpose

Monitor robot power levels and prevent unexpected shutdowns.

Dashboard Information
Battery percentage
Battery voltage
Estimated remaining runtime
Charging status
Power consumption trends
Warning Levels
Green
Battery > 60%
Yellow
Battery 30% - 60%
Red
Battery < 30%
Emergency Actions

Future versions may:

Return to home position
Enter low-power mode
Disable non-critical systems
Manual Control System
Purpose

Allow direct operator control of robot movement.

Controls
Directional Movement
Forward
Backward
Strafe Left
Strafe Right
Rotation
Rotate Left
Rotate Right
Speed Modes
Slow
Normal
Fast
Posture Controls
Stand
Sit
Crouch
Raise Body
Lower Body
Emergency Functions
Stop All Motion
Servo Disable
Reboot System
Autonomous Mode
Purpose

Allow the robot to operate without continuous user input.

Modes
Patrol Mode

Robot follows predefined routes.

Follow Mode

Robot follows a selected target.

Observation Mode

Robot remains stationary while monitoring surroundings.

Exploration Mode

Robot investigates unknown areas.

Return Home Mode

Robot navigates back to a predefined starting position.

Future Modes
SLAM Navigation
Autonomous Mapping
Security Patrol Missions
Search and Inspection Operations
Telemetry System
Purpose

Provide real-time robot status information.

Displayed Metrics
Power
Battery percentage
Battery voltage
Power draw
Processing
Raspberry Pi CPU usage
Raspberry Pi temperature
Memory usage
Connectivity
Wi-Fi signal strength
Network latency
Connection quality
Motion
Walking speed
Current gait
Servo status
Leg positions
Vision
Active detections
Camera status
AI processing load
System Health

Display:

SYSTEM STATUS: ONLINE

Motors: OK
Vision: OK
Network: OK
Battery: OK
Navigation: OK
Long-Term Goal

The dashboard should eventually function as a complete command center for Arachne-X, enabling:

Manual operation
Autonomous operation
Real-time monitoring
AI-assisted navigation
Mission planning
Data collection
Future multi-robot coordination

This interface will serve as the primary control hub for all current and future versions of the Arachne-X platform.

Phase 1
Single Leg Prototype

Phase 2
Full Leg Validation

Phase 3
Body Design

Phase 4
Six-Leg Assembly

Phase 5
Vision Integration

Phase 6
AI Detection

Phase 7
Autonomous Navigation

Completed

- Initial concept
- Hardware selection
- Dashboard concept
- Leg CAD v1

In Progress

- Mechanical redesign
- Hardware integration
- Repository setup
