// Tile grid drawn with OpenGL/GLUT: one 256 x 256 px box per map tile, labelled with its x/y.
//
// Controls: drag with the left mouse button or use the arrow keys to move; the tile under
// the mouse has its image loaded and its x/y shown in the top-left corner. A loaded tile is
// held, so it stays on screen after the mouse moves on.
//
// Usage:
//   ./map_ui [map_dir]      (default data/map_delhi)

#include <GL/glut.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>

namespace {

constexpr long kX0 = 374513, kX1 = 374662;  // tile x range, inclusive
constexpr long kY0 = 218548, kY1 = 218697;  // tile y range, inclusive
constexpr int kCell = 256;                  // px per tile
constexpr int kCols = kX1 - kX0 + 1, kRows = kY1 - kY0 + 1;

int win_w = 1024, win_h = 768;
double view_x = 0, view_y = 0;  // grid pixel at the window's top-left corner
int mouse_x = -1, mouse_y = -1, drag_x = 0, drag_y = 0;
bool dragging = false;

std::string map_dir = "data/map_delhi";

// Per-cell flag: once a tile is loaded it is held (kept and drawn) until the program exits.
enum TileState : unsigned char { kNotLoaded, kLoaded, kFailed };
std::vector<TileState> tile_state(kCols * kRows, kNotLoaded);
std::vector<GLuint> tile_tex(kCols * kRows, 0);

// Loads the image of cell (c, r) the first time it is hovered; later calls do nothing.
void loadTile(int c, int r) {
  const int i = r * kCols + c;
  if (tile_state[i] != kNotLoaded) return;
  const std::string path = map_dir + "/tiles/19/" + std::to_string(kX0 + c) + "/" + std::to_string(kY0 + r) + ".jpg";
  const cv::Mat img = cv::imread(path, cv::IMREAD_COLOR);
  if (img.empty()) {
    tile_state[i] = kFailed;
    std::fprintf(stderr, "cannot read %s\n", path.c_str());
    return;
  }
  tile_state[i] = kLoaded;
  
  glGenTextures(1, &tile_tex[i]);
  glBindTexture(GL_TEXTURE_2D, tile_tex[i]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.cols, img.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, img.data);
}

void drawText(double x, double y, const char* s) {
  glRasterPos2d(x, y);
  for (; *s; ++s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s);
}

void clampView() {
  view_x = std::clamp(view_x, 0.0, std::max(0.0, double(kCols * kCell - win_w)));
  view_y = std::clamp(view_y, 0.0, std::max(0.0, double(kRows * kCell - win_h)));
}

void display() {
  glClear(GL_COLOR_BUFFER_BIT);
  glLoadIdentity();
  glTranslated(-view_x, -view_y, 0);

  // Only the cells inside the window.
  const int c0 = int(view_x) / kCell, c1 = std::min(kCols - 1, int(view_x + win_w) / kCell);
  const int r0 = int(view_y) / kCell, r1 = std::min(kRows - 1, int(view_y + win_h) / kCell);

  const int hc = int(view_x + mouse_x) / kCell, hr = int(view_y + mouse_y) / kCell;
  const bool hover = mouse_x >= 0 && hc < kCols && hr < kRows;
  if (hover) loadTile(hc, hr);

  // Every held tile inside the window.
  glEnable(GL_TEXTURE_2D);
  glColor3f(1, 1, 1);
  for (int r = r0; r <= r1; ++r) {
    for (int c = c0; c <= c1; ++c) {
      if (tile_state[r * kCols + c] != kLoaded) continue;
      const double l = c * kCell, t = r * kCell, rgt = l + kCell, b = t + kCell;
      glBindTexture(GL_TEXTURE_2D, tile_tex[r * kCols + c]);
      glBegin(GL_QUADS);
      glTexCoord2d(0, 0), glVertex2d(l, t);
      glTexCoord2d(1, 0), glVertex2d(rgt, t);
      glTexCoord2d(1, 1), glVertex2d(rgt, b);
      glTexCoord2d(0, 1), glVertex2d(l, b);
      glEnd();
    }
  }
  glDisable(GL_TEXTURE_2D);

  if (hover && tile_state[hr * kCols + hc] != kLoaded) {
    glColor3f(0.2f, 0.3f, 0.5f);
    glRectd(hc * kCell, hr * kCell, (hc + 1) * kCell, (hr + 1) * kCell);
  }

  glColor3f(0.6f, 0.6f, 0.6f);
  glBegin(GL_LINES);
  for (int c = c0; c <= c1 + 1; ++c) {
    glVertex2d(c * kCell, r0 * kCell);
    glVertex2d(c * kCell, (r1 + 1) * kCell);
  }
  for (int r = r0; r <= r1 + 1; ++r) {
    glVertex2d(c0 * kCell, r * kCell);
    glVertex2d((c1 + 1) * kCell, r * kCell);
  }
  glEnd();

  char buf[64];
  glColor3f(0.9f, 0.9f, 0.9f);
  for (int r = r0; r <= r1; ++r) {
    for (int c = c0; c <= c1; ++c) {
      std::snprintf(buf, sizeof(buf), "x %ld", kX0 + c);
      drawText(c * kCell + 8, r * kCell + 20, buf);
      std::snprintf(buf, sizeof(buf), "y %ld", kY0 + r);
      drawText(c * kCell + 8, r * kCell + 36, buf);
    }
  }

  // Current tile, fixed in the top-left corner.
  glLoadIdentity();
  glColor3f(0, 0, 0);
  glRectd(0, 0, 200, 28);
  glColor3f(1.0f, 0.85f, 0.2f);
  if (hover) {
    std::snprintf(buf, sizeof(buf), "x %ld  y %ld", kX0 + hc, kY0 + hr);
  } else {
    std::snprintf(buf, sizeof(buf), "x -  y -");
  }
  drawText(8, 19, buf);

  glutSwapBuffers();
}

void reshape(int w, int h) {
  win_w = w;
  win_h = h;
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, w, h, 0);  // window pixels, y down like tile y
  glMatrixMode(GL_MODELVIEW);
  clampView();
}

void mouse(int button, int state, int x, int y) {
  if (button != GLUT_LEFT_BUTTON) return;
  dragging = state == GLUT_DOWN;
  drag_x = x;
  drag_y = y;
}

void motion(int x, int y) {
  if (dragging) {
    view_x -= x - drag_x;
    view_y -= y - drag_y;
    drag_x = x;
    drag_y = y;
    clampView();
  }
  mouse_x = x;
  mouse_y = y;
  glutPostRedisplay();
}

void special(int key, int, int) {
  if (key == GLUT_KEY_LEFT) view_x -= kCell;
  if (key == GLUT_KEY_RIGHT) view_x += kCell;
  if (key == GLUT_KEY_UP) view_y -= kCell;
  if (key == GLUT_KEY_DOWN) view_y += kCell;
  clampView();
  glutPostRedisplay();
}

}  // namespace

int main(int argc, char** argv) {
  glutInit(&argc, argv);
  if (argc > 1) map_dir = argv[1];
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize(win_w, win_h);
  glutCreateWindow("map_ui");
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  glutPassiveMotionFunc(motion);
  glutSpecialFunc(special);
  glutMainLoop();
  return 0;
}
