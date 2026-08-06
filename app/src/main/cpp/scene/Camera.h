// Camara de orbita tipo Procreate/Nomad: gira alrededor de un objetivo,
// con paneo y zoom. Guarda tambien la conversion pantalla <-> mundo que
// necesita el motor de pintado.
#pragma once

#include "core/Math.h"

namespace uvp {

class Camera {
public:
    void frameBounds(const Aabb& bounds);

    void orbit(float deltaYawRad, float deltaPitchRad);
    void pan(float dxPixels, float dyPixels);
    void dolly(float scaleFactor);
    void roll(float deltaRad);
    void resetRoll();

    void setViewport(int width, int height);
    void setFovDegrees(float deg) { fovDeg_ = clampf(deg, 5.0f, 120.0f); }
    void setOrthographic(bool ortho) { orthographic_ = ortho; }
    bool isOrthographic() const { return orthographic_; }

    Vec3 position() const;
    Vec3 target() const { return target_; }
    Vec3 upVector() const;
    Vec3 forward() const;

    Mat4 view() const;
    Mat4 projection() const;
    Mat4 viewProjection() const { return projection() * view(); }

    float nearPlane() const { return nearPlane_; }
    float farPlane() const { return farPlane_; }
    float distance() const { return distance_; }
    int viewportWidth() const { return viewportW_; }
    int viewportHeight() const { return viewportH_; }

    // Escala de referencia para que el paneo se sienta 1:1 con el dedo.
    float worldUnitsPerPixel() const;

    // Distancia a la que [frameBounds] deja la camara al encuadrar el modelo.
    // No cambia con el zoom (solo con el tamano del modelo y el campo de
    // vision), asi que sirve de referencia estable para el pincel de tamano
    // fijo: comparando esta distancia con la actual se sabe cuanto ha
    // cambiado el zoom desde el encuadre inicial.
    float referenceDistance() const;

private:
    Vec3 target_{0.0f, 0.0f, 0.0f};
    float distance_ = 3.0f;
    float yaw_ = radians(35.0f);
    float pitch_ = radians(15.0f);
    float roll_ = 0.0f;
    float fovDeg_ = 40.0f;
    float nearPlane_ = 0.01f;
    float farPlane_ = 100.0f;
    float sceneRadius_ = 1.0f;
    bool orthographic_ = false;
    int viewportW_ = 1;
    int viewportH_ = 1;
};

}  // namespace uvp
