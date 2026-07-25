import cv2

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

    # Draw a small circle so we know which pixel we're reading
    cv2.circle(frame, (center_x, center_y), 5, (0, 255, 0), -1)

    cv2.imshow("Pixel Reader", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

camera.release()
cv2.destroyAllWindows()