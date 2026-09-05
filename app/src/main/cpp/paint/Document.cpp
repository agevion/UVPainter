#include "paint/Document.h"

#include <algorithm>
#include <cstring>

#include "core/Log.h"
#include "render/Shaders.h"

namespace uvp {

Document::~Document() { destroy(); }

bool Document::create(int resolution) {
    destroy();

    // Potencias de dos entre 512 y 4096: mas alla no cabe comodamente en la
    // memoria de una tablet junto con varias capas.
    resolution_ = std::clamp(resolution, 512, 4096);

    if (!ensureShaders()) {
        LOGE("No se pudieron compilar los shaders del documento");
        resolution_ = 0;
        return false;
    }

    workFbo_.create();
    composite_.create(resolution_, resolution_, GL_RGBA8, GL_LINEAR);
    scratchA_.create(resolution_, resolution_, GL_RGBA8, GL_LINEAR);
    scratchB_.create(resolution_, resolution_, GL_RGBA8, GL_LINEAR);
    uvMask_.create(resolution_, resolution_, GL_R8, GL_LINEAR);

    // El etiquetado de regiones se hace a resolucion reducida: la pared la
    // dibuja una persona a mano, asi que dos texels de precision sobran, y a
    // cambio la lectura de vuelta baja de 16 MB a 4 MB.
    regionResolution_ = std::min(resolution_, 1024);
    boundaryMask_.create(resolution_, resolution_, GL_R8, GL_LINEAR);
    regionMap_.create(regionResolution_, regionResolution_, GL_RGBA8, GL_NEAREST);
    regionSource_.create(regionResolution_, regionResolution_, GL_RGBA8, GL_NEAREST);
    clearBoundary();

    const int background = addLayer("Fondo", -1);
    fillLayer(background, {0.62f, 0.60f, 0.58f, 1.0f});
    activeIndex_ = 0;

    compositeDirty_ = true;
    LOGI("Documento creado a %dx%d", resolution_, resolution_);
    return true;
}

void Document::destroy() {
    if (packFence_ != nullptr) {
        glDeleteSync(static_cast<GLsync>(packFence_));
        packFence_ = nullptr;
    }
    if (packBuffer_ != 0) {
        glDeleteBuffers(1, &packBuffer_);
        packBuffer_ = 0;
    }
    packBytes_ = 0;
    layers_.clear();
    composite_.destroy();
    scratchA_.destroy();
    scratchB_.destroy();
    uvMask_.destroy();
    thumbTex_.destroy();
    boundaryMask_.destroy();
    regionMap_.destroy();
    regionSource_.destroy();
    workFbo_.destroy();
    compositeShader_.destroy();
    dilateShader_.destroy();
    uvMaskShader_.destroy();
    copyShader_.destroy();
    thumbShader_.destroy();
    regionSourceShader_.destroy();
    shadersReady_ = false;
    thumbSize_ = 0;
    hasRegions_ = false;
    regionsDirty_ = true;
    boundaryDrawn_ = false;
    resolution_ = 0;
    activeIndex_ = 0;
    nextLayerId_ = 1;
}

// ---------------------------------------------------------------------------
// Estado fuera de la GPU
//
// Todo lo pintado vive en texturas, y las texturas mueren con el contexto EGL.
// Basta con que la app pase a segundo plano (abrir el selector de archivos ya
// lo hace) para perderlo todo, asi que hay que saber bajarlo a CPU y volver a
// subirlo. La misma pareja de funciones sirve para guardar el proyecto.
// ---------------------------------------------------------------------------
bool Document::captureState(DocumentState& out) {
    out.clear();
    if (!valid()) return false;

    out.resolution = resolution_;
    out.activeIndex = activeIndex_;
    out.nextLayerId = nextLayerId_;
    out.layers.resize(layers_.size());

    for (size_t i = 0; i < layers_.size(); ++i) {
        const Layer* layer = layers_[i].get();
        out.layers[i].info = layer->info;
        out.layers[i].id = layer->id;
        if (!readRegion(layer->texture, 0, 0, resolution_, resolution_, out.layers[i].pixels)) {
            LOGE("No se pudo volcar la capa %zu", i);
            out.clear();
            return false;
        }
    }

    if (boundaryDrawn_) readBoundary(out.boundary);
    return true;
}

bool Document::restoreState(const DocumentState& state) {
    if (state.empty()) return false;
    if (!create(state.resolution)) return false;

    // create() deja una capa de fondo: fuera, las buenas son las del estado.
    layers_.clear();
    const size_t pixelCount = static_cast<size_t>(resolution_) *
                              static_cast<size_t>(resolution_) * 4u;

    for (const LayerState& saved : state.layers) {
        if (saved.pixels.size() < pixelCount) {
            LOGE("Capa guardada con %zu bytes, se esperaban %zu", saved.pixels.size(), pixelCount);
            continue;
        }
        auto layer = std::make_unique<Layer>();
        layer->id = saved.id;
        layer->info = saved.info;
        layer->texture.create(resolution_, resolution_, GL_RGBA8, GL_LINEAR);
        layer->texture.upload(saved.pixels.data());
        layers_.push_back(std::move(layer));
    }
    if (layers_.empty()) {
        // Mejor un documento vacio que uno sin capas, que no admite pintura.
        const int background = addLayer("Fondo", -1);
        fillLayer(background, {0.62f, 0.60f, 0.58f, 1.0f});
    }

    nextLayerId_ = std::max(state.nextLayerId, 1);
    for (const auto& layer : layers_) nextLayerId_ = std::max(nextLayerId_, layer->id + 1);
    activeIndex_ = std::clamp(state.activeIndex, 0, layerCount() - 1);

    if (!state.boundary.empty()) uploadBoundary(state.boundary);

    compositeDirty_ = true;
    LOGI("Documento restaurado: %d capas a %dpx%s", layerCount(), resolution_,
         state.boundary.empty() ? "" : " (con limites)");
    return true;
}

bool Document::readBoundary(std::vector<uint8_t>& out) {
    out.clear();
    if (!boundaryMask_.valid()) return false;

    // glReadPixels solo garantiza GL_RGBA/GL_UNSIGNED_BYTE, asi que se lee en
    // cuatro canales y se guarda solo el rojo: el resto es relleno.
    std::vector<uint8_t> rgba;
    if (!readRegion(boundaryMask_, 0, 0, resolution_, resolution_, rgba)) return false;

    const size_t count = static_cast<size_t>(resolution_) * static_cast<size_t>(resolution_);
    out.resize(count);
    for (size_t i = 0; i < count; ++i) out[i] = rgba[i * 4u];
    return true;
}

void Document::uploadBoundary(const std::vector<uint8_t>& mask) {
    const size_t count = static_cast<size_t>(resolution_) * static_cast<size_t>(resolution_);
    if (!boundaryMask_.valid() || mask.size() < count) return;
    boundaryMask_.upload(mask.data(), GL_RED);
    boundaryDrawn_ = true;
    regionsDirty_ = true;
}

bool Document::ensureShaders() {
    if (shadersReady_) return true;
    bool ok = true;
    ok &= compositeShader_.compile(shaders::kFullscreenVS, shaders::kLayerCompositeFS, "composite");
    ok &= dilateShader_.compile(shaders::kFullscreenVS, shaders::kDilateFS, "dilate");
    ok &= uvMaskShader_.compile(shaders::kUvMaskVS, shaders::kUvMaskFS, "uvmask");
    ok &= copyShader_.compile(shaders::kFullscreenVS, shaders::kCopyFS, "copy");
    ok &= thumbShader_.compile(shaders::kFullscreenVS, shaders::kThumbnailFS, "thumbnail");
    ok &= regionSourceShader_.compile(shaders::kFullscreenVS, shaders::kRegionSourceFS,
                                      "regionSource");
    shadersReady_ = ok;
    return ok;
}

// ---------------------------------------------------------------------------
// Capas
// ---------------------------------------------------------------------------
Layer* Document::layerAt(int index) {
    if (index < 0 || index >= layerCount()) return nullptr;
    return layers_[static_cast<size_t>(index)].get();
}

const Layer* Document::layerAt(int index) const {
    if (index < 0 || index >= layerCount()) return nullptr;
    return layers_[static_cast<size_t>(index)].get();
}

void Document::setActiveIndex(int index) {
    if (index >= 0 && index < layerCount()) activeIndex_ = index;
}

int Document::addLayer(const std::string& name, int insertAbove) {
    if (resolution_ <= 0) return -1;

    auto layer = std::make_unique<Layer>();
    layer->id = nextLayerId_++;
    layer->info.name = name;
    layer->texture.create(resolution_, resolution_, GL_RGBA8, GL_LINEAR);

    // Toda capa nueva nace vacia y transparente.
    bindTargetTexture(layer->texture);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, resolution_, resolution_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Framebuffer::unbind();

    const int index = (insertAbove < 0 || insertAbove >= layerCount())
                          ? layerCount()
                          : insertAbove + 1;
    layers_.insert(layers_.begin() + index, std::move(layer));
    activeIndex_ = index;
    compositeDirty_ = true;
    return index;
}

bool Document::removeLayer(int index) {
    if (index < 0 || index >= layerCount() || layerCount() <= 1) return false;
    layers_.erase(layers_.begin() + index);
    activeIndex_ = std::clamp(activeIndex_, 0, layerCount() - 1);
    compositeDirty_ = true;
    return true;
}

bool Document::moveLayer(int from, int to) {
    if (from < 0 || from >= layerCount() || to < 0 || to >= layerCount() || from == to) {
        return false;
    }
    auto moved = std::move(layers_[static_cast<size_t>(from)]);
    layers_.erase(layers_.begin() + from);
    layers_.insert(layers_.begin() + to, std::move(moved));
    activeIndex_ = to;
    compositeDirty_ = true;
    return true;
}

bool Document::duplicateLayer(int index) {
    const Layer* src = layerAt(index);
    if (src == nullptr) return false;

    // La coletilla se escribe aqui en castellano y la interfaz la reconoce y la
    // traduce al pintarla (ver i18n/Strings.kt): el nombre viaja dentro del
    // documento, asi que no puede depender del idioma que hubiera al duplicar.
    const int newIndex = addLayer(src->info.name + " copia", index);
    Layer* dst = layerAt(newIndex);
    if (dst == nullptr) return false;
    dst->info = src->info;
    dst->info.name = src->info.name + " copia";

    bindTargetTexture(dst->texture);
    glViewport(0, 0, resolution_, resolution_);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    copyShader_.bind();
    src->texture.bind(0);
    copyShader_.set("uTex", 0);
    drawFullscreenTriangle();
    Framebuffer::unbind();

    compositeDirty_ = true;
    return true;
}

void Document::clearLayer(int index) {
    Layer* layer = layerAt(index);
    if (layer == nullptr) return;
    bindTargetTexture(layer->texture);
    glViewport(0, 0, resolution_, resolution_);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Framebuffer::unbind();
    compositeDirty_ = true;
}

void Document::fillLayer(int index, Vec4 color) {
    Layer* layer = layerAt(index);
    if (layer == nullptr) return;
    bindTargetTexture(layer->texture);
    glViewport(0, 0, resolution_, resolution_);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(color.x, color.y, color.z, color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    Framebuffer::unbind();
    compositeDirty_ = true;
}

// ---------------------------------------------------------------------------
// Composicion
// ---------------------------------------------------------------------------
void Document::bindTargetTexture(const Texture2D& tex) {
    workFbo_.bind();
    workFbo_.attachColor(tex, 0);
    const GLenum buffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, buffers);
}

const Texture2D& Document::composite() {
    if (!compositeDirty_ || resolution_ <= 0) return composite_;

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, resolution_, resolution_);

    // Empezamos en negro transparente y vamos mezclando de abajo a arriba,
    // alternando entre scratchA y scratchB.
    bindTargetTexture(scratchA_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Texture2D* src = &scratchA_;
    Texture2D* dst = &scratchB_;
    bool anyDrawn = false;

    compositeShader_.bind();
    for (const auto& layer : layers_) {
        if (!layer->info.visible || layer->info.opacity <= 0.0f) continue;

        bindTargetTexture(*dst);
        src->bind(0);
        layer->texture.bind(1);
        compositeShader_.set("uDst", 0);
        compositeShader_.set("uSrc", 1);
        compositeShader_.set("uOpacity", layer->info.opacity);
        compositeShader_.set("uBlendMode", static_cast<int>(layer->info.blend));
        compositeShader_.set("uClipToBelow", layer->info.clipToBelow ? 1 : 0);
        drawFullscreenTriangle();

        std::swap(src, dst);
        anyDrawn = true;
    }

    // El resultado acaba en `src`; lo copiamos a la textura de composicion.
    bindTargetTexture(composite_);
    if (anyDrawn) {
        copyShader_.bind();
        src->bind(0);
        copyShader_.set("uTex", 0);
        drawFullscreenTriangle();
    } else {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Un par de pasadas de dilatado para que las costuras no se vean en el 3D.
    dilate(composite_, 2);

    Framebuffer::unbind();
    compositeDirty_ = false;
    GL_CHECK("Document::composite");
    return composite_;
}

void Document::buildUvMask(const Mesh& mesh) {
    if (resolution_ <= 0 || !mesh.valid()) return;

    bindTargetTexture(uvMask_);
    glViewport(0, 0, resolution_, resolution_);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    uvMaskShader_.bind();
    mesh.draw();

    Framebuffer::unbind();
    GL_CHECK("Document::buildUvMask");
}

void Document::dilate(Texture2D& target, int iterations) {
    if (iterations <= 0 || resolution_ <= 0) return;

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, resolution_, resolution_);

    const float texel = 1.0f / static_cast<float>(resolution_);
    dilateShader_.bind();
    dilateShader_.set("uTexelSize", Vec2(texel, texel));

    for (int i = 0; i < iterations; ++i) {
        // target -> scratchA
        bindTargetTexture(scratchA_);
        target.bind(0);
        dilateShader_.set("uTex", 0);
        drawFullscreenTriangle();

        // scratchA -> target
        bindTargetTexture(target);
        copyShader_.bind();
        scratchA_.bind(0);
        copyShader_.set("uTex", 0);
        drawFullscreenTriangle();
        dilateShader_.bind();
        dilateShader_.set("uTexelSize", Vec2(texel, texel));
    }
    GL_CHECK("Document::dilate");
}

// ---------------------------------------------------------------------------
// Lectura hacia CPU
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Lectura sin esperas
// ---------------------------------------------------------------------------
bool Document::beginReadback(const Texture2D& tex) {
    if (!tex.valid() || resolution_ <= 0 || packFence_ != nullptr) return false;

    const size_t bytes = static_cast<size_t>(resolution_) * static_cast<size_t>(resolution_) * 4u;
    if (packBuffer_ == 0) glGenBuffers(1, &packBuffer_);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, packBuffer_);
    if (packBytes_ != bytes) {
        glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(bytes), nullptr, GL_STREAM_READ);
        packBytes_ = bytes;
    }

    bindTargetTexture(tex);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // El puntero nulo no es un descuido: con un buffer de empaquetado atado, el
    // ultimo argumento deja de ser una direccion de memoria y pasa a ser un
    // desplazamiento dentro del buffer.
    glReadPixels(0, 0, resolution_, resolution_, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    Framebuffer::unbind();
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    packFence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    // Sin esto la valla puede quedarse en la cola del driver sin llegar a la
    // GPU, y entonces no se cumple nunca por mucho que se pregunte.
    glFlush();
    GL_CHECK("Document::beginReadback");
    return packFence_ != nullptr;
}

bool Document::readbackReady() {
    if (packFence_ == nullptr) return false;
    const GLenum result = glClientWaitSync(static_cast<GLsync>(packFence_), 0, 0);
    return result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED;
}

bool Document::finishReadback(std::vector<uint8_t>& out) {
    if (packFence_ == nullptr || packBuffer_ == 0 || packBytes_ == 0) return false;

    glDeleteSync(static_cast<GLsync>(packFence_));
    packFence_ = nullptr;

    glBindBuffer(GL_PIXEL_PACK_BUFFER, packBuffer_);
    void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0,
                                    static_cast<GLsizeiptr>(packBytes_), GL_MAP_READ_BIT);
    if (mapped == nullptr) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        LOGE("No se pudo mapear el buffer de lectura");
        return false;
    }
    out.resize(packBytes_);
    std::memcpy(out.data(), mapped, packBytes_);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    GL_CHECK("Document::finishReadback");
    return true;
}

bool Document::readRegion(const Texture2D& tex, int x, int y, int w, int h,
                          std::vector<uint8_t>& out) {
    if (!tex.valid() || w <= 0 || h <= 0) return false;
    bindTargetTexture(tex);
    out.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    Framebuffer::unbind();
    return true;
}

void Document::writeRegion(Texture2D& tex, int x, int y, int w, int h, const uint8_t* pixels) {
    if (!tex.valid() || pixels == nullptr) return;
    tex.uploadSub(x, y, w, h, pixels);
    compositeDirty_ = true;
}

bool Document::readComposite(std::vector<uint8_t>& out, bool dilateForExport, int dilatePasses) {
    if (resolution_ <= 0) return false;
    composite();
    if (dilateForExport) dilate(composite_, dilatePasses);
    const bool ok = readRegion(composite_, 0, 0, resolution_, resolution_, out);
    // El dilatado extra ensucia la textura de pantalla: la recomponemos.
    if (dilateForExport) compositeDirty_ = true;
    return ok;
}

bool Document::readLayer(int index, std::vector<uint8_t>& out) {
    const Layer* layer = layerAt(index);
    if (layer == nullptr) return false;
    return readRegion(layer->texture, 0, 0, resolution_, resolution_, out);
}

bool Document::readLayerThumbnail(int index, int size, std::vector<uint8_t>& out) {
    const Layer* layer = layerAt(index);
    if (layer == nullptr || size <= 0 || resolution_ <= 0) return false;

    if (thumbSize_ != size) {
        thumbTex_.create(size, size, GL_RGBA8, GL_LINEAR);
        thumbSize_ = size;
    }

    bindTargetTexture(thumbTex_);
    glViewport(0, 0, size, size);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    thumbShader_.bind();
    layer->texture.bind(0);
    thumbShader_.set("uTex", 0);
    const float texel = 1.0f / static_cast<float>(resolution_);
    thumbShader_.set("uSourceTexel", Vec2(texel, texel));
    thumbShader_.set("uFootprint", static_cast<float>(resolution_) / static_cast<float>(size));
    drawFullscreenTriangle();

    out.resize(static_cast<size_t>(size) * static_cast<size_t>(size) * 4u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    Framebuffer::unbind();
    return true;
}

// ---------------------------------------------------------------------------
// Limites dibujados a mano
// ---------------------------------------------------------------------------
void Document::clearBoundary() {
    if (!boundaryMask_.valid()) return;
    bindTargetTexture(boundaryMask_);
    glViewport(0, 0, resolution_, resolution_);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Framebuffer::unbind();
    hasRegions_ = false;
    regionsDirty_ = true;
    boundaryDrawn_ = false;
}

bool Document::ensureRegions() {
    if (!regionsDirty_) return hasRegions_;
    regionsDirty_ = false;
    if (!boundaryMask_.valid() || !uvMask_.valid()) return false;

    const int side = regionResolution_;

    // 1. Cobertura UV y pared, reducidas y combinadas en una sola textura.
    bindTargetTexture(regionSource_);
    glViewport(0, 0, side, side);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    regionSourceShader_.bind();
    uvMask_.bind(0);
    boundaryMask_.bind(1);
    regionSourceShader_.set("uCoverage", 0);
    regionSourceShader_.set("uBoundary", 1);
    const float texel = 1.0f / static_cast<float>(resolution_);
    regionSourceShader_.set("uTexel", Vec2(texel, texel));
    drawFullscreenTriangle();

    std::vector<uint8_t> src(static_cast<size_t>(side) * static_cast<size_t>(side) * 4u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, src.data());
    Framebuffer::unbind();

    // 2. Etiquetado de componentes conexas en CPU. Se hace con una pila propia
    //    en vez de recursion: un area grande desbordaria la del hilo.
    const size_t count = static_cast<size_t>(side) * static_cast<size_t>(side);
    std::vector<uint16_t> labels(count, 0);
    std::vector<int32_t> stack;
    stack.reserve(4096);

    bool anyBoundary = false;
    uint16_t nextLabel = 0;
    for (size_t i = 0; i < count; ++i) {
        if (src[i * 4u + 1u] > 96) anyBoundary = true;
    }
    if (!anyBoundary) {
        hasRegions_ = false;
        return false;
    }

    auto paintable = [&](int32_t index) {
        return src[static_cast<size_t>(index) * 4u] > 8 &&      // hay geometria
               src[static_cast<size_t>(index) * 4u + 1u] <= 96;  // y no es pared
    };

    for (int32_t seed = 0; seed < static_cast<int32_t>(count); ++seed) {
        if (labels[static_cast<size_t>(seed)] != 0 || !paintable(seed)) continue;
        if (nextLabel == 0xFFFF) break;  // tope de etiquetas representables
        ++nextLabel;

        stack.clear();
        stack.push_back(seed);
        labels[static_cast<size_t>(seed)] = nextLabel;

        while (!stack.empty()) {
            const int32_t current = stack.back();
            stack.pop_back();
            const int32_t x = current % side;
            const int32_t y = current / side;

            const int32_t neighbours[4] = {
                x > 0 ? current - 1 : -1,
                x < side - 1 ? current + 1 : -1,
                y > 0 ? current - side : -1,
                y < side - 1 ? current + side : -1,
            };
            for (const int32_t n : neighbours) {
                if (n < 0) continue;
                if (labels[static_cast<size_t>(n)] != 0 || !paintable(n)) continue;
                labels[static_cast<size_t>(n)] = nextLabel;
                stack.push_back(n);
            }
        }
    }

    // 3. De vuelta a GPU: la etiqueta ocupa dos canales de 8 bits.
    std::vector<uint8_t> encoded(count * 4u, 0);
    for (size_t i = 0; i < count; ++i) {
        const uint16_t label = labels[i];
        encoded[i * 4u] = static_cast<uint8_t>(label & 0xFF);
        encoded[i * 4u + 1u] = static_cast<uint8_t>(label >> 8);
        encoded[i * 4u + 3u] = 255;
    }
    regionMap_.upload(encoded.data());

    hasRegions_ = nextLabel > 1;
    LOGI("Regiones delimitadas: %d (mapa %dx%d)", static_cast<int>(nextLabel), side, side);
    return hasRegions_;
}

}  // namespace uvp
