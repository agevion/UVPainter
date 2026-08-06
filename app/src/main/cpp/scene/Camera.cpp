#include "scene/Camera.h"

namespace uvp {

namespace {
// Dejamos un margen por debajo de los polos: si pitch llega a +-90 grados el
// vector "up" degenera y la camara da un tumbo.
constexpr float kPitchLimit = 1.5533f;  // ~89 grados
}  // namespace

void Camera::frameBounds(const Aabb& bounds) {
    if (!bounds.valid()) return;
    target_ = bounds.center();
    sceneRadius_ = std::max(bounds.radius(), 1e-3f);

    // Distancia que mete la esfera envolvente en el campo de vision vertical,
    // con un 15% de aire alrededor.
    const float halfFov = radians(fovDeg_) * 0.5f;
    distance_ = (sceneRadius_ / std::sin(halfFov)) * 1.15f;

    // El plano cercano se ata a la distancia, no al tamano de la escena: un
    // near de 0.001 con la camara a 250 unidades tira toda la precision del
    // z-buffer a la basura.
    nearPlane_ = std::max(distance_ * 0.005f, 1e-4f);
    farPlane_ = distance_ + sceneRadius_ * 12.0f;
    yaw_ = radians(35.0f);
    pitch_ = radians(15.0f);
    roll_ = 0.0f;
}

void Camera::orbit(float deltaYawRad, float deltaPitchRad) {
    yaw_ += deltaYawRad;
    pitch_ = clampf(pitch_ + deltaPitchRad, -kPitchLimit, kPitchLimit);
}

void Camera::pan(float dxPixels, float dyPixels) {
    const float scale = worldUnitsPerPixel();
    const Vec3 fwd = forward();
    const Vec3 right = normalize(cross(fwd, upVector()));
    const Vec3 up = cross(right, fwd);
    target_ = target_ - right * (dxPixels * scale) + up * (dyPixels * scale);
}

void Camera::dolly(float scaleFactor) {
    if (scaleFactor <= 0.0f) return;
    distance_ = clampf(distance_ / scaleFactor, sceneRadius_ * 0.02f, sceneRadius_ * 60.0f);
    nearPlane_ = std::max(distance_ * 0.005f, 1e-4f);
    farPlane_ = distance_ + sceneRadius_ * 12.0f;
}

void Camera::roll(float deltaRad) { roll_ += deltaRad; }

void Camera::resetRoll() { roll_ = 0.0f; }

void Camera::setViewport(int width, int height) {
    viewportW_ = std::max(width, 1);
    viewportH_ = std::max(height, 1);
}

Vec3 Camera::position() const {
    const float cp = std::cos(pitch_);
    const Vec3 dir{cp * std::sin(yaw_), std::sin(pitch_), cp * std::cos(yaw_)};
    return target_ + dir * distance_;
}

Vec3 Camera::forward() const { return normalize(target_ - position()); }

Vec3 Camera::upVector() const {
    if (roll_ == 0.0f) return {0.0f, 1.0f, 0.0f};
    // Rotamos el "up" del mundo alrededor del eje de vision.
    const Vec3 fwd = normalize(target_ - position());
    const Vec3 worldUp{0.0f, 1.0f, 0.0f};
    const Vec3 right = normalize(cross(fwd, worldUp));
    const Vec3 up = cross(right, fwd);
    return normalize(up * std::cos(roll_) + right * std::sin(roll_));
}

Mat4 Camera::view() const { return lookAt(position(), target_, upVector()); }

Mat4 Camera::projection() const {
    const float aspect = static_cast<float>(viewportW_) / static_cast<float>(viewportH_);
    if (orthographic_) {
        const float halfH = distance_ * std::tan(radians(fovDeg_) * 0.5f);
        return orthographic(halfH * aspect, halfH, -farPlane_, farPlane_);
    }
    return perspective(radians(fovDeg_), aspect, nearPlane_, farPlane_);
}

float Camera::worldUnitsPerPixel() const {
    const float visibleHeight = 2.0f * distance_ * std::tan(radians(fovDeg_) * 0.5f);
    return visibleHeight / static_cast<float>(viewportH_);
}

float Camera::referenceDistance() const {
    // Misma cuenta que frameBounds(): la distancia que mete la esfera
    // envolvente del modelo en el campo de vision vertical, con un 15% de
    // aire. sceneRadius_ solo cambia al cargar un modelo nuevo, no al hacer
    // zoom, por eso sirve de ancla.
    const float halfFov = radians(fovDeg_) * 0.5f;
    return (sceneRadius_ / std::sin(halfFov)) * 1.15f;
}

}  // namespace uvp
