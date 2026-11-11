
#include <iostream>
#include <string>
#include <random>
#include <opencv2/opencv.hpp>

int main(int argc, char** argv)
{
    std::string path = "grayscale_tile.png";

    if (argc > 1)
        path = argv[1];

    // Load original image in BGR format
    cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);

    if (image.empty())
    {
        std::cerr << "Failed to load image: " << path << "\n";
        return 1;
    }

    std::cout << "Image: " << image.cols << " x "
              << image.rows << "\n";

    // Create black output image
    cv::Mat final_image = cv::Mat::zeros(
        image.rows, image.cols, CV_8UC3);

    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 100);

    int selected_pixels = 0;

    // Process every pixel
    for (int y = 0; y < image.rows; ++y)
    {
        for (int x = 0; x < image.cols; ++x)
        {
            int random_value = dist(gen);

            // Include pixel only if random value is even
            if (random_value % 2 == 0)
            {
                final_image.at<cv::Vec3b>(y, x) =
                    image.at<cv::Vec3b>(y, x);

                selected_pixels++;
            }
        }
    }

    std::cout << "Total pixels: " << image.total() << "\n";
    std::cout << "Selected pixels: " << selected_pixels << "\n";
    std::cout << "Removed pixels: "
              << image.total() - selected_pixels << "\n";

    // Display images
    cv::imshow("Original Image", image);
    cv::imshow("Random Selected Pixels", final_image);

    cv::waitKey(0);

    return 0;
}
