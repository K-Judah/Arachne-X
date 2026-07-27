import cv2

camera = cv2.VideoCapture(0)

RED = (0, 0, 255)
BLUE = (255, 0, 0)
GREEN = (0, 255, 0)
YELLOW = (0, 255, 255)

while True:

    success, frame = camera.read()

    if not success:
        break

    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    height, width = hsv.shape[:2]

    center_x = width // 2
    center_y = height // 2

    pixel = hsv[center_y, center_x]
    h,s,v = pixel

    cv2.putText(
        hsv,
        f"HSV : H = {h}, S = {s}, V = {v}",
        (20, height - 20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        YELLOW,
        2
    )
    cv2.line(hsv, (center_x - 15, center_y), (center_x + 15, center_y), YELLOW, 2)
    cv2.line(hsv, (center_x, center_y - 15), (center_x, center_y + 15), YELLOW, 2)
    cv2.circle(hsv, (center_x, center_y), 7, YELLOW, -1)

    cv2.imshow("Original", frame)
    cv2.imshow("HSV", hsv)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

camera.release()
cv2.destroyAllWindows()