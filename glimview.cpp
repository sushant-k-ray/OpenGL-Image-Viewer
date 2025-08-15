/*    Copyright (c) 2025 Sushant kr. Ray
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a copy
 *    of this software and associated documentation files (the "Software"), to deal
 *    in the Software without restriction, including without limitation the rights
 *    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *    copies of the Software, and to permit persons to whom the Software is
 *    furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in all
 *    copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *    SOFTWARE.
 */

#include "glimview.hpp"
#include <cmath>
#include <iostream>

static unsigned char* data;
static void (*free_data)();

struct Vec2{
    float x, y;
    Vec2(float _x = 0, float _y = 0):
        x(_x), y(_y) {}
};

static inline Vec2 operator-(const Vec2 &a, const Vec2 &b){
    return Vec2(a.x - b.x, a.y - b.y);
}

static inline Vec2 operator+(const Vec2 &a, const Vec2 &b){
    return Vec2(a.x + b.x, a.y + b.y);
}

static inline Vec2 operator*(const Vec2 &a, float s){
    return Vec2(a.x * s, a.y * s);
}

static inline float clampf(float v, float a, float b){
    return v < a ? a : (v > b ? b : v);
}

void freeData(void (*func)())
{
    free_data = func;
}

int winW = 1200, winH = 800;
int imgW = 0, imgH = 0;
GLuint tex = 0;

Vec2 pan(0, 0);
Vec2 targetPan(0, 0);
Vec2 panVel(0, 0);
float zoomLevel = 1.0f;

bool leftDown = false;
bool draggingMain = false;
bool draggingBird = false;
double lastMouseX = 0, lastMouseY = 0;

int birdeyeW = 220, birdeyeH = 0;
int birdeyeX = 0, birdeyeY = 0;
int birdeyeMargin = 12;

const float stiffness = 250.0f;
const float damping = 25.0f;

const char* vs_src = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
uniform mat4 uProj;
uniform vec2 uPan;
uniform float uZoom;
out vec2 vUV;
void main(){
    vec2 pos = aPos * uZoom + uPan;
    gl_Position = uProj * vec4(pos.xy, 0.0, 1.0);
    vUV = aUV;
}
)";

const char* fs_src = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main(){ FragColor = texture(uTex, vUV); }
)";

const char* color_vs = R"(
#version 330 core
layout(location=0) in vec2 aPos;
uniform mat4 uProj;
void main(){ gl_Position = uProj * vec4(aPos.xy, 0.0, 1.0); }
)";

const char* color_fs = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main(){ FragColor = uColor; }
)";

GLuint quadVAO = 0, quadVBO = 0, quadEBO = 0;
GLuint program = 0, colorProgram = 0;

struct Mat4{
    float d[16];
    static Mat4 ortho(float l, float r, float b, float t){
        Mat4 m{};
        m.d[0]  = 2.0f / (r - l);
        m.d[5]  = 2.0f / (t - b);
        m.d[10] = -1.0f;
        m.d[12] = -(r + l) / (r - l);
        m.d[13] = -(t + b) / (t - b);
        m.d[15] = 1.0f;
        return m;
    }
};

Vec2 glfwToScreen(double x, double y){
    return Vec2((float)x, (float)(winH - y));
}

Vec2 clampedPan(const Vec2 &p, float z){
    float dispW = imgW * z;
    float dispH = imgH * z;
    float minX, maxX, minY, maxY;
    if(dispW > winW){
        minX = winW - dispW;
        maxX = 0.0f;
    } else {
        float cx = (winW - dispW) * 0.5f;
        minX = maxX = cx;
    }

    if(dispH > winH){
        minY = winH - dispH;
        maxY = 0.0f;
    } else {
        float cy = (winH - dispH) * 0.5f;
        minY = maxY = cy;
    }

    float cx = clampf(p.x, minX, maxX);
    float cy = clampf(p.y, minY, maxY);
    return Vec2(cx, cy);
}

Vec2 screenToImage(const Vec2 &s){
    return Vec2((s.x - pan.x) / zoomLevel, (s.y - pan.y) / zoomLevel );
}

void centerImage(const Vec2 &imgPt){
    Vec2 center(winW * 0.5f, winH * 0.5f);
    Vec2 newPan(center.x - imgPt.x * zoomLevel, center.y - imgPt.y * zoomLevel);
    targetPan = clampedPan(newPan, zoomLevel);
    pan = targetPan;
    panVel = Vec2(0, 0);
}

void updateBirdeyeDims(){
    birdeyeW = 220;
    birdeyeH = int(round((float)birdeyeW * ((float)imgH / (float)imgW)));
    birdeyeX = winW - birdeyeW - birdeyeMargin;
    birdeyeY = birdeyeMargin;
}

GLuint compileShader(GLenum type, const char* src){
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok){
        char buf[1024];
        glGetShaderInfoLog(s, 1024, nullptr, buf);
        std::cerr << "Shader compile error: " << buf << "\n";
        exit(1);
    }

    return s;
}

GLuint linkProgram(GLuint vs, GLuint fs){
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok){
        char buf[1024];
        glGetProgramInfoLog(p, 1024, nullptr, buf);
        std::cerr << "Link error: " << buf << "\n";
        exit(1);
    }

    return p;
}

void framebufferSizeCallback(GLFWwindow* w, int width, int height){
    winW = width;
    winH = height;

    float imgWidth = imgW * zoomLevel;
    float imgHeight = imgH * zoomLevel;

    if (imgWidth <= winW) {
        pan.x = (winW - imgWidth) / 2.0f;
        targetPan.x = pan.x;
    } else {
        if (pan.x > 0) {
            pan.x = 0;
            targetPan.x = pan.x;
        }

        if (pan.x + imgWidth < winW) {
            pan.x = winW - imgWidth;
            targetPan.x = pan.x;
        }
    }

    if (imgHeight <= winH) {
        pan.y = (winH - imgHeight) / 2.0f;
        targetPan.y = pan.y;
    } else {
        if (pan.y > 0) {
            pan.y = 0;
            targetPan.y = pan.y;
        }

        if (pan.y + imgHeight < winH) {
            pan.y = winH - imgHeight;
            targetPan.y = pan.y;
        }
    }

    updateBirdeyeDims();
}

void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods){
    double mx, my;
    glfwGetCursorPos(w, &mx, &my);
    Vec2 s = glfwToScreen(mx, my);

    if(button == GLFW_MOUSE_BUTTON_LEFT){
        if(action == GLFW_PRESS){
            leftDown=true;
            lastMouseX = s.x;
            lastMouseY = s.y;

            if(s.x >= birdeyeX && s.x <= birdeyeX + birdeyeW &&
                s.y >= birdeyeY && s.y <= birdeyeY + birdeyeH){
                draggingBird = true;
                draggingMain = false;
                panVel = Vec2(0, 0);

                float localX = s.x - birdeyeX;
                float localY = s.y - birdeyeY;

                float sx = (float)birdeyeW / imgW;
                float sy = (float)birdeyeH / imgH;

                float birdZoom = fmin(sx, sy);
                float imgDisplayW = imgW * birdZoom;
                float imgDisplayH = imgH * birdZoom;

                float birdPanX = (birdeyeW - imgDisplayW) * 0.5f;
                float birdPanY = (birdeyeH - imgDisplayH) * 0.5f;

                float imgX = (localX - birdPanX) / birdZoom;
                float imgY = (localY - birdPanY) / birdZoom;

                imgX = clampf(imgX, 0.0f, (float)imgW);
                imgY = clampf(imgY, 0.0f, (float)imgH);

                centerImage(Vec2(imgX, imgY));
            } else {
                draggingMain = true;
                draggingBird = false;
                panVel = Vec2(0, 0);
            }
        } else if(action==GLFW_RELEASE){
            leftDown = false;
            if(draggingMain || draggingMain)
                targetPan = clampedPan(pan, zoomLevel);

            draggingMain = false;
            draggingBird = false;
        }
    }
}

void cursorCallback(GLFWwindow* w, double xpos, double ypos){
    Vec2 cur = glfwToScreen(xpos,ypos);

    if(leftDown && draggingMain){
        float dx = cur.x - (float)lastMouseX;
        float dy = cur.y - (float)lastMouseY;

        pan.x += dx;
        pan.y += dy;

        lastMouseX = cur.x;
        lastMouseY = cur.y;
    } else if(leftDown && draggingBird){
        float localX = cur.x - birdeyeX;
        float localY = cur.y - birdeyeY;

        float sx = (float)birdeyeW / imgW;
        float sy = (float)birdeyeH / imgH;

        float birdZoom = fmin(sx, sy);
        float imgDisplayW = imgW * birdZoom;
        float imgDisplayH = imgH * birdZoom;

        float birdPanX = (birdeyeW - imgDisplayW) * 0.5f;
        float birdPanY = (birdeyeH - imgDisplayH) * 0.5f;

        float imgX = (localX - birdPanX) / birdZoom;
        float imgY = (localY - birdPanY) / birdZoom;

        imgX = clampf(imgX, 0.0f, (float)imgW);
        imgY = clampf(imgY, 0.0f, (float)imgH);

        centerImage(Vec2(imgX, imgY));
    } else {
        lastMouseX = cur.x;
        lastMouseY = cur.y;
    }
}

void scrollCallback(GLFWwindow* w, double xoffset, double yoffset){
    double mx, my;
    glfwGetCursorPos(w, &mx, &my);
    Vec2 s = glfwToScreen(mx,my);
    Vec2 worldBefore = screenToImage(s);
    float factor = exp((float)yoffset * 0.18f);
    float newZoom = clampf(zoomLevel * factor, 0.05f, 20.0f);
    zoomLevel = newZoom;
    pan.x = s.x - worldBefore.x * zoomLevel;
    pan.y = s.y - worldBefore.y * zoomLevel;
    if (imgW * zoomLevel <= winW)
        pan.x = (winW - imgW * zoomLevel) / 2;

    if (imgH * zoomLevel <= winH)
        pan.y = (winH - imgH * zoomLevel) / 2;

    targetPan = clampedPan(pan, zoomLevel);
}

void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if ((mods & GLFW_MOD_CONTROL) &&
            (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD))
            scrollCallback(w, 0, 0.36);

        if ((mods & GLFW_MOD_CONTROL) &&
            (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT))
            scrollCallback(w, 0, -0.36);
    }
}


void updateSpring(float dt){
    Vec2 disp = pan - targetPan;
    float ax = -stiffness * disp.x - damping * panVel.x;
    float ay = -stiffness * disp.y - damping * panVel.y;
    panVel.x += ax * dt;
    panVel.y += ay * dt;
    pan.x += panVel.x * dt;
    pan.y += panVel.y * dt;

    if(fabs(disp.x) < 0.5f && fabs(panVel.x) < 0.5f) {
        pan.x = targetPan.x;
        panVel.x = 0.0f;
    }

    if(fabs(disp.y) < 0.5f && fabs(panVel.y) < 0.5f){
        pan.y = targetPan.y;
        panVel.y = 0.0f;
    }
}

void glimviewUpdateImage(unsigned char *d, int width, int height)
{
    data = d;
    imgW = width;
    imgH = height;
}

int showGlimview(){
    if(!glfwInit()){
        std::cerr<<"Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(winW, winH, "Image Viewer", NULL, NULL);
    if(!window){
        std::cerr<<"Failed to create window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr<<"Failed to init GLAD\n";
        return 1;
    }

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if(free_data)
        free_data();

    float vertices[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        (float)imgW, 0.0f, 1.0f, 0.0f,
        (float)imgW, (float)imgH, 1.0f, 1.0f,
        0.0f, (float)imgH, 0.0f, 1.0f
    };

    unsigned int indices[] = {0, 1, 2, 2, 3, 0};
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glGenBuffers(1, &quadEBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(2*sizeof(float)));
    glBindVertexArray(0);

    GLuint vs = compileShader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fs_src);
    program = linkProgram(vs, fs);

    GLuint cvs = compileShader(GL_VERTEX_SHADER, color_vs);
    GLuint cfs = compileShader(GL_FRAGMENT_SHADER, color_fs);
    colorProgram = linkProgram(cvs, cfs);

    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteShader(cvs);
    glDeleteShader(cfs);

    pan.x = (winW - imgW) * 0.5f;
    pan.y = (winH - imgH) * 0.5f;
    targetPan = clampedPan(pan, zoomLevel);
    panVel = Vec2(0,0);

    updateBirdeyeDims();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    double lastTime = glfwGetTime();

    while(!glfwWindowShouldClose(window)){
        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;
        if(dt > 0.05)
            dt = 0.05;

        glfwPollEvents();

        if(!draggingMain)
            updateSpring((float)dt);

        glViewport(0,0,winW,winH);
        glClearColor(0.12f,0.12f,0.12f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        Mat4 proj = Mat4::ortho(0.0f, (float)winW, 0.0f, (float)winH);

        glUseProgram(program);
        GLint locProj = glGetUniformLocation(program, "uProj");
        GLint locPan = glGetUniformLocation(program, "uPan");
        GLint locZoom = glGetUniformLocation(program, "uZoom");
        glUniformMatrix4fv(locProj, 1, GL_FALSE, proj.d);
        glUniform2f(locPan, pan.x, pan.y);
        glUniform1f(locZoom, zoomLevel);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(program, "uTex"), 0);
        glBindVertexArray(quadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        updateBirdeyeDims();
        glViewport(birdeyeX, birdeyeY, birdeyeW, birdeyeH);
        Mat4 birdProj = Mat4::ortho(0.0f, (float)birdeyeW, 0.0f, (float)birdeyeH);

        float sx = (float)birdeyeW / (float)imgW;
        float sy = (float)birdeyeH / (float)imgH;

        float birdZoom = fmin(sx, sy);
        float imgDisplayW = imgW * birdZoom;
        float imgDisplayH = imgH * birdZoom;

        float birdPanX = ((float)birdeyeW - imgDisplayW) * 0.5f;
        float birdPanY = ((float)birdeyeH - imgDisplayH) * 0.5f;

        glUseProgram(program);
        glUniformMatrix4fv(locProj, 1, GL_FALSE, birdProj.d);
        glUniform2f(locPan, birdPanX, birdPanY);
        glUniform1f(locZoom, birdZoom);
        glBindTexture(GL_TEXTURE_2D, tex);
        glBindVertexArray(quadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        Vec2 visMin, visMax;
        Vec2 s0(0, 0), s1((float)winW, (float)winH);
        Vec2 i0 = Vec2((s0.x - pan.x) / zoomLevel, (s0.y - pan.y) / zoomLevel);
        Vec2 i1 = Vec2((s1.x - pan.x) / zoomLevel, (s1.y - pan.y) / zoomLevel);
        visMin.x = fmin(i0.x, i1.x);
        visMin.y = fmin(i0.y, i1.y);
        visMax.x = fmax(i0.x, i1.x);
        visMax.y = fmax(i0.y, i1.y);

        visMin.x = clampf(visMin.x, 0.0f, (float)imgW);
        visMin.y = clampf(visMin.y, 0.0f, (float)imgH);
        visMax.x = clampf(visMax.x, 0.0f, (float)imgW);
        visMax.y = clampf(visMax.y, 0.0f, (float)imgH);

        float bx0 = birdPanX + visMin.x * birdZoom;
        float by0 = birdPanY + visMin.y * birdZoom;
        float bx1 = birdPanX + visMax.x * birdZoom;
        float by1 = birdPanY + visMax.y * birdZoom;

        glUseProgram(colorProgram);
        GLint locP = glGetUniformLocation(colorProgram, "uProj");
        glUniformMatrix4fv(locP, 1, GL_FALSE, birdProj.d);
        GLint locColor = glGetUniformLocation(colorProgram, "uColor");
        glUniform4f(locColor, 0.5f, 0.5f, 1.0f, 0.75f);
        float rectVerts[] = { bx0, by0, bx1, by0, bx1, by1, bx0, by1 };
        GLuint rectVBO, rectVAO;
        glGenVertexArrays(1, &rectVAO);
        glGenBuffers(1, &rectVBO);
        glBindVertexArray(rectVAO);
        glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(rectVerts), rectVerts, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
        glLineWidth(2.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glDeleteBuffers(1, &rectVBO);
        glDeleteVertexArrays(1, &rectVAO);

        glViewport(0, 0, winW, winH);
        Mat4 fullProj = Mat4::ortho(0.0f, (float)winW, 0.0f, (float)winH);
        glUseProgram(colorProgram);
        glUniformMatrix4fv(locP, 1, GL_FALSE, fullProj.d);
        glUniform4f(locColor, 0.12f, 0.12f, 0.12f, 0.80f);
        float borderVerts[] = { (float)birdeyeX, (float)birdeyeY,
                               (float)(birdeyeX + birdeyeW), (float)birdeyeY,
                               (float)(birdeyeX + birdeyeW),(float)(birdeyeY + birdeyeH),
                               (float)birdeyeX,(float)(birdeyeY + birdeyeH) };

        GLuint bVBO, bVAO;
        glGenVertexArrays(1, &bVAO);
        glGenBuffers(1, &bVBO);
        glBindVertexArray(bVAO);

        glBindBuffer(GL_ARRAY_BUFFER, bVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(borderVerts), borderVerts, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        glDeleteBuffers(1, &bVBO);
        glDeleteVertexArrays(1, &bVAO);

        glfwSwapBuffers(window);
    }

    glDeleteProgram(program);
    glDeleteProgram(colorProgram);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteBuffers(1, &quadEBO);
    glDeleteTextures(1, &tex);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
