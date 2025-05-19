import torch
import cv2
import numpy as np
import matplotlib.pyplot as plt
import segmentation_models_pytorch as smp
from albumentations import Compose, Normalize, CenterCrop
from torch.nn.functional import sigmoid

# --- Настройка устройства ---
device = torch.device("mps" if torch.backends.mps.is_available() else "cpu")
print(f"Using device: {device}")

# --- Загрузка модели ---
model = smp.Unet(
    encoder_name="resnet34",
    encoder_weights="imagenet",
    in_channels=3,
    classes=1,
    activation=None,  # без sigmoid здесь
)
model.load_state_dict(torch.load("best_unet_resnet34.pth", map_location=device))
model.to(device)
model.eval()

# --- Трансформация изображения ---
transform = Compose([
    CenterCrop(512, 512),
    Normalize(mean=(0.485, 0.456, 0.406),
              std=(0.229, 0.224, 0.225)),
])

def preprocess_image(image_path):
    img = cv2.imread(image_path)
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    augmented = transform(image=img)
    img = augmented["image"]
    img = img.transpose(2, 0, 1)  # HWC → CHW
    tensor = torch.from_numpy(img).unsqueeze(0).float().to(device)
    return tensor, img

# --- Предсказание ---
def predict(image_path):
    tensor, orig_img = preprocess_image(image_path)
    with torch.no_grad():
        output = model(tensor)
        mask = sigmoid(output)[0, 0].cpu().numpy()
    return orig_img.transpose(1, 2, 0), mask  # CHW → HWC

# --- Главная функция ---
if __name__ == '__main__':
    image_path = "/Users/elizavetaomelcenko/AerialImageDataset/test/images/innsbruck28.tif"  # поменяй на свой путь

    image, mask = predict(image_path)

    # Визуализация
    plt.figure(figsize=(12, 5))
    plt.subplot(1, 2, 1)
    plt.title("Original Image")
    plt.imshow(image)

    plt.subplot(1, 2, 2)
    plt.title("Predicted Mask")
    plt.imshow(mask, cmap="gray")

    plt.tight_layout()
    plt.show()

    # (опционально) сохранить бинарную маску
    binary_mask = (mask > 0.5).astype(np.uint8) * 255
    cv2.imwrite("predicted_mask.png", binary_mask)
