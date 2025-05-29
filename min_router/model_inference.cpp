#include "model_inference.h"
#include <iostream>

ModelInference::ModelInference(const std::string& model_path) {
    try {
        model = torch::jit::load(model_path);
        model.eval();
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading model: " << e.what() << std::endl;
    }
}

ModelInference::~ModelInference() {}

cv::Mat ModelInference::create_mask(const std::vector<cv::Mat>& spectrums) {
    if (spectrums.size() != 3) {
        std::cerr << "Expected 3 RGB spectrums" << std::endl;
        return cv::Mat();
    }

    try {
        // Предобработка входных данных
        torch::Tensor input = preprocess(spectrums);
        
        // Инференс модели
        std::vector<torch::jit::IValue> inputs;
        inputs.push_back(input);
        auto output = model.forward(inputs).toTensor();
        
        // Постобработка результата
        output = torch::sigmoid(output);
        output = (output > 0.5).to(torch::kFloat32);
        
        // Преобразование в OpenCV Mat
        return tensor_to_mat(output);
    }
    catch (const std::exception& e) {
        std::cerr << "Error during inference: " << e.what() << std::endl;
        return cv::Mat();
    }
}

torch::Tensor ModelInference::mat_to_tensor(const cv::Mat& image) {
    // Преобразование BGR в RGB
    cv::Mat rgb;
    cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    
    // Нормализация и преобразование в float
    cv::Mat float_img;
    rgb.convertTo(float_img, CV_32F, 1.0/255.0);
    
    // Преобразование в torch::Tensor
    auto tensor = torch::from_blob(float_img.data, {1, float_img.rows, float_img.cols, 3});
    tensor = tensor.permute({0, 3, 1, 2}); // NCHW формат
    
    return tensor.clone();
}

cv::Mat ModelInference::tensor_to_mat(const torch::Tensor& tensor) {
    // Преобразование из NCHW в NHWC
    auto permuted = tensor.permute({0, 2, 3, 1});
    
    // Преобразование в OpenCV Mat
    cv::Mat mask(tensor.size(2), tensor.size(3), CV_32F);
    std::memcpy(mask.data, permuted.data_ptr(), permuted.numel() * sizeof(float));
    
    // Преобразование в uint8
    cv::Mat mask_uint8;
    mask.convertTo(mask_uint8, CV_8U, 255.0);
    
    return mask_uint8;
}

torch::Tensor ModelInference::preprocess(const std::vector<cv::Mat>& spectrums) {
    std::vector<torch::Tensor> tensors;
    for (const auto& spectrum : spectrums) {
        tensors.push_back(mat_to_tensor(spectrum));
    }
    
    // Объединение каналов
    return torch::cat(tensors, 1);
} 