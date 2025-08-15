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
#include <iostream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

unsigned char* data;

void freeImage()
{
    stbi_image_free(data);
}

int main(int argc, char** argv){
    if(argc < 2){
        std::cout << "Usage: " << argv[0] << " path_to_image\n";
        return 1;
    }

    std::string path = argv[1];
    stbi_set_flip_vertically_on_load(true);
    int imgW, imgH;
    data = stbi_load(path.c_str(), &imgW, &imgH, NULL, 4);
    if(!data){
        std::cerr << "Failed to load image: " << path << "\n";
        return 1;
    }

    glimviewUpdateImage(data, imgW, imgH);
    freeData(freeImage);
    return showGlimview();
}
