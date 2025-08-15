# OpenGL-Image-Viewer
An OpenGL + GLFW based image viewer with **smooth spring-based panning** and an optional **bird’s-eye mini-map** for navigation.
Supports drag-based navigation, zooming, and inertia for a natural viewing experience.

---

## Preview

![Screenshot](screenshot.png)

---

## Features

* **Spring physics** for smooth and elastic panning.
* **Bird’s-eye view** for quick navigation.
* **Zooming** with mouse wheel and `Ctrl +` / `Ctrl -` keyboard shortcuts.
* **Pan clamping** so the image never drifts outside the viewport.
* **OpenGL accelerated rendering** for high performance.
* Works with **any image format** supported by [stb\_image](https://github.com/nothings/stb).

---

## Dependencies

This project uses:

* [GLFW](https://www.glfw.org/) – Window and input handling.
* [GLAD](https://glad.dav1d.de/) – OpenGL loader.
* [stb\_image.h](https://github.com/nothings/stb) – Image loading.

---

## Build Instructions

1. **Clone this repository**:

   ```bash
   git clone https://github.com/sushant-k-ray/OpenGL-Image-Viewer.git
   cd OpenGL-Image-Viewer
   ```

2. **Install dependencies**:

   * **Linux** (Debian/Ubuntu):

     ```bash
     sudo apt install libglfw3-dev
     ```
   * **Windows**:
     Use vcpkg or manually download GLFW + GLAD.

3. **Compile**:

   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

4. **Run**:

   ```bash
   ./glimviewer image_file
   ```

---

## Controls

| Action                        | Control                                   |
| ----------------------------- | ----------------------------------------- |
| Pan                           | Click + drag                              |
| Zoom in/out (smooth)          | Mouse wheel / `Ctrl` + `+` / `Ctrl` + `-` |
| Navigate from bird’s-eye view | Click + drag inside mini-map              |

---

## Project Structure

```
image-viewer-spring/
├── glad/...        # GLAD files
├── stb/...         # STB header only image loader
├── glimview.cpp    # OpenGL image viewer source code
├── glimview.hpp
├── main.cpp
```

---

## License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.

---

