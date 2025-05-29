#pragma once

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

class SentinelProcessor {
public:
    SentinelProcessor();
    ~SentinelProcessor();

    // Получение спектров из архива Sentinel-2
    std::vector<cv::Mat> get_spectrums(const std::string& image_id);
    
    // Извлечение RGB спектров (B02, B03, B04)
    std::vector<cv::Mat> extract_rgb_spectrums(const std::string& image_id);

private:
    // Путь к архиву Sentinel-2
    std::string archive_path;
    
    // Загрузка отдельного спектра
    cv::Mat load_spectrum(const std::string& spectrum_path);
}; 