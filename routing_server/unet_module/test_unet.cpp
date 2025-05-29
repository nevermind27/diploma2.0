#include "unet_module.h"
#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    try {
        UNetSegmenter segmenter("unet_resnet34_scripted.pt");

        cv::Mat image = cv::imread("/Users/elizavetaomelcenko/AerialImageDataset/test/images/innsbruck28.tif");
        if (image.empty()) {
            std::cerr << "Failed to load image\n";
            return 1;
        }

        cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
        cv::Mat mask = segmenter.predict(image);

        cv::imwrite("mask_cpp.png", mask);
        std::cout << "Mask saved to mask_cpp.png\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
