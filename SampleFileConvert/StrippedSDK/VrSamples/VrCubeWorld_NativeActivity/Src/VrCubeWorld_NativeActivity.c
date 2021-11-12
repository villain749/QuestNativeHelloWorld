/************************************************************************************

Filename    :   VrCubeWorld_NativeActivity.c
Content     :   This sample uses the Android NativeActivity class. This sample does
                not use the application framework.
                This sample only uses the VrApi.
Created     :   March, 2015 dude
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright (c) Facebo0000ok Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>


#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

#if !defined(GL_EXT_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples);
#endif

#if !defined(GL_OVR_multiview)
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR = 0x9630;
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR = 0x9632;
static const int GL_MAX_VIEWS_OVR = 0x9631;
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

#if !defined(GL_OVR_multiview_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLsizei samples,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

#define DEBUG 1
#define OVR_LOG_TAG "VrCubeWorld"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static const int NUM_MULTI_SAMPLES = 4;



/*
================================================================================

System Clock Time

================================================================================
*/

static double GetTimeInSeconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif

typedef struct {
    bool multi_view; // GL_OVR_multiview, GL_OVR_multiview2
    bool EXT_texture_border_clamp; // GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
} OpenGLExtensions_t;

OpenGLExtensions_t glExtensions;

static void EglInitExtensions() {
    const char* allExtensions = (const char*)glGetString(GL_EXTENSIONS);
    if (allExtensions != NULL) {
        glExtensions.multi_view = strstr(allExtensions, "GL_OVR_multiview2") &&
            strstr(allExtensions, "GL_OVR_multiview_multisampled_render_to_texture");

        glExtensions.EXT_texture_border_clamp =
            strstr(allExtensions, "GL_EXT_texture_border_clamp") ||
            strstr(allExtensions, "GL_OES_texture_border_clamp");
    }
}

static const char* EglErrorString(const EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}

static const char* GlFrameBufferStatusString(GLenum status) {
    switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            return "GL_FRAMEBUFFER_UNDEFINED";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        default:
            return "unknown";
    }
}



/*
================================================================================

ovrEgl

================================================================================
*/

typedef struct {
    EGLint MajorVersion;
    EGLint MinorVersion;
    EGLDisplay Display;
    EGLConfig Config;
    EGLSurface TinySurface;
    EGLSurface MainSurface;
    EGLContext Context;
} ovrEgl;

static void ovrEgl_Clear(ovrEgl* egl) {
    egl->MajorVersion = 0;
    egl->MinorVersion = 0;
    egl->Display = 0;
    egl->Config = 0;
    egl->TinySurface = EGL_NO_SURFACE;
    egl->MainSurface = EGL_NO_SURFACE;
    egl->Context = EGL_NO_CONTEXT;
}

static void ovrEgl_CreateContext(ovrEgl* egl, const ovrEgl* shareEgl) {
    if (egl->Display != 0) {
        return;
    }

    egl->Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ALOGV("        eglInitialize( Display, &MajorVersion, &MinorVersion )");
    eglInitialize(egl->Display, &egl->MajorVersion, &egl->MinorVersion);
    // Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
    // flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
    // settings, and that is completely wasted for our warp target.
    const int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    if (eglGetConfigs(egl->Display, configs, MAX_CONFIGS, &numConfigs) == EGL_FALSE) {
        ALOGE("        eglGetConfigs() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint configAttribs[] = {
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8, // need alpha for the multi-pass timewarp compositor
        EGL_DEPTH_SIZE,
        0,
        EGL_STENCIL_SIZE,
        0,
        EGL_SAMPLES,
        0,
        EGL_NONE};
    egl->Config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;

        eglGetConfigAttrib(egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value);
        if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
            continue;
        }

        // The pbuffer config also needs to be compatible with normal window rendering
        // so it can share textures with the window context.
        eglGetConfigAttrib(egl->Display, configs[i], EGL_SURFACE_TYPE, &value);
        if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
            continue;
        }

        int j = 0;
        for (; configAttribs[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(egl->Display, configs[i], configAttribs[j], &value);
            if (value != configAttribs[j + 1]) {
                break;
            }
        }
        if (configAttribs[j] == EGL_NONE) {
            egl->Config = configs[i];
            break;
        }
    }
    if (egl->Config == 0) {
        ALOGE("        eglChooseConfig() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    ALOGV("        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )");
    egl->Context = eglCreateContext(
        egl->Display,
        egl->Config,
        (shareEgl != NULL) ? shareEgl->Context : EGL_NO_CONTEXT,
        contextAttribs);
    if (egl->Context == EGL_NO_CONTEXT) {
        ALOGE("        eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    ALOGV("        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )");
    egl->TinySurface = eglCreatePbufferSurface(egl->Display, egl->Config, surfaceAttribs);
    if (egl->TinySurface == EGL_NO_SURFACE) {
        ALOGE("        eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
    ALOGV("        eglMakeCurrent( Display, TinySurface, TinySurface, Context )");
    if (eglMakeCurrent(egl->Display, egl->TinySurface, egl->TinySurface, egl->Context) ==
        EGL_FALSE) {
        ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        eglDestroySurface(egl->Display, egl->TinySurface);
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
}

static void ovrEgl_DestroyContext(ovrEgl* egl) {
    if (egl->Display != 0) {
        ALOGE("        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )");
        if (eglMakeCurrent(egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ==
            EGL_FALSE) {
            ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        }
    }
    if (egl->Context != EGL_NO_CONTEXT) {
        ALOGE("        eglDestroyContext( Display, Context )");
        if (eglDestroyContext(egl->Display, egl->Context) == EGL_FALSE) {
            ALOGE("        eglDestroyContext() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Context = EGL_NO_CONTEXT;
    }
    if (egl->TinySurface != EGL_NO_SURFACE) {
        ALOGE("        eglDestroySurface( Display, TinySurface )");
        if (eglDestroySurface(egl->Display, egl->TinySurface) == EGL_FALSE) {
            ALOGE("        eglDestroySurface() failed: %s", EglErrorString(eglGetError()));
        }
        egl->TinySurface = EGL_NO_SURFACE;
    }
    if (egl->Display != 0) {
        ALOGE("        eglTerminate( Display )");
        if (eglTerminate(egl->Display) == EGL_FALSE) {
            ALOGE("        eglTerminate() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Display = 0;
    }
}




/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct {
    int Width;
    int Height;
    int Multisamples;
    int TextureSwapChainLength;
    int TextureSwapChainIndex;
    bool UseMultiview;
    ovrTextureSwapChain* ColorTextureSwapChain;
    GLuint* DepthBuffers;
    GLuint* FrameBuffers;
} ovrFramebuffer;

static void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer) {
    frameBuffer->Width = 0;
    frameBuffer->Height = 0;
    frameBuffer->Multisamples = 0;
    frameBuffer->TextureSwapChainLength = 0;
    frameBuffer->TextureSwapChainIndex = 0;
    frameBuffer->UseMultiview = false;
    frameBuffer->ColorTextureSwapChain = NULL;
    frameBuffer->DepthBuffers = NULL;
    frameBuffer->FrameBuffers = NULL;
}

static bool ovrFramebuffer_Create(
    ovrFramebuffer* frameBuffer,
    const bool useMultiview,
    const GLenum colorFormat,
    const int width,
    const int height,
    const int multisamples) {
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT =
        (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress(
            "glRenderbufferStorageMultisampleEXT");
    PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT =
        (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress(
            "glFramebufferTexture2DMultisampleEXT");

    PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR =
        (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)eglGetProcAddress(
            "glFramebufferTextureMultiviewOVR");
    PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR =
        (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)eglGetProcAddress(
            "glFramebufferTextureMultisampleMultiviewOVR");

    frameBuffer->Width = width;
    frameBuffer->Height = height;
    frameBuffer->Multisamples = multisamples;
    frameBuffer->UseMultiview =
        (useMultiview && (glFramebufferTextureMultiviewOVR != NULL)) ? true : false;

    frameBuffer->ColorTextureSwapChain = vrapi_CreateTextureSwapChain3(
        frameBuffer->UseMultiview ? VRAPI_TEXTURE_TYPE_2D_ARRAY : VRAPI_TEXTURE_TYPE_2D,
        colorFormat,
        width,
        height,
        1,
        3);
    frameBuffer->TextureSwapChainLength =
        vrapi_GetTextureSwapChainLength(frameBuffer->ColorTextureSwapChain);
    frameBuffer->DepthBuffers =
        (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));
    frameBuffer->FrameBuffers =
        (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));

    ALOGV("        frameBuffer->UseMultiview = %d", frameBuffer->UseMultiview);

    for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        // Create the color buffer texture.
        const GLuint colorTexture =
            vrapi_GetTextureSwapChainHandle(frameBuffer->ColorTextureSwapChain, i);
        GLenum colorTextureTarget = frameBuffer->UseMultiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
        glBindTexture(colorTextureTarget, colorTexture);
        glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glTexParameterfv(colorTextureTarget, GL_TEXTURE_BORDER_COLOR, borderColor);
        glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(colorTextureTarget, 0);

        if (frameBuffer->UseMultiview) {
            // Create the depth buffer texture.
            glGenTextures(1, &frameBuffer->DepthBuffers[i]);
            glBindTexture(GL_TEXTURE_2D_ARRAY, frameBuffer->DepthBuffers[i]);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT24, width, height, 2);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

            // Create the frame buffer.
            glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i]);
            if (multisamples > 1 && (glFramebufferTextureMultisampleMultiviewOVR != NULL)) {
                glFramebufferTextureMultisampleMultiviewOVR(
                    GL_DRAW_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    frameBuffer->DepthBuffers[i],
                    0 /* level */,
                    multisamples /* samples */,
                    0 /* baseViewIndex */,
                    2 /* numViews */);
                glFramebufferTextureMultisampleMultiviewOVR(
                    GL_DRAW_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    colorTexture,
                    0 /* level */,
                    multisamples /* samples */,
                    0 /* baseViewIndex */,
                    2 /* numViews */);
            } else {
                glFramebufferTextureMultiviewOVR(
                    GL_DRAW_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    frameBuffer->DepthBuffers[i],
                    0 /* level */,
                    0 /* baseViewIndex */,
                    2 /* numViews */);
                glFramebufferTextureMultiviewOVR(
                    GL_DRAW_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    colorTexture,
                    0 /* level */,
                    0 /* baseViewIndex */,
                    2 /* numViews */);
            }

            GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                ALOGE(
                    "Incomplete frame buffer object: %s",
                    GlFrameBufferStatusString(renderFramebufferStatus));
                return false;
            }
        } else {
            if (multisamples > 1 && glRenderbufferStorageMultisampleEXT != NULL &&
                glFramebufferTexture2DMultisampleEXT != NULL) {
                // Create multisampled depth buffer.
                glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]);
                glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]);
                glRenderbufferStorageMultisampleEXT(
                    GL_RENDERBUFFER, multisamples, GL_DEPTH_COMPONENT24, width, height);
                glBindRenderbuffer(GL_RENDERBUFFER, 0);

                // Create the frame buffer.
                // NOTE: glFramebufferTexture2DMultisampleEXT only works with GL_FRAMEBUFFER.
                glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]);
                glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i]);
                glFramebufferTexture2DMultisampleEXT(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D,
                    colorTexture,
                    0,
                    multisamples);
                glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    GL_RENDERBUFFER,
                    frameBuffer->DepthBuffers[i]);
                GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                    ALOGE(
                        "Incomplete frame buffer object: %s",
                        GlFrameBufferStatusString(renderFramebufferStatus));
                    return false;
                }
            } else {
                // Create depth buffer.
                glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]);
                glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
                glBindRenderbuffer(GL_RENDERBUFFER, 0);

                // Create the frame buffer.
                glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i]);
                glFramebufferRenderbuffer(
                    GL_DRAW_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    GL_RENDERBUFFER,
                    frameBuffer->DepthBuffers[i]);
                glFramebufferTexture2D(
                    GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
                GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                    ALOGE(
                        "Incomplete frame buffer object: %s",
                        GlFrameBufferStatusString(renderFramebufferStatus));
                    return false;
                }
            }
        }
    }

    return true;
}

static void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer) {
    glDeleteFramebuffers(frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers);
    if (frameBuffer->UseMultiview) {
        glDeleteTextures(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers);
    } else {
        glDeleteRenderbuffers(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers);
    }
    vrapi_DestroyTextureSwapChain(frameBuffer->ColorTextureSwapChain);

    free(frameBuffer->DepthBuffers);
    free(frameBuffer->FrameBuffers);

    ovrFramebuffer_Clear(frameBuffer);
}

static void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer) {
    glBindFramebuffer(
        GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex]);
}

static void ovrFramebuffer_SetNone() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

static void ovrFramebuffer_Resolve(ovrFramebuffer* frameBuffer) {
    // Discard the depth buffer, so the tiler won't need to write it back out to memory.
    const GLenum depthAttachment[1] = {GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, depthAttachment);

    // We now let the resolve happen implicitly.
}

static void ovrFramebuffer_Advance(ovrFramebuffer* frameBuffer) {
    // Advance to the next texture from the set.
    frameBuffer->TextureSwapChainIndex =
        (frameBuffer->TextureSwapChainIndex + 1) % frameBuffer->TextureSwapChainLength;
}

/*
================================================================================

ovrScene

================================================================================
*/



typedef struct {
    bool CreatedScene;
    unsigned int Random;
    GLuint SceneMatrices;
} ovrScene;

static void ovrScene_Clear(ovrScene* scene) {
    scene->CreatedScene = false;
    scene->Random = 2;
    scene->SceneMatrices = 0;
}

static bool ovrScene_IsCreated(ovrScene* scene) {
    return scene->CreatedScene;
}



static void ovrScene_Create(ovrScene* scene, bool useMultiview) {
    // Setup the scene matrices.
    glGenBuffers(1, &scene->SceneMatrices);
    glBindBuffer(GL_UNIFORM_BUFFER, scene->SceneMatrices);
    glBufferData(
        GL_UNIFORM_BUFFER,
        2 * sizeof(ovrMatrix4f) /* 2 view matrices */ +
            2 * sizeof(ovrMatrix4f) /* 2 projection matrices */,
        NULL,
        GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    scene->CreatedScene = true;
}


static void ovrScene_Destroy(ovrScene* scene) {
    glDeleteBuffers(1, &scene->SceneMatrices);
    scene->CreatedScene = false;
}


/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct {
    ovrFramebuffer FrameBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
    int NumBuffers;
} ovrRenderer;

static void ovrRenderer_Clear(ovrRenderer* renderer) {
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrFramebuffer_Clear(&renderer->FrameBuffer[eye]);
    }
    renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;
}

static void
ovrRenderer_Create(ovrRenderer* renderer, const ovrJava* java, const bool useMultiview) {
    renderer->NumBuffers = useMultiview ? 1 : VRAPI_FRAME_LAYER_EYE_MAX;

    // Create the frame buffers.
    for (int eye = 0; eye < renderer->NumBuffers; eye++) {
        ovrFramebuffer_Create(
            &renderer->FrameBuffer[eye],
            useMultiview,
            GL_SRGB8_ALPHA8,
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
            vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT),
            NUM_MULTI_SAMPLES);
    }
}

static void ovrRenderer_Destroy(ovrRenderer* renderer) {
    for (int eye = 0; eye < renderer->NumBuffers; eye++) {
        ovrFramebuffer_Destroy(&renderer->FrameBuffer[eye]);
    }
}

static ovrLayerProjection2 ovrRenderer_RenderFrame(
    ovrRenderer* renderer,
    const ovrJava* java,
    const ovrScene* scene,
    const ovrTracking2* tracking,
    ovrMobile* ovr) {

    ovrTracking2 updatedTracking = *tracking;

    ovrMatrix4f eyeViewMatrixTransposed[2];
    eyeViewMatrixTransposed[0] = ovrMatrix4f_Transpose(&updatedTracking.Eye[0].ViewMatrix);
    eyeViewMatrixTransposed[1] = ovrMatrix4f_Transpose(&updatedTracking.Eye[1].ViewMatrix);

    ovrMatrix4f projectionMatrixTransposed[2];
    projectionMatrixTransposed[0] = ovrMatrix4f_Transpose(&updatedTracking.Eye[0].ProjectionMatrix);
    projectionMatrixTransposed[1] = ovrMatrix4f_Transpose(&updatedTracking.Eye[1].ProjectionMatrix);

    // Update the scene matrices.
    glBindBuffer(GL_UNIFORM_BUFFER, scene->SceneMatrices);
    ovrMatrix4f* sceneMatrices = (ovrMatrix4f*)glMapBufferRange(
           GL_UNIFORM_BUFFER,
           0,
           2 * sizeof(ovrMatrix4f) /* 2 view matrices */ +
               2 * sizeof(ovrMatrix4f) /* 2 projection matrices */,
           GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if (sceneMatrices != NULL) {
        memcpy((char*)sceneMatrices, &eyeViewMatrixTransposed, 2 * sizeof(ovrMatrix4f));
        memcpy(
            (char*)sceneMatrices + 2 * sizeof(ovrMatrix4f),
            &projectionMatrixTransposed,
            2 * sizeof(ovrMatrix4f));
    }

    glUnmapBuffer(GL_UNIFORM_BUFFER);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = updatedTracking.HeadPose;
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[renderer->NumBuffers == 1 ? 0 : eye];
        layer.Textures[eye].ColorSwapChain = frameBuffer->ColorTextureSwapChain;
        layer.Textures[eye].SwapChainIndex = frameBuffer->TextureSwapChainIndex;
        layer.Textures[eye].TexCoordsFromTanAngles =
            ovrMatrix4f_TanAngleMatrixFromProjection(&updatedTracking.Eye[eye].ProjectionMatrix);
    }
    layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

    // Render the eye images.
    for (int eye = 0; eye < renderer->NumBuffers; eye++) {
        // NOTE: In the non-mv case, latency can be further reduced by updating the sensor
        // prediction for each eye (updates orientation, not position)
        ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[eye];
        ovrFramebuffer_SetCurrent(frameBuffer);

        //glDepthMask(GL_TRUE);
        //glEnable(GL_SCISSOR_TEST);
        //glEnable(GL_DEPTH_TEST);
        //glDepthFunc(GL_LEQUAL);
        //glEnable(GL_CULL_FACE);
        //glCullFace(GL_BACK);
        glViewport(0, 0, frameBuffer->Width, frameBuffer->Height);
        glScissor(0, 0, frameBuffer->Width, frameBuffer->Height);
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //doGLStuff();
        ovrFramebuffer_Resolve(frameBuffer);
        ovrFramebuffer_Advance(frameBuffer);
    }

    ovrFramebuffer_SetNone();

    return layer;
}



/*
================================================================================

ovrApp

================================================================================
*/

typedef struct {
    ovrJava Java;
    ovrEgl Egl;
    ANativeWindow* NativeWindow;
    bool Resumed;
    ovrMobile* Ovr;
    ovrScene Scene;
    long long FrameIndex;
    double DisplayTime;
    int SwapInterval;
    int CpuLevel;
    int GpuLevel;
    int MainThreadTid;
    int RenderThreadTid;

    ovrRenderer Renderer;

    bool UseMultiview;
} ovrApp;

static void ovrApp_Clear(ovrApp* app) {
    app->Java.Vm = NULL;
    app->Java.Env = NULL;
    app->Java.ActivityObject = NULL;
    app->NativeWindow = NULL;
    app->Resumed = false;
    app->Ovr = NULL;
    app->FrameIndex = 1;
    app->DisplayTime = 0;
    app->SwapInterval = 1;
    app->CpuLevel = 2;
    app->GpuLevel = 2;
    app->MainThreadTid = 0;
    app->RenderThreadTid = 0;
    app->UseMultiview = true;

    ovrEgl_Clear(&app->Egl);
    ovrScene_Clear(&app->Scene);
    ovrRenderer_Clear(&app->Renderer);

}

static void ovrApp_HandleVrModeChanges(ovrApp* app) {
    if (app->Resumed != false && app->NativeWindow != NULL) {
        if (app->Ovr == NULL) {
            ovrModeParms parms = vrapi_DefaultModeParms(&app->Java);
            // No need to reset the FLAG_FULLSCREEN window flag when using a View
            parms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

            parms.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
            parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
            parms.Display = (size_t)app->Egl.Display;
            parms.WindowSurface = (size_t)app->NativeWindow;
            parms.ShareContext = (size_t)app->Egl.Context;

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            ALOGV("        vrapi_EnterVrMode()");

            app->Ovr = vrapi_EnterVrMode(&parms);

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            // If entering VR mode failed then the ANativeWindow was not valid.
            if (app->Ovr == NULL) {
                ALOGE("Invalid ANativeWindow!");
                app->NativeWindow = NULL;
            }

            // Set performance parameters once we have entered VR mode and have a valid ovrMobile.
            if (app->Ovr != NULL) {
                vrapi_SetClockLevels(app->Ovr, app->CpuLevel, app->GpuLevel);

                ALOGV("		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel);

                vrapi_SetPerfThread(app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid);

                ALOGV("		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid);

                vrapi_SetPerfThread(
                    app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid);

                ALOGV("		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid);
            }
        }
    } else {
        if (app->Ovr != NULL) {

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            ALOGV("        vrapi_LeaveVrMode()");

            vrapi_LeaveVrMode(app->Ovr);
            app->Ovr = NULL;

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));
        }
    }
}

static void ovrApp_HandleInput(ovrApp* app) {}

static void ovrApp_HandleVrApiEvents(ovrApp* app) {
    ovrEventDataBuffer eventDataBuffer = {};

    // Poll for VrApi events
    for (;;) {
        ovrEventHeader* eventHeader = (ovrEventHeader*)(&eventDataBuffer);
        ovrResult res = vrapi_PollEvent(eventHeader);
        if (res != ovrSuccess) {
            break;
        }

        switch (eventHeader->EventType) {
            case VRAPI_EVENT_DATA_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_DATA_LOST");
                break;
            case VRAPI_EVENT_VISIBILITY_GAINED:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_GAINED");
                break;
            case VRAPI_EVENT_VISIBILITY_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_LOST");
                break;
            case VRAPI_EVENT_FOCUS_GAINED:
                // FOCUS_GAINED is sent when the application is in the foreground and has
                // input focus. This may be due to a system overlay relinquishing focus
                // back to the application.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_GAINED");
                break;
            case VRAPI_EVENT_FOCUS_LOST:
                // FOCUS_LOST is sent when the application is no longer in the foreground and
                // therefore does not have input focus. This may be due to a system overlay taking
                // focus from the application. The application should take appropriate action when
                // this occurs.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_LOST");
                break;
            default:
                ALOGV("vrapi_PollEvent: Unknown event");
                break;
        }
    }
}

/*
================================================================================

Native Activity

================================================================================
*/

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    ovrApp* appState = (ovrApp*)app->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            ALOGV("onStart()");
            ALOGV("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            ALOGV("onResume()");
            ALOGV("    APP_CMD_RESUME");
            appState->Resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            ALOGV("onPause()");
            ALOGV("    APP_CMD_PAUSE");
            appState->Resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            ALOGV("onStop()");
            ALOGV("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            ALOGV("onDestroy()");
            ALOGV("    APP_CMD_DESTROY");
            appState->NativeWindow = NULL;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            ALOGV("surfaceCreated()");
            ALOGV("    APP_CMD_INIT_WINDOW");
            appState->NativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            ALOGV("surfaceDestroyed()");
            ALOGV("    APP_CMD_TERM_WINDOW");
            appState->NativeWindow = NULL;
            break;
        }
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");

    ovrJava java;
    java.Vm = app->activity->vm;
    (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
    java.ActivityObject = app->activity->clazz;

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);

    const ovrInitParms initParms = vrapi_DefaultInitParms(&java);
    int32_t initResult = vrapi_Initialize(&initParms);
    if (initResult != VRAPI_INITIALIZE_SUCCESS) {
        // If intialization failed, vrapi_* function calls will not be available.
        exit(0);
    }

    ovrApp appState;
    ovrApp_Clear(&appState);
    appState.Java = java;

    ovrEgl_CreateContext(&appState.Egl, NULL);

    EglInitExtensions();

    glDisable(GL_FRAMEBUFFER_SRGB_EXT);

    appState.UseMultiview &= glExtensions.multi_view;

    ALOGV("AppState UseMultiview : %d", appState.UseMultiview);

    appState.CpuLevel = CPU_LEVEL;
    appState.GpuLevel = GPU_LEVEL;
    appState.MainThreadTid = gettid();


    ovrRenderer_Create(&appState.Renderer, &java, appState.UseMultiview);


    app->userData = &appState;
    app->onAppCmd = app_handle_cmd;

    const double startTime = GetTimeInSeconds();

    while (app->destroyRequested == 0) {
        // Read all pending events.
        for (;;) {
            int events;
            struct android_poll_source* source;
            const int timeoutMilliseconds =
                (appState.Ovr == NULL && app->destroyRequested == 0) ? -1 : 0;
            if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void**)&source) < 0) {
                break;
            }

            // Process this event.
            if (source != NULL) {
                source->process(app, source);
            }

            ovrApp_HandleVrModeChanges(&appState);
        }

        // We must read from the event queue with regular frequency.
        ovrApp_HandleVrApiEvents(&appState);

        ovrApp_HandleInput(&appState);

        if (appState.Ovr == NULL) {
            continue;
        }

        // Create the scene if not yet created.
        // The scene is created here to be able to show a loading icon.
        if (!ovrScene_IsCreated(&appState.Scene)) {

            // Show a loading icon.
            int frameFlags = 0;
            frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

            ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
            blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

            ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
            iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

            const ovrLayerHeader2* layers[] = {
                &blackLayer.Header,
                &iconLayer.Header,
            };

            ovrSubmitFrameDescription2 frameDesc = {0};
            frameDesc.Flags = frameFlags;
            frameDesc.SwapInterval = 1;
            frameDesc.FrameIndex = appState.FrameIndex;
            frameDesc.DisplayTime = appState.DisplayTime;
            frameDesc.LayerCount = 2;
            frameDesc.Layers = layers;

            vrapi_SubmitFrame2(appState.Ovr, &frameDesc);


            // Create the scene.
            ovrScene_Create(&appState.Scene, appState.UseMultiview);
        }

        // This is the only place the frame index is incremented, right before
        // calling vrapi_GetPredictedDisplayTime().
        appState.FrameIndex++;

        // Get the HMD pose, predicted for the middle of the time period during which
        // the new eye images will be displayed. The number of frames predicted ahead
        // depends on the pipeline depth of the engine and the synthesis rate.
        // The better the prediction, the less black will be pulled in at the edges.
        const double predictedDisplayTime =
            vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
        const ovrTracking2 tracking =
            vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);

        appState.DisplayTime = predictedDisplayTime;


        // Render eye images and setup the primary layer using ovrTracking2.
        const ovrLayerProjection2 worldLayer = ovrRenderer_RenderFrame(
            &appState.Renderer,
            &appState.Java,
            &appState.Scene,
            &tracking,
            appState.Ovr);

        const ovrLayerHeader2* layers[] = {&worldLayer.Header};

        ovrSubmitFrameDescription2 frameDesc = {0};
        frameDesc.Flags = 0;
        frameDesc.SwapInterval = appState.SwapInterval;
        frameDesc.FrameIndex = appState.FrameIndex;
        frameDesc.DisplayTime = appState.DisplayTime;
        frameDesc.LayerCount = 1;
        frameDesc.Layers = layers;

        // Hand over the eye images to the time warp.
        vrapi_SubmitFrame2(appState.Ovr, &frameDesc);

    }


    ovrRenderer_Destroy(&appState.Renderer);


    ovrScene_Destroy(&appState.Scene);
    ovrEgl_DestroyContext(&appState.Egl);

    vrapi_Shutdown();

    (*java.Vm)->DetachCurrentThread(java.Vm);
}
