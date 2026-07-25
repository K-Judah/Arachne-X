import cv2
import time

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

    cv2.line(frame, (center_x-20, center_y), (center_x+20, center_y), (0,255,0), 2)
    cv2.line(frame, (center_x, center_y-20), (center_x, center_y+20), (0,255,0), 2)

    # -------------------------
    # Text
    # -------------------------

    cv2.putText(
        frame,
        "ARACHNE-X VISION SYSTEM",
        (20,40),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        (0,255,0),
        2
    )

    cv2.putText(
        frame,
        f"FPS: {int(fps)}",
        (20,80),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (0,255,0),
        2
    )

    cv2.putText(
        frame,
        f"Resolution: {width} x {height}",
        (20,120),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (0,255,0),
        2
    )

    cv2.putText(
        frame,
        "Status: CAMERA ONLINE",
        (20,height-20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (0,255,0),
        2
    )

    cv2.imshow("Arachne-X Vision System", frame)

    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

camera.release()
cv2.destroyAllWindows()
