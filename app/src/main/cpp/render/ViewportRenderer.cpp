#include "render/ViewportRenderer.h"

#include <algorithm>

#include "core/Log.h"
#include "render/Shaders.h"
#include "scene/Camera.h"

namespace uvp {

bool ViewportRenderer::initialize() {
    if (ready_) return true;
    bool ok = true;
    ok &= backgroundShader_.compile(shaders::kFullscreenVS, shaders::kBackgroundFS, "background");
    ok &= modelShader_.compile(shaders::kModelVS, shaders::kModelFS, "model");
    ok &= wireframeShader_.compile(shaders::kModelVS, shaders::kWireframeFS, "wireframe");
    ok &= cursorShader_.compile(shaders::kFullscreenVS, shaders::kBrushCursorFS, "brushCursor");
    ok &= ropeShader_.compile(shaders::kFullscreenVS, shaders::kRopeFS, "stabilizerRope");
    if (!ok) LOGE("ViewportRenderer: fallo compilando shaders");
    ready_ = ok;
    return ok;
}

void ViewportRenderer::shutdown() {
    backgroundShader_.destroy();
    modelShader_.destroy();
    wireframeShader_.destroy();
    cursorShader_.destroy();
    ropeShader_.destroy();
    ready_ = false;
}

void ViewportRenderer::renderScene(int width, int height, const Camera& camera, const Mesh& mesh,
                                   const Texture2D& baseColor, const Texture2D* boundaryMask,
                                   const ViewportSettings& settings) {
    if (!ready_) return;

    Framebuffer::unbind();
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);

    // Fondo primero, sin profundidad.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    backgroundShader_.bind();
    backgroundShader_.set("uTopColor", settings.backgroundTop);
    backgroundShader_.set("uBottomColor", settings.backgroundBottom);
    drawFullscreenTriangle();

    if (!mesh.valid()) return;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClearDepthf(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    if (settings.backfaceCull) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    } else {
        glDisable(GL_CULL_FACE);
    }

    const Mat4 viewProj = camera.viewProjection();

    modelShader_.bind();
    modelShader_.set("uViewProj", viewProj);
    modelShader_.set("uCameraPos", camera.position());
    modelShader_.set("uViewMode", static_cast<int>(settings.mode));
    modelShader_.set("uRoughness", settings.roughness);
    modelShader_.set("uMetallic", settings.metallic);
    modelShader_.set("uLightDir", normalize(settings.lightDir));
    modelShader_.set("uLightColor", settings.lightColor);
    modelShader_.set("uAmbientSky", settings.ambientSky);
    modelShader_.set("uAmbientGround", settings.ambientGround);
    modelShader_.set("uUvCheckerDensity", settings.uvCheckerDensity);
    modelShader_.set("uShowUnpaintedTint", settings.showUnpaintedTint ? 1 : 0);
    modelShader_.set("uUnlitShading", settings.unlitShading);
    baseColor.bind(0);
    modelShader_.set("uBaseColor", 0);

    const bool showBoundary =
        settings.showBoundary && boundaryMask != nullptr && boundaryMask->valid();
    modelShader_.set("uShowBoundary", showBoundary ? 1 : 0);
    modelShader_.set("uBoundaryColor", settings.boundaryColor);
    if (showBoundary) {
        boundaryMask->bind(1);
        modelShader_.set("uBoundaryMask", 1);
    }
    mesh.draw();

    const bool drawWire = settings.showWireframe && mesh.hasWireframe();
    const bool drawSeams = settings.showSeams && mesh.hasSeams();
    if (drawWire || drawSeams) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Empujamos las lineas hacia la camara para que no se coman por el
        // z-buffer contra la propia superficie.
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
        glDepthMask(GL_FALSE);

        // Las lineas de 1 px se pierden en una pantalla de 2560: pedimos mas
        // grosor, recortado a lo que el driver acepte.
        GLfloat range[2] = {1.0f, 1.0f};
        glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
        glLineWidth(std::min(std::max(settings.lineWidth, range[0]), range[1]));

        wireframeShader_.bind();
        wireframeShader_.set("uViewProj", viewProj);

        if (drawWire) {
            wireframeShader_.set("uColor", settings.wireframeColor);
            mesh.drawWireframe();
        }
        // Las costuras van despues para que queden por encima de la malla.
        if (drawSeams) {
            wireframeShader_.set("uColor", settings.seamColor);
            mesh.drawSeams();
        }

        glLineWidth(1.0f);
        glDepthMask(GL_TRUE);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_BLEND);
    }

    glDisable(GL_CULL_FACE);
    GL_CHECK("ViewportRenderer::renderScene");
}

void ViewportRenderer::drawBrushCursor(int width, int height, Vec2 centerPx, float radiusPx,
                                       float hardness, Vec4 color) {
    if (!ready_ || radiusPx <= 0.5f) return;

    Framebuffer::unbind();
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    cursorShader_.bind();
    cursorShader_.set("uViewportSize",
                      Vec2(static_cast<float>(width), static_cast<float>(height)));
    cursorShader_.set("uCenter", centerPx);
    cursorShader_.set("uRadius", radiusPx);
    cursorShader_.set("uInnerScale", clampf(hardness, 0.0f, 1.0f));
    cursorShader_.set("uColor", color);
    drawFullscreenTriangle();

    glDisable(GL_BLEND);
    GL_CHECK("ViewportRenderer::drawBrushCursor");
}

void ViewportRenderer::drawRope(int width, int height, Vec2 anchorPx, Vec2 tipPx, Vec4 color) {
    if (!ready_) return;
    // Con la cuerda destensada los dos extremos coinciden y no hay nada que
    // dibujar; pintar un segmento de longitud cero solo mete un punto raro.
    if (length(tipPx - anchorPx) < 1.5f) return;

    Framebuffer::unbind();
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ropeShader_.bind();
    ropeShader_.set("uViewportSize",
                    Vec2(static_cast<float>(width), static_cast<float>(height)));
    ropeShader_.set("uAnchor", anchorPx);
    ropeShader_.set("uTip", tipPx);
    ropeShader_.set("uColor", color);
    drawFullscreenTriangle();

    glDisable(GL_BLEND);
    GL_CHECK("ViewportRenderer::drawRope");
}

}  // namespace uvp
