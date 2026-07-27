import cv2

RED = (0, 0, 255)
BLUE = (255, 0, 0)
GREEN = (0, 255, 0)
YELLOW = (0, 255, 255)
KILL = ("Q") or ("q")

camera = cv2.VideoCapture(0)

while True:

    success, frame = camera.read()

    if not success:
        break

    # Find the centre of the image
    height, width = frame.shape[:2]

    center_x = width // 2
    center_y = height // 2

    # Read the colour of the centre pixel
    pixel = frame[center_y, center_x]

    print(pixel)

    # Draw a small circle and crosshair so we know which pixel we're reading

    b,g,r = pixel

    cv2.line(frame, (center_x-15, center_y), (center_x+15, center_y), RED, 2)
    cv2.line(frame, (center_x, center_y-15), (center_x, center_y+15), RED, 2)
    cv2.circle(frame, (center_x, center_y), 7, YELLOW, -1)

    cv2.putText(
        frame,
        f"Centre Pixel: B={b} G={g} R={r}",
        (width-320,height-20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (255,0,0),
        2
    )

    cv2.imshow("Pixel Reader", frame)

    if cv2.waitKey(1) & 0xFF == ord(KILL):
        break

camera.release()
cv2.destroyAllWindows()
