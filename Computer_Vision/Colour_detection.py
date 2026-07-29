import cv2
import numpy as np

RED = (0, 0, 255)
GREEN = (0, 255, 0)
BLUE = (255, 0, 0)

camera = cv2.VideoCapture(0)

while True:

    success, frame = camera.read()

    if not success:
        break

    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    height, width = frame.shape[:2]

    frame_center_x = width // 2
    frame_center_y = height // 2

    # Lower and Upper HSV values for red
    lower_red = np.array([0, 120, 70])
    upper_red = np.array([10, 255, 255])

    # Create the mask
    mask = cv2.inRange(hsv, lower_red, upper_red)

    contours, hierarchy = cv2.findContours(
        mask,
        cv2.RETR_EXTERNAL,
        cv2.CHAIN_APPROX_SIMPLE
    )

    for contour in contours:

        x,y, w, h = cv2.boundingRect(contour)

        area = cv2.contourArea(contour)

        if area < 500:
            continue

        cv2.rectangle(
            frame,
            (x, y),
            (x + w, y + h),
            (0, 255, 0),
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
            (0, 0, 255),
            -1
        )

        cv2.putText(
            frame,
            f"({center_x}, {center_y})",
            (x, y - 10),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            (0, 255, 0),
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
            RED,
            2
        )
        
    cv2.circle(
            frame,
            (frame_center_x, frame_center_y),
            6,
            BLUE,
            -1
        )

    cv2.imshow("Original", frame)
    cv2.imshow("Red Mask", mask)

    if cv2.waitKey(1) & 0xFF in (ord('q'), ord('Q')):
        break

camera.release()
cv2.destroyAllWindows()