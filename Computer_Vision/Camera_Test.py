import cv2

# Print OpenCV version
print(f"OpenCV Version: {cv2.__version__}")

# Open the default webcam
camera = cv2.VideoCapture(0)

if not camera.isOpened():
    print("❌ Could not open camera.")
    exit()

print("✅ Camera opened successfully!")

while True:
    ret, frame = camera.read()

    if not ret:
        print("Failed to grab frame.")
        break

    cv2.imshow("Arachne-X Vision System", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

camera.release()
cv2.destroyAllWindows()