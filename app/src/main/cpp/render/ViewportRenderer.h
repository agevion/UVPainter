#pragma once

#include "gl/GLObjects.h"
#include "paint/Types.h"

namespace uvp {

class Camera;

struct ViewportSettings {
    ViewMode mode = ViewMode::Unlit;
    bool showWireframe = false;
    bool showSeams = true;
    bool backfaceCull = false;
    bool showUnpaintedTint = true;

    float roughness = 0.65f;
    float metallic = 0.0f;
    /// 0 = color exacto al pixel; valores bajos dan volumen sin cambiar el tono.
    float unlitShading = 0.4f;
    /// Grosor de las lineas de malla y costura, si el driver lo admite.
    float lineWidth = 3.0f;

    Vec3 lightDir{-0.45f, -0.72f, -0.53f};
    Vec3 lightColor{2.6f, 2.5f, 2.4f};
    Vec3 ambientSky{0.30f, 0.34f, 0.42f};
    Vec3 ambientGround{0.14f, 0.13f, 0.12f};

    float uvCheckerDensity = 32.0f;

    Vec3 backgroundTop{0.16f, 0.17f, 0.20f};
    Vec3 backgroundBottom{0.07f, 0.07f, 0.09f};
    // Contraste alto a proposito: sobre un modelo gris sin pintar, una malla
    // grisacea es invisible, y las costuras son justo lo que hay que ver.
    Vec4 wireframeColor{0.35f, 0.85f, 1.0f, 0.42f};
    Vec4 seamColor{1.0f, 0.42f, 0.15f, 0.95f};
    bool showBoundary = true;
    Vec3 boundaryColor{0.25f, 1.0f, 0.55f};
};

class ViewportRenderer {
public:
    bool initialize();
    void shutdown();

    void renderScene(int width, int height, const Camera& camera, const Mesh& mesh,
                     const Texture2D& baseColor, const Texture2D* boundaryMask,
                     const ViewportSettings& settings);

    /// Cuerda del regulador: une donde cae la pintura con la punta del lapiz.
    /// `holeRadiusPx` es el hueco que se deja alrededor del anclaje para no
    /// dibujar por debajo del circulo del pincel.
    void drawRope(int width, int height, Vec2 anchorPx, Vec2 tipPx, float holeRadiusPx,
                  Vec4 color);
    void drawBrushCursor(int width, int height, Vec2 centerPx, float radiusPx, float hardness,
                         Vec4 color);

private:
    Shader backgroundShader_;
    Shader modelShader_;
    Shader wireframeShader_;
    Shader cursorShader_;
    Shader ropeShader_;
    bool ready_ = false;
};

}  // namespace uvp
