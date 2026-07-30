import cv2
import numpy as np
import time

GREEN = (0, 255, 0)
RED = (0, 0, 255)
BLUE = (255, 0, 0)

camera = cv2.VideoCapture(0)

if not camera.isOpened():
    print("Camera not found.")
    exit()

previous_time = time.time()

while True:

    success, frame = camera.read()

    if not success:
        break

    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    height, width = frame.shape[:2]

    # -------------------------
    # FPS Counter
    # -------------------------

    current_time = time.time()
    fps = 1 / (current_time - previous_time)
    previous_time = current_time

    # -------------------------
    # Crosshair
    # -------------------------

    frame_center_x = width // 2
    frame_center_y = height // 2

    lower_red = np.array([0, 120, 70])
    upper_red = np.array([10, 255, 255])

    mask = cv2.inRange(hsv, lower_red, upper_red)

    contours, hierarchy = cv2.findContours(
        mask,
        cv2.RETR_EXTERNAL,
        cv2.CHAIN_APPROX_SIMPLE
    )

    for contour in contours:

        x, y, w, h = cv2.boundingRect(contour)

        area = cv2.contourArea(contour)

        if area < 500:
             continue

        cv2.rectangle(
            frame,
            (x, y),
            (x + w, y + h),
            GREEN,
            2
        )

        center_x = x + w // 2
        center_y = y + h // 2
        error = center_x - frame_center_x
        if error > 40:
            direction = "Turn Right"
        elif error < -40:
            direction = "Turn Left"
        else:
            direction = "Go Straight"

        cv2.circle(
            frame,
            (center_x, center_y),
            5,
            GREEN,
            -1
        )

        cv2.putText(
            frame,
            f"({center_x}, {center_y})",
            (x - 10, y - 10),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            GREEN,
            2
        )

        cv2.putText(
            frame,
            f"Error: {error}",
            (20, 120),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            GREEN,
            2
        )

        cv2.putText(
            frame,
            direction,
            (20, 160),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            BLUE,
            2
        )

    cv2.line(frame, (frame_center_x-15, frame_center_y), (frame_center_x+15, frame_center_y), RED, 2)
    cv2.line(frame, (frame_center_x, frame_center_y-15), (frame_center_x, frame_center_y+15), RED, 2)
    cv2.circle(
        frame,
        (frame_center_x, frame_center_y),
        7,
        BLUE,
        -1
    )

    # -------------------------
    # Text
    # -------------------------

    cv2.putText(
        frame,
        "ARACHNE-X Mk II Vision",
        (20,40),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        GREEN,
        2
    )

    cv2.putText(
        frame,
        f"FPS: {int(fps)}",
        (width-110,40),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        GREEN,
        2
    )

    cv2.putText(
        frame,
        f"Resolution: {width} x {height}",
        (20,80),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        GREEN,
        2
    )

    cv2.putText(
        frame,
        "Status: CAMERA ONLINE",
        (width-250,height-20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        GREEN,
        2
    )

    cv2.putText(
        frame,
        "Operator: Judah",
        (20,height-20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        GREEN,
        2
    )

    cv2.imshow("Red Mask", mask)
    cv2.imshow("ARACHNE-X Mk II Vision", frame)

    if cv2.waitKey(1) & 0xFF in (ord('q'), ord('Q')):
        break

camera.release()
cv2.destroyAllWindows()