#include "unet_module.h"
#include <iostream>

UNetSegmenter::UNetSegmenter(const std::string& model_path) {
    try {
        model = torch::jit::load(model_path);
        model.eval();
    } catch (const c10::Error& e) {
        std::cerr << "Ошибка загрузки модели: " << e.what() << std::endl;
        throw;
    }
}

cv::Mat UNetSegmenter::predict(const cv::Mat& image_rgb) {
    if (image_rgb.empty() || image_rgb.channels() != 3) {
        throw std::invalid_argument("Input image must be a non-empty 3-channel RGB image");
    }

    cv::Mat resized;
    cv::resize(image_rgb, resized, cv::Size(512, 512));
    cv::Mat img_float;
    resized.convertTo(img_float, CV_32FC3, 1.0 / 255.0);

    // Нормализация (как при обучении)
    std::vector<cv::Mat> channels(3);
    cv::split(img_float, channels);
    channels[0] = (channels[0] - 0.485f) / 0.229f;
    channels[1] = (channels[1] - 0.456f) / 0.224f;
    channels[2] = (channels[2] - 0.406f) / 0.225f;
    cv::merge(channels, img_float);

    auto input_tensor = torch::from_blob(
        img_float.data,
        {1, 512, 512, 3},
        torch::kFloat32
    ).permute({0, 3, 1, 2});

    torch::NoGradGuard no_grad;
    auto output = model.forward({input_tensor}).toTensor();
    output = torch::sigmoid(output);
    auto mask = output.squeeze().detach().cpu();

    cv::Mat mask_cv(512, 512, CV_32F, mask.data_ptr());
    cv::Mat binary_mask;
    cv::threshold(mask_cv, binary_mask, 0.5, 255, cv::THRESH_BINARY);
    binary_mask.convertTo(binary_mask, CV_8U);
    return binary_mask;
}
