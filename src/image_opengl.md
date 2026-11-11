Your code performs a complete pipeline:

1. Loads an image from disk using OpenCV.
2. Converts the image from OpenCV's BGR format to RGB.
3. Flips the image vertically to match OpenGL's texture-coordinate convention.
4. Uploads the image to the GPU as an OpenGL texture.
5. Draws that texture on a rectangular area using two triangles (internally represented by `GL_QUADS`).
6. Displays the rendered image inside a GLUT window.

Let's understand what happens internally, step by step.

# 1. Global variables

```
int win_w = 512;
int win_h = 512;

double view_x = 0;
double view_y = 0;

GLuint texture_id = 0;
```

| Variable     | Purpose                               |
| ------------ | ------------------------------------- |
| `win_w`      | Window width                          |
| `win_h`      | Window height                         |
| `view_x`     | Horizontal camera/view offset         |
| `view_y`     | Vertical camera/view offset           |
| `texture_id` | OpenGL identifier for the GPU texture |

Important: `texture_id` does not contain image pixels. It is simply an identifier that OpenGL uses to reference the texture stored on the GPU.

# 2. Understanding `loadTexture()`

This function loads an image and converts it into an OpenGL texture.

```
bool loadTexture(const std::string& image_path)
```

Let's break it down.

## Step 1: Load the image using OpenCV

```
cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);
```

OpenCV reads the image from your disk.

For example, suppose your image is:

[Paviljoni Slaven](https://images.openai.com/static-rsc-4/hODphTnFGhD5hxi5KMLva6wVUaTbU_hACwYZ2rp1Ljd35IldRMe00iShU_0elGV96C-BPlkZgMKY0jh-eQCX6rT4phZ30RLMl1FBlHUQAtlgOYWLiCs_ndQ7ZtoAKL3tGxUDURgat5Hcx2rN6yhqVLXe62piz33kkD0Ugg_LU0w?purpose=inline)

[integral-zagreb.hr](https://www.integral-zagreb.hr/hr/paviljoni-slaven-1?utm_source=chatgpt.com)

The resulting `cv::Mat` contains the pixel information.

For a 512 × 512 RGB image:

- Width = 512 pixels
- Height = 512 pixels
- Channels = 3 (Blue, Green, Red)

Each pixel contains three 8-bit values.

For example:

```
B = 120;
G = 150;
R = 200;
```

Why check `image.empty()`?

```
if (image.empty())
{
    std::cerr << "Failed to load image\n";
    return false;
}
```

If the file doesn't exist or OpenCV cannot decode it, the function stops.

## Step 2: Convert BGR to RGB

```
cv::Mat rgb;
cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
```

This is important.

OpenCV normally stores color images in BGR order.

OpenGL, with the `GL_RGB` upload format used here, expects RGB order.

Consider one pixel:

OpenCV representation

### 120

Blue

### 150

Green

### 200

Red

B → G → R

After conversion

### 200

Red

### 150

Green

### 120

Blue

R → G → B

Without this conversion, red and blue would be swapped.

## Step 3: Flip the image vertically

```
cv::flip(rgb, rgb, 0);
```

This is one of the most important parts.

OpenCV and OpenGL use different conventions for image coordinates.

OpenCV:

The origin (0,0) is at the top-left corner.

Your OpenGL texture mapping:

The bottom-left vertex is assigned texture coordinate (0,0).

Conceptually:

[Mastering OpenCV Part 1: A Comprehensive Guide to Computer Vision with Python | by Ebad Sayed | Medium](https://images.openai.com/static-rsc-4/lAd4ciOFKiJ4vGfpoT9IMRuV8M7WNrQioMuBrFxr7rCE8D_ZNLRqo8Z76bYB58a52yhwCo3EXuslnH2Ctmx2jiOjh8dc3O4BeFeHkcIHOaaqBp5EUYol29rWqmKGKWbsjXnxIacnLMrJvDoWjN9Y0JVQOOe-EHvHYqDVzZ483r0?purpose=inline)

OpenCV

Origin at top-left

[Lab 11 | CSCI 1230](https://images.openai.com/static-rsc-4/csq3ljQsIFXG-477i7OqGAL9XxM2AWYRe2o9qRFo000Pj2Muo2MIsvV64iH_mbZVNl1mjHAURUG1wryUaB44N1ySl5cPFXkgdR9ofifpJX2jx7Gi2gf608xJoFsdEY_DGnd7_mPNh1GnU-oPL3nLhedpgrJLl8pMg1NDusB92kM?purpose=inline)

OpenGL mapping

Origin at bottom-left

So this operation:

```
cv::flip(rgb, rgb, 0);
```

reverses the image's row order.

For example:

Before:

```
A B C     <- First row
D E F
G H I     <- Last row
```

After:

```
G H I
D E F
A B C
```

This ensures the image appears in the correct orientation when mapped using your current texture coordinates.

## Step 4: Ensure continuous memory

```
if (!rgb.isContinuous())
    rgb = rgb.clone();
```

OpenCV images are stored in memory.

Ideally, the pixel data should be laid out continuously:

```
R G B R G B R G B ...
```

However, some OpenCV operations can produce matrices with non-contiguous memory layouts.

`isContinuous()` checks whether the rows are stored without gaps.

If they are not continuous, `clone()` creates a new continuous copy.

Why?

Because you are passing:

```
rgb.data
```

directly to OpenGL.

OpenGL expects the pixel data to follow the layout described by the supplied dimensions, format and pixel-store settings.

## Step 5: Generate a GPU texture

```
glGenTextures(1, &texture_id);
```

This generates an OpenGL texture object and assigns its identifier to `texture_id`.

For example:

```
texture_id = 1;
```

The actual identifier is assigned by OpenGL.

Next:

```
glBindTexture(GL_TEXTURE_2D, texture_id);
```

This binds the texture.

Think of binding as selecting the texture that subsequent texture operations will modify.

## Step 6: Configure pixel alignment

```
glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
```

This tells OpenGL how pixel rows are aligned in the source memory.

By default, OpenGL uses 4-byte alignment.

For RGB images, each pixel occupies 3 bytes.

For example, an image of width 5 has:

\\[ 5 \times 3 = 15 \text{ bytes per row} \\]

15 is not divisible by 4.

Setting alignment to 1 avoids OpenGL assuming additional padding between rows.

This is particularly important for RGB images whose row sizes are not multiples of four.

## Step 7: Texture filtering

```
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

These parameters determine how OpenGL samples the image when rendering.

There are two situations:

| Parameter               | Meaning                                          |
| ----------------------- | ------------------------------------------------ |
| `GL_TEXTURE_MIN_FILTER` | Image is rendered smaller than its original size |
| `GL_TEXTURE_MAG_FILTER` | Image is rendered larger than its original size  |

`GL_LINEAR` means OpenGL interpolates between neighboring pixels.

For example, when enlarging an image, OpenGL calculates intermediate pixel values instead of simply repeating the nearest pixel.

This produces smoother results.

## Step 8: Texture wrapping

```
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
```

Here:

- `S` represents horizontal texture coordinates.
- `T` represents vertical texture coordinates.

`GL_CLAMP_TO_EDGE` means that if texture coordinates go outside the range [0,1], OpenGL uses the nearest edge pixels rather than repeating the texture.

This is useful for map tiles because you generally don't want the image to repeat.

## Step 9: Upload image data to GPU

This is the most important instruction in the entire function:

```
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
```

Let's understand each argument.

| Argument           | Meaning                                 |
| ------------------ | --------------------------------------- |
| `GL_TEXTURE_2D`    | Target is a 2D texture                  |
| `0`                | Mipmap level 0                          |
| `GL_RGB`           | GPU texture format                      |
| `rgb.cols`         | Image width                             |
| `rgb.rows`         | Image height                            |
| `0`                | Border (must be 0 in modern usage)      |
| `GL_RGB`           | Input pixel format                      |
| `GL_UNSIGNED_BYTE` | Each channel is an unsigned 8-bit value |
| `rgb.data`         | Pointer to CPU-side image pixels        |

What happens internally?

CPU memory

OpenCV cv::Mat (RGB pixels)

glTexImage2D()

Transfers image pixel data to OpenGL

GPU texture memory

Texture object identified by texture_id

After uploading, the GPU has its own copy of the image data. You can release the OpenCV matrix without destroying the GPU texture.

Finally:

```
glBindTexture(GL_TEXTURE_2D, 0);
```

Unbinds the texture.

# 3. Understanding `display()`

This function is responsible for rendering the image.

GLUT calls it whenever the window needs to be redrawn.

## Step 1: Clear the screen

```
glClear(GL_COLOR_BUFFER_BIT);
```

Clears the previous frame using the background color configured here:

```
glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
```

This produces a dark grey background.

## Step 2: Reset the model-view matrix

```
glMatrixMode(GL_MODELVIEW);
glLoadIdentity();
```

OpenGL uses transformation matrices to position and transform objects.

`glLoadIdentity()` resets the current model-view matrix to the identity matrix.

In other words, it removes transformations from previous rendering operations.

## Step 3: Apply view translation

```
glTranslated(-view_x, -view_y, 0);
```

This translates the entire scene.

For example:

```
view_x = 100;
view_y = 50;
```

OpenGL applies:

```
glTranslated(-100, -50, 0);
```

Your image moves left by 100 units and down by 50 units.

This is the beginning of a camera/panning system.

Currently, `view_x` and `view_y` are always zero because you haven't implemented keyboard or mouse controls.

## Step 4: Enable texture mapping

```
glEnable(GL_TEXTURE_2D);
glBindTexture(GL_TEXTURE_2D, texture_id);
```

The first instruction enables 2D texture mapping.

The second selects your uploaded image.

Now OpenGL knows which texture to use when drawing subsequent geometry.

## Step 5: Set the drawing color

```
glColor3f(1.0f, 1.0f, 1.0f);
```

This sets the vertex color to white.

Why white?

OpenGL modulates the texture color with the current drawing color.

Conceptually:

\\[ C\_{\text{final}}=C\_{\text{texture}}\times C\_{\text{vertex}} \\]

Since white is (1,1,1), the original image colors remain unchanged.

If you set:

```
glColor3f(1.0f, 0.0f, 0.0f);
```

the texture would be modulated with red, suppressing its green and blue channels.

## Step 6: Draw the rectangle

This is where your image actually appears.

```
glBegin(GL_QUADS);
```

You are asking OpenGL to draw a quadrilateral using four vertices.

Your rectangle has these coordinates:

## Rectangle geometry

Schematic: rectangle from (128,128) to (384,384), with a 256 × 256 display area.

You specify each vertex and its corresponding texture coordinate.

Vertex 1: Bottom-left

```
glTexCoord2f(0.0f, 0.0f);
glVertex2f(128.0f, 128.0f);
```

This means:

- Take the bottom-left corner of the texture.
- Place it at coordinate (128,128).

Vertex 2: Bottom-right

```
glTexCoord2f(1.0f, 0.0f);
glVertex2f(384.0f, 128.0f);
```

Take the bottom-right corner of the texture and place it at (384,128).

Vertex 3: Top-right

```
glTexCoord2f(1.0f, 1.0f);
glVertex2f(384.0f, 384.0f);
```

Vertex 4: Top-left

```
glTexCoord2f(0.0f, 1.0f);
glVertex2f(128.0f, 384.0f);
```

The relationship is:

| Texture coordinate | Rectangle position |
| ------------------ | ------------------ |
| (0,0)              | (128,128)          |
| (1,0)              | (384,128)          |
| (1,1)              | (384,384)          |
| (0,1)              | (128,384)          |

Texture coordinates range from 0 to 1, regardless of the actual image resolution.

This is how OpenGL knows which image pixels should appear at which rectangle positions.

What happens internally?

OpenGL rasterizes the rectangle into fragments (potential pixels). For each fragment, it interpolates the texture coordinates and samples the texture to determine the final color.

Finally:

```
glEnd();
```

Ends the geometry definition.

## Step 7: Clean up and display

```
glBindTexture(GL_TEXTURE_2D, 0);
glDisable(GL_TEXTURE_2D);

glutSwapBuffers();
```

The first two instructions unbind and disable texture mapping.

The last instruction swaps the front and back buffers.

You initialized GLUT with:

```
glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
```

This enables double buffering.

- Back buffer: where OpenGL draws the new frame.
- Front buffer: what the user currently sees.

`glutSwapBuffers()` displays the newly rendered frame.

# 4. Understanding `reshape()`

```
void reshape(int width, int height)
```

GLUT calls this function when the window is created or resized.

First:

```
win_w = width;
win_h = height;
```

Stores the new window dimensions.

Next:

```
glViewport(0, 0, width, height);
```

Defines the area of the window where OpenGL rendering occurs.

Then:

```
glMatrixMode(GL_PROJECTION);
glLoadIdentity();

glOrtho(0, 512, 0, 512, -1, 1);
```

This is important.

`glOrtho()` defines an orthographic projection.

It establishes a coordinate system where:

- X ranges from 0 to 512.
- Y ranges from 0 to 512.
- Z ranges from -1 to 1.

This means you can position objects using coordinates that resemble screen pixels.

For example:

```
glVertex2f(100, 100);
```

places a vertex at approximately pixel coordinate (100,100) in the original 512 × 512 viewport.

One limitation in your current implementation:

Although the window can be resized, your projection always remains 512 × 512.

Therefore, when the window becomes 1024 × 1024, your coordinate system does not automatically scale to the new dimensions.

Also, if the aspect ratio changes, the displayed image can stretch.

# 5. Understanding `main()`

This is the application initialization sequence.

```
glutInit(&argc, argv);
```

Initializes GLUT.

```
glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
```

Configures double buffering and RGB color output.

```
glutInitWindowSize(win_w, win_h);
```

Sets the initial window size to 512 × 512.

```
glutCreateWindow("OpenGL RGB Image");
```

Creates the window and establishes an OpenGL context.

This is important because OpenGL operations such as `glGenTextures()` require a valid OpenGL context.

Then:

```
glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
```

Sets the background color.

Next:

```
std::string image_path =
    (argc > 1) ? argv[1] :
    "/home/octo/Github/GlobalMap/build/grayscale_tile.png";
```

This is a ternary operator.

It means:

- If the user provides an image path through command-line arguments, use it.
- Otherwise, load the default image.

For example:

```
./image_viewer /home/user/map.png
```

Finally:

```
if (!loadTexture(image_path))
    return 1;
```

Loads the image and uploads it to the GPU.

Then:

```
glutDisplayFunc(display);
glutReshapeFunc(reshape);
```

Registers callback functions.

| Callback    | Responsibility         |
| ----------- | ---------------------- |
| `display()` | Draw the image         |
| `reshape()` | Handle window resizing |

Finally:

```
glutMainLoop();
```

Starts the GLUT event loop.

The application now waits for events and invokes callbacks when necessary.

# 6. Complete execution flow

This is the actual sequence of operations in your program.

1\. Initialize GLUT

Create window and OpenGL context

2\. Load image

OpenCV reads image into cv::Mat

3\. Image preprocessing

BGR → RGB → Vertical flip → Continuous memory

4\. Upload texture

glGenTextures → glBindTexture → glTexImage2D

5\. Enter GLUT event loop

6\. display()

Clear screen → Apply view transform → Bind texture

7\. Render rectangle

Interpolate texture coordinates and sample pixels

8\. Swap buffers

Display rendered frame

The event loop repeats the rendering process whenever GLUT requests a redraw.

# 7. What I would improve next

Since you're working toward displaying map tiles, I would recommend developing this code incrementally.

| Feature            | Why it matters                                   |
| ------------------ | ------------------------------------------------ |
| Mouse drag         | Pan around the map                               |
| Mouse scroll       | Zoom in/out                                      |
| Dynamic projection | Support different window sizes                   |
| Multiple textures  | Display multiple map tiles                       |
| Texture caching    | Avoid repeatedly uploading the same tiles        |
| Tile positioning   | Place tiles using world/map coordinates          |
| Texture cleanup    | Release GPU resources using `glDeleteTextures()` |

One important architectural distinction for your future map viewer:

Image loading and rendering should be separate responsibilities.

For example:

```
ImageLoader
    |
    | OpenCV reads image
    |
TextureManager
    |
    | Uploads and manages GPU textures
    |
MapRenderer
    |
    | Positions and draws map tiles
    |
OpenGL Window
```

This will make it much easier to extend your application from one image to a complete map viewer with thousands of tiles.

My recommendation: Before implementing multiple tiles, understand three concepts thoroughly: OpenGL coordinate systems, texture coordinates (UV mapping), and transformations (translation, scaling and rotation). These three concepts form the foundation of a 2D map renderer.