// Algebra lineal minima. Header-only, sin dependencias.
// Convencion: matrices column-major (igual que GLSL), vectores columna,
// asi que se puede subir directamente con glUniformMatrix4fv(..., GL_FALSE, m.m).
#pragma once

#include <cmath>
#include <algorithm>

namespace uvp {

constexpr float kPi = 3.14159265358979323846f;

inline float radians(float deg) { return deg * (kPi / 180.0f); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(Vec2 a) { return std::sqrt(dot(a, a)); }

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit Vec3(float s) : x(s), y(s), z(s) {}
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline Vec3& operator+=(Vec3& a, Vec3 b) { a = a + b; return a; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 normalize(Vec3 a) {
    const float len = length(a);
    return len > 1e-8f ? a * (1.0f / len) : Vec3(0.0f, 0.0f, 0.0f);
}
inline Vec3 minv(Vec3 a, Vec3 b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
inline Vec3 maxv(Vec3 a, Vec3 b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(Vec3 v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    Vec3 xyz() const { return {x, y, z}; }
};

// Column-major: m[col * 4 + row].
struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    static Mat4 identity() { return Mat4{}; }

    float& at(int col, int row) { return m[col * 4 + row]; }
    float at(int col, int row) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    }
    return r;
}

inline Vec4 operator*(const Mat4& a, const Vec4& v) {
    return {
        a.m[0] * v.x + a.m[4] * v.y + a.m[8] * v.z + a.m[12] * v.w,
        a.m[1] * v.x + a.m[5] * v.y + a.m[9] * v.z + a.m[13] * v.w,
        a.m[2] * v.x + a.m[6] * v.y + a.m[10] * v.z + a.m[14] * v.w,
        a.m[3] * v.x + a.m[7] * v.y + a.m[11] * v.z + a.m[15] * v.w,
    };
}

inline Mat4 translate(Vec3 t) {
    Mat4 r;
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}

inline Mat4 scaleMat(Vec3 s) {
    Mat4 r;
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
    return r;
}

// Cuaternion (x, y, z, w) -> matriz de rotacion. Lo usa el cargador glTF.
inline Mat4 quatToMat(float qx, float qy, float qz, float qw) {
    Mat4 r;
    const float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    const float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    const float wx = qw * qx, wy = qw * qy, wz = qw * qz;
    r.m[0] = 1.0f - 2.0f * (yy + zz); r.m[1] = 2.0f * (xy + wz);        r.m[2] = 2.0f * (xz - wy);
    r.m[4] = 2.0f * (xy - wz);        r.m[5] = 1.0f - 2.0f * (xx + zz); r.m[6] = 2.0f * (yz + wx);
    r.m[8] = 2.0f * (xz + wy);        r.m[9] = 2.0f * (yz - wx);        r.m[10] = 1.0f - 2.0f * (xx + yy);
    return r;
}

inline Mat4 perspective(float fovYRad, float aspect, float zNear, float zFar) {
    Mat4 r;
    for (float& v : r.m) v = 0.0f;
    const float f = 1.0f / std::tan(fovYRad * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return r;
}

inline Mat4 orthographic(float halfW, float halfH, float zNear, float zFar) {
    Mat4 r;
    r.m[0] = 1.0f / halfW;
    r.m[5] = 1.0f / halfH;
    r.m[10] = -2.0f / (zFar - zNear);
    r.m[14] = -(zFar + zNear) / (zFar - zNear);
    return r;
}

inline Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r;
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;  r.m[12] = -dot(s, eye);
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;  r.m[13] = -dot(u, eye);
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = dot(f, eye);
    r.m[3] = 0;    r.m[7] = 0;    r.m[11] = 0;    r.m[15] = 1;
    return r;
}

// Inversa general 4x4. Solo se usa una vez por frame, no vale la pena optimizarla.
inline Mat4 inverse(const Mat4& src) {
    const float* a = src.m;
    float inv[16];

    inv[0]  =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
    inv[4]  = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
    inv[8]  =  a[4]*a[9]*a[15]  - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
    inv[12] = -a[4]*a[9]*a[14]  + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
    inv[1]  = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
    inv[5]  =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
    inv[9]  = -a[0]*a[9]*a[15]  + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
    inv[13] =  a[0]*a[9]*a[14]  - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
    inv[2]  =  a[1]*a[6]*a[15]  - a[1]*a[7]*a[14]  - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7]  - a[13]*a[3]*a[6];
    inv[6]  = -a[0]*a[6]*a[15]  + a[0]*a[7]*a[14]  + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7]  + a[12]*a[3]*a[6];
    inv[10] =  a[0]*a[5]*a[15]  - a[0]*a[7]*a[13]  - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7]  - a[12]*a[3]*a[5];
    inv[14] = -a[0]*a[5]*a[14]  + a[0]*a[6]*a[13]  + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6]  + a[12]*a[2]*a[5];
    inv[3]  = -a[1]*a[6]*a[11]  + a[1]*a[7]*a[10]  + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7]   + a[9]*a[3]*a[6];
    inv[7]  =  a[0]*a[6]*a[11]  - a[0]*a[7]*a[10]  - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7]   - a[8]*a[3]*a[6];
    inv[11] = -a[0]*a[5]*a[11]  + a[0]*a[7]*a[9]   + a[4]*a[1]*a[11] - a[4]*a[3]*a[9]  - a[8]*a[1]*a[7]   + a[8]*a[3]*a[5];
    inv[15] =  a[0]*a[5]*a[10]  - a[0]*a[6]*a[9]   - a[4]*a[1]*a[10] + a[4]*a[2]*a[9]  + a[8]*a[1]*a[6]   - a[8]*a[2]*a[5];

    float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    Mat4 out;
    if (std::fabs(det) < 1e-12f) return out;  // singular: devolvemos identidad
    det = 1.0f / det;
    for (int i = 0; i < 16; ++i) out.m[i] = inv[i] * det;
    return out;
}

// Transpuesta de la inversa de la parte 3x3, para transformar normales.
inline Mat4 normalMatrix(const Mat4& model) {
    Mat4 inv = inverse(model);
    Mat4 r;
    for (int c = 0; c < 3; ++c)
        for (int row = 0; row < 3; ++row)
            r.at(c, row) = inv.at(row, c);
    r.at(3, 3) = 1.0f;
    return r;
}

struct Aabb {
    Vec3 min{1e30f, 1e30f, 1e30f};
    Vec3 max{-1e30f, -1e30f, -1e30f};

    void expand(Vec3 p) { min = minv(min, p); max = maxv(max, p); }
    bool valid() const { return min.x <= max.x; }
    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 extent() const { return max - min; }
    float radius() const { return valid() ? length(extent()) * 0.5f : 1.0f; }
};

}  // namespace uvp
