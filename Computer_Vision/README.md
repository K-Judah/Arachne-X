
# ARACHNE-X Mk I Vision

> Computer Vision subsystem for the ARACHNE-X Autonomous Hexapod Robot.

---

## Overview

The ARACHNE-X Vision System is responsible for allowing the robot to perceive, interpret, and react to its environment in real time.

The long-term objective is to develop an autonomous vision system capable of:

- Object detection
- Object tracking
- Human detection
- Obstacle recognition
- Target following
- Autonomous navigation
- Integration with the robot's AI system

Development is currently being carried out in Python using OpenCV before deployment onto the Raspberry Pi.

---

# Current Features

## Camera Initialisation

- Webcam initialisation
- Camera availability checking
- Live video streaming
- Graceful shutdown

---

## Vision HUD

A custom Heads-Up Display (HUD) was developed featuring:

- ARACHNE-X Mk I title
- FPS counter
- Camera resolution
- Camera status
- Operator name
- Central aiming reticle

---

## Frame Processing

Each captured frame is processed in real time by:

1. Capturing image
2. Converting BGR → HSV colour space
3. Detecting selected colours
4. Finding object contours
5. Drawing object boundaries
6. Calculating object centre
7. Estimating object position
8. Producing steering decisions

---

## Colour Detection

Implemented colour-based object detection using HSV filtering.

Current supported colour:

- Red

Processing pipeline:

```
Camera
    ↓
Capture Frame
    ↓
Convert BGR → HSV
    ↓
HSV Thresholding
    ↓
Binary Mask
```

---

## Object Detection

Implemented contour-based object detection using:

- `cv2.findContours()`
- `cv2.boundingRect()`
- `cv2.contourArea()`

Each detected object displays:

- Bounding box
- Object centre
- Object coordinates

Small contours are filtered to remove image noise.

---

## Object Tracking

For each detected object:

- Calculate centre coordinates
- Display coordinates
- Draw centre marker

Example:

```
Object Centre:
(318, 241)
```

---

## Robot Steering Logic

The vision system compares the detected object's centre with the camera's centre.

```
Error = Object X - Frame Centre X
```

Based on this error, the robot decides whether to:

- Turn Left
- Turn Right
- Go Straight

Current steering thresholds:

```
Error > 40      → Turn Right
Error < -40     → Turn Left
Otherwise        → Go Straight
```

---

## Current Vision Pipeline

```
Camera
    │
    ▼
Capture Frame
    │
    ▼
Convert to HSV
    │
    ▼
Create Colour Mask
    │
    ▼
Find Contours
    │
    ▼
Remove Noise
    │
    ▼
Calculate Bounding Box
    │
    ▼
Find Object Centre
    │
    ▼
Compare With Camera Centre
    │
    ▼
Determine Steering Direction
    │
    ▼
Display HUD
```

---

# Technologies Used

- Python 3.14
- OpenCV
- NumPy

---

# Current Project Structure

```
Computer_Vision/

│
├── Lessons/
│   ├── Camera_Test.py
│   ├── Pixel_Reader.py
│   ├── HSV_Explorer.py
│   ├── Colour_Detection.py
│   ├── Object_Tracking.py
│   └── MkII_Vision.py
│
├── Images/
│
├── Videos/
│
└── README.md
```

---

# Future Development

The following features are planned:

## Motion Detection

- Background subtraction
- Intruder detection
- Patrol mode

---

## Face Detection

- Human face recognition
- Operator tracking

---

## AI Object Detection

Integration of YOLO for detection of:

- Person
- Backpack
- Bottle
- Chair
- Laptop
- Phone
- Door
- Animals
- Vehicles

---

## Multi-Object Tracking

Track multiple objects simultaneously.

---

## Depth Estimation

Estimate target distance using camera vision.

---

## Raspberry Pi Deployment

Port the complete vision system to the Raspberry Pi onboard computer.

---

## Robot Integration

Connect vision output to:

- ESP32 controller
- Servo control system
- Navigation system
- AI decision engine

---

# Long-Term Goal

The ARACHNE-X Vision System will eventually serve as the robot's primary perception module, enabling autonomous exploration, obstacle avoidance, object recognition, and intelligent interaction with its surroundings.

---

## Project Status

**Current Stage:** Early Development (Mk II Vision)

### Completed

- Camera interface
- Live HUD
- HSV colour processing
- Colour segmentation
- Contour detection
- Bounding boxes
- Object tracking
- Steering decision logic

### In Progress

- Motion detection
- Human detection
- AI object recognition

---

Developed as part of the **ARACHNE-X Autonomous Hexapod Robot Project**.
