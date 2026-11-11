
#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>

int main(int argc, char **argv)
{
    std::string path = "/home/octo/Github/GlobalMap/build/grayscale_tile.png";

    if (argc > 1)
        path = argv[1];

    // Load image in grayscale mode
    cv::Mat gray = cv::imread(path, cv::IMREAD_GRAYSCALE);

    if (gray.empty())
    {
        std::cerr << "Failed to load image: " << path << "\n";
        return 1;
    }

    std::cout << "Loaded: " << path << "\n";
    std::cout << "Dimensions: "
              << gray.cols << " x " << gray.rows << "\n";
    std::cout << "Channels: " << gray.channels() << "\n";
    std::cout << "Total pixels: " << gray.total() << "\n\n";

    // Print every pixel intensity
    std::cout << "Grayscale Pixel Values:\n";

    for (int y = 0; y < gray.rows; ++y)
    {
        for (int x = 0; x < gray.cols; ++x)
        {

            cv::Vec3b pixel = gray.at<cv::Vec3b>(y, x);

            unsigned char B = pixel[0];
            unsigned char G = pixel[1];
            unsigned char R = pixel[2];

            std::cout << "[" << y << "," << x << "] "
                      << "B=" << (int)pixel[0] << " "
                      << "G=" << (int)pixel[1] << " "
                      << "R=" << (int)pixel[2] << "\n";

            uchar intensity = gray.at<uchar>(y, x);

            std::cout << "[" << y << "," << x << "] = "
                      << static_cast<int>(intensity) << "\n";
        }
    }

    cv::imshow("Loaded Grayscale Image", gray);
    cv::waitKey(0);

    return 0;
}
