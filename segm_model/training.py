import os
import glob
import torch
import torch.nn as nn
import torch.optim as optim
import cv2
import albumentations as A
from torch.utils.data import Dataset, DataLoader
import segmentation_models_pytorch as smp

device = torch.device('mps' if torch.backends.mps.is_available() else 'cpu')
print(f"Using device: {device}")

BASE_DIR = "/Users/elizavetaomelcenko/AerialImageDataset"
TRAIN_IMG_DIR = os.path.join(BASE_DIR, "train/images")
TRAIN_MASK_DIR = os.path.join(BASE_DIR, "train/gt")
VAL_IMG_DIR   = os.path.join(BASE_DIR, "val/images")
VAL_MASK_DIR  = os.path.join(BASE_DIR, "val/gt")

train_transform = A.Compose([
    A.RandomCrop(512, 512),
    A.HorizontalFlip(p=0.5),
    A.VerticalFlip(p=0.5),
    A.RandomBrightnessContrast(p=0.2),
    A.Normalize(),
])
val_transform = A.Compose([
    A.CenterCrop(512, 512),
    A.Normalize(),
])

class AerialDataset(Dataset):
    def __init__(self, img_dir, mask_dir=None, transforms=None):
        self.img_paths  = sorted(glob.glob(os.path.join(img_dir, "*")))
        self.mask_paths = sorted(glob.glob(os.path.join(mask_dir, "*"))) if mask_dir else None
        self.tf = transforms

    def __len__(self):
        return len(self.img_paths)

    def __getitem__(self, idx):
        img = cv2.imread(self.img_paths[idx], cv2.IMREAD_COLOR)
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        mask = None
        if self.mask_paths:
            m = cv2.imread(self.mask_paths[idx], cv2.IMREAD_GRAYSCALE)
            mask = (m > 127).astype("float32")
        if self.tf:
            augmented = self.tf(image=img, mask=mask) if mask is not None else self.tf(image=img)
            img, mask = augmented["image"], augmented["mask"] if mask is not None else None
        img = img.transpose(2, 0, 1)
        img = torch.from_numpy(img).float()
        if mask is not None:
            mask = torch.from_numpy(mask).unsqueeze(0).float()
            return img, mask
        return img

def main():
    train_ds = AerialDataset(TRAIN_IMG_DIR, TRAIN_MASK_DIR, train_transform)
    val_ds   = AerialDataset(VAL_IMG_DIR,   VAL_MASK_DIR,   val_transform)
    train_loader = DataLoader(train_ds, batch_size=8, shuffle=True,  num_workers=4)
    val_loader   = DataLoader(val_ds,   batch_size=8, shuffle=False, num_workers=2)

    model = smp.Unet(
        encoder_name="resnet34",
        encoder_weights="imagenet",
        in_channels=3,
        classes=1,
        activation="sigmoid",
    ).to(device)

    criterion = smp.losses.DiceLoss(mode="binary")
    optimizer = optim.Adam(model.parameters(), lr=1e-4)

    NUM_EPOCHS = 20
    best_iou = 0.0

    for epoch in range(1, NUM_EPOCHS + 1):
        model.train()
        train_loss = 0.0
        for imgs, masks in train_loader:
            imgs, masks = imgs.to(device), masks.to(device)
            preds = model(imgs)
            loss  = criterion(preds, masks)
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * imgs.size(0)
        train_loss /= len(train_loader.dataset)

        model.eval()
        total_iou = 0.0
        with torch.no_grad():
            for imgs, masks in val_loader:
                imgs, masks = imgs.to(device), masks.to(device)
                preds = (model(imgs) > 0.5).float()
                intersection = (preds * masks).sum((1,2,3))
                union        = (preds + masks - preds * masks).sum((1,2,3)) + 1e-6
                total_iou   += (intersection / union).sum().item()
        mean_iou = total_iou / len(val_loader.dataset)

        print(f"Epoch {epoch:02d} | Train Loss: {train_loss:.4f} | Val IoU: {mean_iou:.4f}")

        if mean_iou > best_iou:
            best_iou = mean_iou
            torch.save(model.state_dict(), "best_unet_resnet34.pth")

    print("Training complete. Best Val IoU:", best_iou)

if __name__ == '__main__':
    main()
