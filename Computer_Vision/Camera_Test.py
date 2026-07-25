import cv2
import time

Green = (0, 255, 0)
Red = (0, 0, 255)
Blue = (255, 0, 0)

camera = cv2.VideoCapture(0)

if not camera.isOpened():
    print("Camera not found.")
    exit()

previous_time = time.time()

while True:

    success, frame = camera.read()

    if not success:
        break

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

    center_x = width // 2
    center_y = height // 2

    cv2.line(frame, (center_x-15, center_y), (center_x+15, center_y), Red, 2)
    cv2.line(frame, (center_x, center_y-15), (center_x, center_y+15), Red, 2)

    # -------------------------
    # Text
    # -------------------------

    cv2.putText(
        frame,
        "ARACHNE-X Mk I Vision",
        (20,40),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        Green,
        2
    )

    cv2.putText(
        frame,
        f"FPS: {int(fps)}",
        (width-110,40),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        Green,
        2
    )

    cv2.putText(
        frame,
        f"Resolution: {width} x {height}",
        (20,80),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        Green,
        2
    )

    cv2.putText(
        frame,
        "Status: CAMERA ONLINE",
        (390,height-20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        Green,
        2
    )

    cv2.putText(
        frame,
        "Operator: Judah",
        (20,height-20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        Green,
        2
    )

    cv2.imshow("ARACHNE-X Mk I Vision", frame)

    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

camera.release()
cv2.destroyAllWindows()
