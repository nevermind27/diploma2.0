import torch
import segmentation_models_pytorch as smp

# Создание модели — та же архитектура, что в обучении
model = smp.Unet(
    encoder_name="resnet34",
    encoder_weights=None,  # при экспорте не нужны
    in_channels=3,
    classes=1,
    activation=None,
)

# Загрузка весов
model.load_state_dict(torch.load("best_unet_resnet34.pth", map_location="cpu"))
model.eval()

# Тестовый ввод
example = torch.rand(1, 3, 512, 512)

# Экспорт в TorchScript
traced = torch.jit.trace(model, example)
traced.save("unet_resnet34_scripted.pt")
