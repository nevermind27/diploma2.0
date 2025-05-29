#pragma once

#include <string>
#include <vector>
#include <torch/torch.h>
#include <opencv2/opencv.hpp>

class ModelInference {
public:
    ModelInference(const std::string& model_path);
    ~ModelInference();

    // Создание бинарной маски из RGB спектров
    cv::Mat create_mask(const std::vector<cv::Mat>& spectrums);

private:
    torch::jit::script::Module model;
    
    // Преобразование OpenCV Mat в torch::Tensor
    torch::Tensor mat_to_tensor(const cv::Mat& image);
    
    // Преобразование torch::Tensor в OpenCV Mat
    cv::Mat tensor_to_mat(const torch::Tensor& tensor);
    
    // Предобработка изображения
    torch::Tensor preprocess(const std::vector<cv::Mat>& spectrums);
}; 