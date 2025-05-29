// unet_module.h
#ifndef UNET_MODULE_H
#define UNET_MODULE_H

#include <string>
#include <opencv2/opencv.hpp>
#include <torch/script.h>

class UNetSegmenter {
public:
    UNetSegmenter(const std::string& model_path);
    cv::Mat predict(const cv::Mat& image_rgb);

private:
    torch::jit::script::Module model;
};

#endif // UNET_MODULE_H

