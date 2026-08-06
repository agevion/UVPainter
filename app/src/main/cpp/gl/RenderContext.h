// Contexto EGL creado directamente sobre el ANativeWindow del SurfaceView.
// No usamos GLSurfaceView: queremos controlar el hilo de render y el momento
// exacto del swap para minimizar latencia con el lapiz.
#pragma once

#include <EGL/egl.h>

struct ANativeWindow;

namespace uvp {

class RenderContext {
public:
    RenderContext() = default;
    ~RenderContext();

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    bool initialize(ANativeWindow* window);
    void shutdown();

    bool makeCurrent();
    void swapBuffers();

    // 0 = sin vsync (lo dejamos activado por defecto; el modo de baja latencia
    // se gestiona con front-buffer rendering en el lado Kotlin).
    void setSwapInterval(int interval);

    int width() const { return width_; }
    int height() const { return height_; }
    bool valid() const { return context_ != EGL_NO_CONTEXT && surface_ != EGL_NO_SURFACE; }

    void refreshSurfaceSize();

private:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLConfig config_ = nullptr;
    ANativeWindow* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace uvp
