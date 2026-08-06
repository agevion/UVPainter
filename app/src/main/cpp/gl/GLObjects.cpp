#include "gl/GLObjects.h"

#include <algorithm>

#include "core/Log.h"

namespace uvp {

void checkGlError(const char* where) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        LOGE("GL error 0x%04x en %s", err, where);
    }
}

// ---------------------------------------------------------------------------
// Shader
// ---------------------------------------------------------------------------
namespace {

GLuint compileStage(GLenum type, const char* src, const char* debugName) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(static_cast<size_t>(logLen > 1 ? logLen : 1), '\0');
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        const char* stageName = type == GL_VERTEX_SHADER     ? "vertex"
                                : type == GL_FRAGMENT_SHADER ? "fragment"
                                                             : "compute";
        LOGE("Fallo compilando shader %s (%s):\n%s", debugName, stageName, log.c_str());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool linkProgram(GLuint program, const char* debugName) {
    glLinkProgram(program);
    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(static_cast<size_t>(logLen > 1 ? logLen : 1), '\0');
        glGetProgramInfoLog(program, logLen, nullptr, log.data());
        LOGE("Fallo enlazando programa %s:\n%s", debugName, log.c_str());
        return false;
    }
    return true;
}

}  // namespace

Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        destroy();
        program_ = o.program_;
        uniformCache_ = std::move(o.uniformCache_);
        o.program_ = 0;
    }
    return *this;
}

bool Shader::compile(const char* vertexSrc, const char* fragmentSrc, const char* debugName) {
    destroy();

    const GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSrc, debugName);
    if (vs == 0) return false;
    const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc, debugName);
    if (fs == 0) {
        glDeleteShader(vs);
        return false;
    }

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    const bool ok = linkProgram(prog, debugName);
    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!ok) {
        glDeleteProgram(prog);
        return false;
    }
    program_ = prog;
    LOGD("Shader '%s' compilado (id=%u)", debugName, program_);
    return true;
}

bool Shader::compileCompute(const char* computeSrc, const char* debugName) {
    destroy();
    const GLuint cs = compileStage(GL_COMPUTE_SHADER, computeSrc, debugName);
    if (cs == 0) return false;

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    const bool ok = linkProgram(prog, debugName);
    glDetachShader(prog, cs);
    glDeleteShader(cs);

    if (!ok) {
        glDeleteProgram(prog);
        return false;
    }
    program_ = prog;
    return true;
}

void Shader::destroy() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    uniformCache_.clear();
}

GLint Shader::uniform(const char* name) const {
    for (const auto& entry : uniformCache_) {
        if (entry.first == name) return entry.second;
    }
    const GLint loc = glGetUniformLocation(program_, name);
    uniformCache_.emplace_back(name, loc);
    return loc;
}

// ---------------------------------------------------------------------------
// Texture2D
// ---------------------------------------------------------------------------
Texture2D& Texture2D::operator=(Texture2D&& o) noexcept {
    if (this != &o) {
        destroy();
        tex_ = o.tex_;
        width_ = o.width_;
        height_ = o.height_;
        internalFormat_ = o.internalFormat_;
        o.tex_ = 0;
        o.width_ = o.height_ = 0;
    }
    return *this;
}

void Texture2D::create(int width, int height, GLenum internalFormat, GLenum filter, GLenum wrap) {
    destroy();
    width_ = width;
    height_ = height;
    internalFormat_ = internalFormat;

    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    // Almacenamiento inmutable: el driver puede optimizar mejor y evita
    // reasignaciones accidentales al pintar.
    glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_CHECK("Texture2D::create");
}

void Texture2D::destroy() {
    if (tex_ != 0) {
        glDeleteTextures(1, &tex_);
        tex_ = 0;
    }
    width_ = height_ = 0;
}

void Texture2D::upload(const void* pixels, GLenum format, GLenum type) {
    if (tex_ == 0) return;
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, format, type, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_CHECK("Texture2D::upload");
}

void Texture2D::uploadSub(int x, int y, int w, int h, const void* pixels, GLenum format,
                          GLenum type) {
    if (tex_ == 0 || w <= 0 || h <= 0) return;
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, format, type, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_CHECK("Texture2D::uploadSub");
}

void Texture2D::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex_);
}

void Texture2D::generateMipmaps() {
    if (tex_ == 0) return;
    glBindTexture(GL_TEXTURE_2D, tex_);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ---------------------------------------------------------------------------
// Framebuffer
// ---------------------------------------------------------------------------
void Framebuffer::create() {
    destroy();
    glGenFramebuffers(1, &fbo_);
}

void Framebuffer::destroy() {
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}

void Framebuffer::attachColor(const Texture2D& tex, int slot) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_TEXTURE_2D, tex.id(), 0);
}

void Framebuffer::attachDepth(const Texture2D& tex) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex.id(), 0);
}

void Framebuffer::detachDepth() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
}

bool Framebuffer::isComplete() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("Framebuffer incompleto: 0x%04x", status);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Mesh
// ---------------------------------------------------------------------------
void Mesh::upload(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    destroy();
    if (vertices.empty() || indices.empty()) return;

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, island)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    indexCount_ = static_cast<int>(indices.size());
    cpuIndices_ = indices;
    GL_CHECK("Mesh::upload");
}

void Mesh::destroy() {
    if (seamEbo_ != 0) glDeleteBuffers(1, &seamEbo_);
    if (wireEbo_ != 0) glDeleteBuffers(1, &wireEbo_);
    if (wireVao_ != 0) glDeleteVertexArrays(1, &wireVao_);
    if (ebo_ != 0) glDeleteBuffers(1, &ebo_);
    if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
    if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
    seamEbo_ = wireEbo_ = wireVao_ = ebo_ = vbo_ = vao_ = 0;
    indexCount_ = 0;
    wireIndexCount_ = 0;
    seamIndexCount_ = 0;
    cpuIndices_.clear();
    cpuIndices_.shrink_to_fit();
}

void Mesh::draw() const {
    if (vao_ == 0) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::buildWireframe() {
    if (vbo_ == 0 || cpuIndices_.empty()) return;

    // Aristas unicas: se ordena el par para que (a,b) y (b,a) colisionen.
    std::vector<uint64_t> edges;
    edges.reserve(cpuIndices_.size());
    for (size_t i = 0; i + 2 < cpuIndices_.size(); i += 3) {
        const uint32_t tri[3] = {cpuIndices_[i], cpuIndices_[i + 1], cpuIndices_[i + 2]};
        for (int e = 0; e < 3; ++e) {
            uint32_t a = tri[e];
            uint32_t b = tri[(e + 1) % 3];
            if (a > b) std::swap(a, b);
            edges.push_back((static_cast<uint64_t>(a) << 32) | b);
        }
    }
    std::sort(edges.begin(), edges.end());

    std::vector<uint32_t> lineIndices;
    std::vector<uint32_t> seamIndices;
    lineIndices.reserve(edges.size() * 2);

    // Una arista usada por un solo triangulo es un borde de isla UV: ahi es
    // justo donde la malla se corto al desplegarla. Son las costuras.
    size_t i = 0;
    while (i < edges.size()) {
        size_t j = i + 1;
        while (j < edges.size() && edges[j] == edges[i]) ++j;

        const uint32_t a = static_cast<uint32_t>(edges[i] >> 32);
        const uint32_t b = static_cast<uint32_t>(edges[i] & 0xFFFFFFFFu);
        lineIndices.push_back(a);
        lineIndices.push_back(b);
        if (j - i == 1) {
            seamIndices.push_back(a);
            seamIndices.push_back(b);
        }
        i = j;
    }

    if (wireVao_ == 0) glGenVertexArrays(1, &wireVao_);
    glBindVertexArray(wireVao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    if (wireEbo_ == 0) glGenBuffers(1, &wireEbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireEbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(lineIndices.size() * sizeof(uint32_t)),
                 lineIndices.data(), GL_STATIC_DRAW);

    if (seamEbo_ == 0) glGenBuffers(1, &seamEbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, seamEbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(seamIndices.size() * sizeof(uint32_t)),
                 seamIndices.empty() ? nullptr : seamIndices.data(), GL_STATIC_DRAW);
    seamIndexCount_ = static_cast<int>(seamIndices.size());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireEbo_);

    constexpr GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, island)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    wireIndexCount_ = static_cast<int>(lineIndices.size());
    GL_CHECK("Mesh::buildWireframe");
}

void Mesh::drawWireframe() const {
    if (wireVao_ == 0 || wireIndexCount_ == 0) return;
    glBindVertexArray(wireVao_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireEbo_);
    glDrawElements(GL_LINES, wireIndexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::drawSeams() const {
    if (wireVao_ == 0 || seamIndexCount_ == 0) return;
    glBindVertexArray(wireVao_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, seamEbo_);
    glDrawElements(GL_LINES, seamIndexCount_, GL_UNSIGNED_INT, nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireEbo_);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Triangulo de pantalla completa
// ---------------------------------------------------------------------------
namespace {
GLuint g_fullscreenVao = 0;
}

void drawFullscreenTriangle() {
    if (g_fullscreenVao == 0) glGenVertexArrays(1, &g_fullscreenVao);
    glBindVertexArray(g_fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void destroyFullscreenVao() {
    if (g_fullscreenVao != 0) {
        glDeleteVertexArrays(1, &g_fullscreenVao);
        g_fullscreenVao = 0;
    }
}

}  // namespace uvp
