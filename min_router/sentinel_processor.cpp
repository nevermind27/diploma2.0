#include "sentinel_processor.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

SentinelProcessor::SentinelProcessor() {
    archive_path = "sentinel2_archive/";
}

SentinelProcessor::~SentinelProcessor() {}

std::vector<cv::Mat> SentinelProcessor::get_spectrums(const std::string& image_id) {
    std::vector<cv::Mat> spectrums;
    std::string image_path = archive_path + image_id + "/";
    
    if (!fs::exists(image_path)) {
        std::cerr << "Image directory not found: " << image_path << std::endl;
        return spectrums;
    }

    // Получаем RGB спектры
    return extract_rgb_spectrums(image_id);
}

std::vector<cv::Mat> SentinelProcessor::extract_rgb_spectrums(const std::string& image_id) {
    std::vector<cv::Mat> rgb_spectrums;
    std::string image_path = archive_path + image_id + "/";
    
    // Загружаем B02 (синий), B03 (зеленый), B04 (красный)
    std::vector<std::string> spectrum_files = {
        image_path + "B02.jp2",
        image_path + "B03.jp2",
        image_path + "B04.jp2"
    };

    for (const auto& file : spectrum_files) {
        cv::Mat spectrum = load_spectrum(file);
        if (!spectrum.empty()) {
            rgb_spectrums.push_back(spectrum);
        }
    }

    return rgb_spectrums;
}

cv::Mat SentinelProcessor::load_spectrum(const std::string& spectrum_path) {
    if (!fs::exists(spectrum_path)) {
        std::cerr << "Spectrum file not found: " << spectrum_path << std::endl;
        return cv::Mat();
    }

    cv::Mat spectrum = cv::imread(spectrum_path, cv::IMREAD_UNCHANGED);
    if (spectrum.empty()) {
        std::cerr << "Failed to load spectrum: " << spectrum_path << std::endl;
        return cv::Mat();
    }

    return spectrum;
} 