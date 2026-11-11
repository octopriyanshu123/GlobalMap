// #include <iostream>
// #include <GL/glut.h>
// #include <algorithm>

// int win_w = 512, win_h = 512;
// double view_x = 0, view_y = 0; 


// void display()
// {
//     glClear(GL_COLOR_BUFFER_BIT);
//     glLoadIdentity();
//     glTranslated(-view_x, -view_y, 0);

//     // Every held tile inside the window.
//     glEnable(GL_TEXTURE_2D);
//     glColor3f(1, 1, 1);

//     glDisable(GL_TEXTURE_2D);


//     glColor3f(0.1f, 0.0f, 0.0f);

//     glBegin(GL_QUADS);
//     glVertex2f(128.0f, 128.0f); 
//     glVertex2f(384.0f, 128.0f); 
//     glVertex2f(384.0f, 384.0f); 
//     glVertex2f(128.0f, 384.0f); 
//     glEnd();

//     glutSwapBuffers();
// }



// int main(int argc, char **argv)
// {
//     glutInit(&argc, argv);

//     glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
//     glutInitWindowSize(win_w, win_h);
//     glutCreateWindow("map_ui");
//     glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
//     glutDisplayFunc(display);
//     glutMainLoop();
//     return 0;
// }



#include <iostream>
#include <string>

#include <GL/glut.h>
#include <opencv2/opencv.hpp>

int win_w = 512;
int win_h = 512;

double view_x = 0;
double view_y = 0;

GLuint texture_id = 100;

// Load image and create OpenGL texture
bool loadTexture(const std::string& image_path)
{
    // Load image using OpenCV (BGR format)
    cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);

    if (image.empty())
    {
        std::cerr << "Failed to load image: " << image_path << '\n';
        return false;
    }

    // Convert BGR to RGB
    cv::Mat rgb;
    cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);

    // OpenCV starts at top-left; flip for OpenGL texture coordinates
    cv::flip(rgb, rgb, 0);

    // Ensure continuous memory
    if (!rgb.isContinuous())
        rgb = rgb.clone();

    // Generate OpenGL texture
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Texture filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Texture wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload RGB pixel data to GPU
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        rgb.cols,
        rgb.rows,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        rgb.data
    );

    glBindTexture(GL_TEXTURE_2D, 100);

    std::cout << "Image loaded: " << rgb.cols << " x " << rgb.rows << '\n';

    return true;
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslated(-view_x, -view_y, 0);

    // Enable texture
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Keep original image colors
    glColor3f(1.0f, 1.0f, 1.0f);

    // Draw image inside rectangle
    glBegin(GL_QUADS);

        // Bottom-left
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(128.0f, 128.0f);

        // Bottom-right
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(384.0f, 128.0f);

        // Top-right
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(384.0f, 384.0f);

        // Top-left
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(128.0f, 384.0f);

    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    glutSwapBuffers();
}

void reshape(int width, int height)
{
    win_w = width;
    win_h = height;

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Use pixel-like window coordinates
    glOrtho(0, 512, 0, 512, -1, 1);

    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(win_w, win_h);

    glutCreateWindow("OpenGL RGB Image");

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    std::string image_path = (argc > 1) ? argv[1] : "/home/octo/Github/GlobalMap/build/grayscale_tile.png";

    if (!loadTexture(image_path))
        return 1;

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);

    glutMainLoop();

    return 0;
}
