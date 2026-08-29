import cv2

GREEN = (0, 255, 0)
RED = (0, 0, 255)

camera = cv2.VideoCapture(0)
if not camera.isOpened():
    print("Camera not found.")
    exit()

previous_frame = None

while True:
    success, frame = camera.read()
    if not success:
        break

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    gray = cv2.GaussianBlur(gray, (21, 21), 0)
    if previous_frame is None:
        previous_frame = gray
        continue
    difference = cv2.absdiff(previous_frame, gray)
    _, threshold = cv2.threshold(
        difference,
        25,
        255,
        cv2.THRESH_BINARY
    )
    contours, _ = cv2.findContours(
        threshold,
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
            RED,
            2
        )

    previous_frame = gray

    cv2.imshow("Motion Detection", frame)

    if cv2.waitKey(1) & 0xFF in (ord('q'), ord('Q')):
        break

camera.release()
cv2.destroyAllWindows()
