// Envoltorios RAII finos sobre objetos de OpenGL ES 3.2.
// Nada de herencia ni virtuales: esto va en el camino caliente de cada frame.
#pragma once

#include <GLES3/gl32.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/Math.h"

namespace uvp {

void checkGlError(const char* where);

#ifdef NDEBUG
#define GL_CHECK(where) ((void)0)
#else
#define GL_CHECK(where) ::uvp::checkGlError(where)
#endif

// ---------------------------------------------------------------------------
// Shader
// ---------------------------------------------------------------------------
class Shader {
public:
    Shader() = default;
    ~Shader() { destroy(); }

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& o) noexcept { *this = std::move(o); }
    Shader& operator=(Shader&& o) noexcept;

    bool compile(const char* vertexSrc, const char* fragmentSrc, const char* debugName);
    bool compileCompute(const char* computeSrc, const char* debugName);
    void destroy();

    void bind() const { glUseProgram(program_); }
    GLuint id() const { return program_; }
    bool valid() const { return program_ != 0; }

    GLint uniform(const char* name) const;

    void set(const char* name, int v) const { glUniform1i(uniform(name), v); }
    void set(const char* name, float v) const { glUniform1f(uniform(name), v); }
    void set(const char* name, Vec2 v) const { glUniform2f(uniform(name), v.x, v.y); }
    void set(const char* name, Vec3 v) const { glUniform3f(uniform(name), v.x, v.y, v.z); }
    void set(const char* name, Vec4 v) const { glUniform4f(uniform(name), v.x, v.y, v.z, v.w); }
    void set(const char* name, const Mat4& v) const {
        glUniformMatrix4fv(uniform(name), 1, GL_FALSE, v.m);
    }
    void setArray(const char* name, const float* data, int count) const {
        glUniform1fv(uniform(name), count, data);
    }
    void setVec4Array(const char* name, const float* data, int count) const {
        glUniform4fv(uniform(name), count, data);
    }

private:
    GLuint program_ = 0;
    // Cache de localizaciones: glGetUniformLocation por frame es caro de mas.
    mutable std::vector<std::pair<std::string, GLint>> uniformCache_;
};

// ---------------------------------------------------------------------------
// Textura 2D
// ---------------------------------------------------------------------------
class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D() { destroy(); }

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&& o) noexcept { *this = std::move(o); }
    Texture2D& operator=(Texture2D&& o) noexcept;

    // internalFormat: GL_RGBA8, GL_R8, GL_RGBA16F, GL_DEPTH_COMPONENT24...
    void create(int width, int height, GLenum internalFormat,
                GLenum filter = GL_LINEAR, GLenum wrap = GL_CLAMP_TO_EDGE);
    void destroy();

    void upload(const void* pixels, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE);
    void uploadSub(int x, int y, int w, int h, const void* pixels,
                   GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE);
    void bind(int unit) const;
    void generateMipmaps();

    GLuint id() const { return tex_; }
    int width() const { return width_; }
    int height() const { return height_; }
    GLenum internalFormat() const { return internalFormat_; }
    bool valid() const { return tex_ != 0; }

private:
    GLuint tex_ = 0;
    int width_ = 0;
    int height_ = 0;
    GLenum internalFormat_ = 0;
};

// ---------------------------------------------------------------------------
// Framebuffer con adjuntos opcionales
// ---------------------------------------------------------------------------
class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer() { destroy(); }

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void create();
    void destroy();

    // Adjunta una textura de color existente (no toma posesion).
    void attachColor(const Texture2D& tex, int slot = 0);
    void attachDepth(const Texture2D& tex);
    void detachDepth();
    bool isComplete() const;

    void bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo_); }
    static void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
    GLuint id() const { return fbo_; }

private:
    GLuint fbo_ = 0;
};

// ---------------------------------------------------------------------------
// Malla estatica indexada
// ---------------------------------------------------------------------------
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    /// Indice de la isla UV a la que pertenece. Lo usan el pincel para no
    /// invadir zonas vecinas y el bote de pintura para rellenar solo una.
    float island = 0.0f;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh() { destroy(); }

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void upload(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void destroy();
    void draw() const;

    // Construye los buffers de lineas para la vista de malla y para las
    // costuras UV. Comparten el mismo VBO de vertices.
    void buildWireframe();
    void drawWireframe() const;
    void drawSeams() const;
    bool hasWireframe() const { return wireVao_ != 0; }
    bool hasSeams() const { return seamIndexCount_ > 0; }

    int indexCount() const { return indexCount_; }
    bool valid() const { return vao_ != 0; }

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    int indexCount_ = 0;

    GLuint wireVao_ = 0;
    GLuint wireEbo_ = 0;
    int wireIndexCount_ = 0;
    GLuint seamEbo_ = 0;
    int seamIndexCount_ = 0;
    std::vector<uint32_t> cpuIndices_;
};

// Quad de pantalla completa sin VBO (se genera en el vertex shader con gl_VertexID).
// Requiere un VAO vacio ligado; drawFullscreenTriangle se encarga.
void drawFullscreenTriangle();
void destroyFullscreenVao();

}  // namespace uvp
