#ifndef SJHEADER_H
#define SJHEADER_H

#include <EGL/egl.h>
#include <GLES3/gl3.h>


float domath(float a, float b) {
    return a + b;
}


float getBlueVal() {
    return 1.0f;
}


void doGLStuff() {
    glClearColor(0.99f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


#endif