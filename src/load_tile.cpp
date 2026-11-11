// #include <fstream>
// #include <iostream>
// #include <string>
// #include <vector>
// #include <opencv2/opencv.hpp>
// #include <turbojpeg.h>

// // Decode a JPEG tile with libjpeg-turbo straight into a BGR cv::Mat.
// static cv::Mat loadTile(tjhandle tj, const std::string &path)
// {
//     std::ifstream f(path, std::ios::binary | std::ios::ate);
//     if (!f)
//         return {};
//     std::vector<unsigned char> buf(static_cast<size_t>(f.tellg()));
//     f.seekg(0);
//     f.read(reinterpret_cast<char *>(buf.data()), buf.size());

//     int w, h, subsamp, colorspace;
//     if (tjDecompressHeader3(tj, buf.data(), buf.size(), &w, &h, &subsamp, &colorspace) != 0)
//         return {};

//     cv::Mat tile(h, w, CV_8UC3);
//     if (tjDecompress2(tj, buf.data(), buf.size(), tile.data, w, 0, h,
//                       TJPF_BGR, TJFLAG_FASTDCT) != 0)
//         return {};
//     return tile;
// }

// int main(int argc, char **argv)
// {
//     std::string path = "/home/octo/Github/GlobalMap/build/data/map_delhi/tiles/19/374580/218615.jpg";
//     if (argc > 1)
//         path = argv[1];

//     tjhandle tj = tjInitDecompress();
//     cv::Mat tile = loadTile(tj, path);
//     if (tile.empty())
//     {
//         std::cerr << "Failed to load tile: " << path << " (" << tjGetErrorStr2(tj) << ")" << std::endl;
//         tjDestroy(tj);
//         return 1;
//     }
//     tjDestroy(tj);

//     std::cout << "Loaded " << path << " (" << tile.cols << "x" << tile.rows
//               << ", " << tile.channels() << " channels)" << std::endl;

//     // -------------------------------------------------------
//     std::cout << "BGR Pixel Values:\n";

//     cv::Mat gray(tile.rows, tile.cols, CV_8UC1);

//     for (int y = 0; y < 10; ++y)
//     {
//         for (int x = 0; x <10; ++x)
//         {
//             // Read BGR pixel
//             cv::Vec3b pixel = tile.at<cv::Vec3b>(y, x);

//             unsigned char B = pixel[0];
//             unsigned char G = pixel[1];
//             unsigned char R = pixel[2];

//              std::cout << "[" << y << "," << x << "] "
//                       << "B=" << (int)pixel[0] << " "
//                       << "G=" << (int)pixel[1] << " "
//                       << "R=" << (int)pixel[2] << "\n";

//             // Manual grayscale conversion
//             unsigned char intensity = static_cast<unsigned char>(
//                 0.114 * B +
//                 0.587 * G +
//                 0.299 * R);

//             gray.at<uchar>(y, x) = intensity;
//         }
//     }

//     cv::imshow("Original", tile);
//     cv::imshow("Manual Grayscale", gray);

//     cv::cvtColor(tile, gray, cv::COLOR_BGR2GRAY);
//     cv::imshow("Grayscale Image", gray);

//     cv::waitKey(0);

//     for (int y = 0; y < 10; ++y)
//     {
//         for (int x = 0; x < 10; ++x)
//         {
//             // Read BGR pixel
//             cv::Vec3b pixel = gray.at<cv::Vec3b>(y, x);

//             unsigned char B = pixel[0];
//             unsigned char G = pixel[1];
//             unsigned char R = pixel[2];

//             std::cout << "[" << y << "," << x << "] "
//                       << "B=" << (int)pixel[0] << " "
//                       << "G=" << (int)pixel[1] << " "
//                       << "R=" << (int)pixel[2] << "\n";
//         }
//     }

//     return 0;
// }



#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>

#include <opencv2/opencv.hpp>
#include <turbojpeg.h>

// Decode JPEG into BGR cv::Mat
static cv::Mat loadTile(tjhandle tj, const std::string &path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return {};

    std::vector<unsigned char> buf(static_cast<size_t>(f.tellg()));

    f.seekg(0);
    f.read(reinterpret_cast<char *>(buf.data()), buf.size());

    if (!f)
        return {};

    int w, h, subsamp, colorspace;

    if (tjDecompressHeader3(tj, buf.data(), buf.size(),
                            &w, &h, &subsamp, &colorspace) != 0)
        return {};

    cv::Mat tile(h, w, CV_8UC3);

    if (tjDecompress2(tj, buf.data(), buf.size(),
                      tile.data, w, 0, h,
                      TJPF_BGR, TJFLAG_FASTDCT) != 0)
        return {};

    return tile;
}

int main(int argc, char **argv)
{
    std::string path =
        "/home/octo/Github/GlobalMap/build/data/map_delhi/tiles/19/374580/218615.jpg";

    if (argc > 1)
        path = argv[1];

    tjhandle tj = tjInitDecompress();

    if (!tj)
    {
        std::cerr << "Failed to initialize TurboJPEG\n";
        return 1;
    }

    cv::Mat tile = loadTile(tj, path);

    if (tile.empty())
    {
        std::cerr << "Failed to load tile: " << path
                  << " (" << tjGetErrorStr2(tj) << ")\n";

        tjDestroy(tj);
        return 1;
    }

    tjDestroy(tj);

    std::cout << "Loaded: " << path << "\n";
    std::cout << "Dimensions: " << tile.cols
              << " x " << tile.rows << "\n";
    std::cout << "BGR Channels: " << tile.channels() << "\n\n";

    // -------------------------------------------------------
    // 1. Original BGR image
    // -------------------------------------------------------

    // tile is CV_8UC3

    // -------------------------------------------------------
    // 2. OpenCV grayscale conversion
    // -------------------------------------------------------

    cv::Mat gray_cv;

    cv::cvtColor(tile, gray_cv, cv::COLOR_BGR2GRAY);

    // gray_cv is CV_8UC1

    // -------------------------------------------------------
    // 3. Manual grayscale conversion
    // -------------------------------------------------------

    cv::Mat gray_manual(tile.rows, tile.cols, CV_8UC1);

    for (int y = 0; y < tile.rows; ++y)
    {
        for (int x = 0; x < tile.cols; ++x)
        {
            // Original BGR pixel
            cv::Vec3b pixel = tile.at<cv::Vec3b>(y, x);

            unsigned char B = pixel[0];
            unsigned char G = pixel[1];
            unsigned char R = pixel[2];

            // Manual grayscale calculation
            double intensity =
                0.114 * B +
                0.587 * G +
                0.299 * R;

            gray_manual.at<uchar>(y, x) =
                static_cast<uchar>(std::lround(intensity));
        }
    }

    // -------------------------------------------------------
    // 4. Print all pixel values for comparison
    // -------------------------------------------------------

    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "                 PIXEL VALUE COMPARISON\n";
    std::cout << "============================================================\n";

    std::cout << std::left
              << std::setw(12) << "Coordinate"
              << std::setw(24) << "Original BGR"
              << std::setw(18) << "OpenCV Gray"
              << std::setw(18) << "Manual Gray"
              << "\n";

    std::cout << "------------------------------------------------------------\n";

    int mismatch_count = 0;
    int max_difference = 0;

    for (int y = 0; y < tile.rows; ++y)
    {
        for (int x = 0; x < tile.cols; ++x)
        {
            // Original BGR
            cv::Vec3b bgr = tile.at<cv::Vec3b>(y, x);

            // OpenCV grayscale
            uchar opencv_gray = gray_cv.at<uchar>(y, x);

            // Manual grayscale
            uchar manual_gray = gray_manual.at<uchar>(y, x);

            // Compare
            int difference = std::abs(
                static_cast<int>(opencv_gray) -
                static_cast<int>(manual_gray));

            if (difference != 0)
                mismatch_count++;

            max_difference = std::max(max_difference, difference);

            // Print all values
            std::cout << std::left
                      << std::setw(12)
                      << ("[" + std::to_string(y) + "," +
                          std::to_string(x) + "]")

                      << std::setw(24)
                      << ("[" + std::to_string(bgr[0]) + "," +
                          std::to_string(bgr[1]) + "," +
                          std::to_string(bgr[2]) + "]")

                      << std::setw(18)
                      << static_cast<int>(opencv_gray)

                      << std::setw(18)
                      << static_cast<int>(manual_gray)

                      << "\n";
        }
    }

    // -------------------------------------------------------
    // 5. Summary
    // -------------------------------------------------------

    std::cout << "\n============================================================\n";
    std::cout << "                    SUMMARY\n";
    std::cout << "============================================================\n";

    std::cout << "Original image: "
              << tile.cols << " x " << tile.rows
              << " x " << tile.channels() << "\n";

    std::cout << "OpenCV grayscale: "
              << gray_cv.cols << " x " << gray_cv.rows
              << " x " << gray_cv.channels() << "\n";

    std::cout << "Manual grayscale: "
              << gray_manual.cols << " x " << gray_manual.rows
              << " x " << gray_manual.channels() << "\n";

    std::cout << "Total pixels: "
              << tile.total() << "\n";

    std::cout << "Different pixels: "
              << mismatch_count << "\n";

    std::cout << "Maximum pixel difference: "
              << max_difference << "\n";

    // -------------------------------------------------------
    // 6. Display images
    // -------------------------------------------------------

    cv::imshow("Original BGR Image", tile);
    cv::imshow("OpenCV Grayscale", gray_cv);
    cv::imshow("Manual Grayscale", gray_manual);

    cv::waitKey(0);

    return 0;
}
