#include "gl/RenderContext.h"

#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include <android/native_window.h>

#include "core/Log.h"

namespace uvp {

RenderContext::~RenderContext() { shutdown(); }

bool RenderContext::initialize(ANativeWindow* window) {
    if (window == nullptr) {
        LOGE("initialize: ANativeWindow nulo");
        return false;
    }
    shutdown();
    window_ = window;

    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay fallo");
        return false;
    }

    EGLint major = 0, minor = 0;
    if (eglInitialize(display_, &major, &minor) != EGL_TRUE) {
        LOGE("eglInitialize fallo: 0x%04x", eglGetError());
        display_ = EGL_NO_DISPLAY;
        return false;
    }
    LOGI("EGL %d.%d inicializado", major, minor);

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_STENCIL_SIZE,    8,
        EGL_NONE,
    };

    EGLint numConfigs = 0;
    if (eglChooseConfig(display_, configAttribs, &config_, 1, &numConfigs) != EGL_TRUE ||
        numConfigs < 1) {
        LOGE("eglChooseConfig no encontro configuracion valida");
        shutdown();
        return false;
    }

    // Pedimos ES 3.2 y bajamos a 3.0 si el driver no lo da.
    const EGLint contextAttribs32[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE,
    };
    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttribs32);
    if (context_ == EGL_NO_CONTEXT) {
        const EGLint contextAttribs30[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttribs30);
        if (context_ == EGL_NO_CONTEXT) {
            LOGE("eglCreateContext fallo: 0x%04x", eglGetError());
            shutdown();
            return false;
        }
        LOGW("Contexto creado como ES 3.0 (sin 3.2)");
    }

    surface_ = eglCreateWindowSurface(display_, config_, window_, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface fallo: 0x%04x", eglGetError());
        shutdown();
        return false;
    }

    if (!makeCurrent()) {
        shutdown();
        return false;
    }

    refreshSurfaceSize();
    LOGI("Contexto GL listo: %dx%d | %s | %s", width_, height_,
         reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
         reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    return true;
}

void RenderContext::shutdown() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        eglTerminate(display_);
    }
    display_ = EGL_NO_DISPLAY;
    context_ = EGL_NO_CONTEXT;
    surface_ = EGL_NO_SURFACE;
    config_ = nullptr;
    window_ = nullptr;
    width_ = height_ = 0;
}

bool RenderContext::makeCurrent() {
    if (display_ == EGL_NO_DISPLAY || surface_ == EGL_NO_SURFACE) return false;
    if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
        LOGE("eglMakeCurrent fallo: 0x%04x", eglGetError());
        return false;
    }
    return true;
}

void RenderContext::swapBuffers() {
    if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
        eglSwapBuffers(display_, surface_);
    }
}

void RenderContext::setSwapInterval(int interval) {
    if (display_ != EGL_NO_DISPLAY) eglSwapInterval(display_, interval);
}

void RenderContext::refreshSurfaceSize() {
    if (display_ == EGL_NO_DISPLAY || surface_ == EGL_NO_SURFACE) return;
    EGLint w = 0, h = 0;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &w);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &h);
    width_ = w;
    height_ = h;
}

}  // namespace uvp
