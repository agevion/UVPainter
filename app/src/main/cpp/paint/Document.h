// El documento: la pila de capas que vive en el atlas UV, su composicion y
// las utilidades de dilatado/exportacion.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "gl/GLObjects.h"
#include "paint/Types.h"

namespace uvp {

class Mesh;

struct Layer {
    Texture2D texture;
    LayerInfo info;
    int id = 0;
};

/// Una capa bajada a memoria de CPU. Es lo que viaja al archivo de proyecto y
/// lo unico que sobrevive a que Android se lleve el contexto EGL: las texturas
/// mueren con el contexto, estos bytes no.
struct LayerState {
    LayerInfo info;
    int id = 0;
    std::vector<uint8_t> pixels;  // RGBA8, resolution x resolution
};

struct DocumentState {
    int resolution = 0;
    int activeIndex = 0;
    int nextLayerId = 1;
    std::vector<LayerState> layers;
    /// Pared dibujada a mano, un byte por texel (el canal rojo de la mascara).
    std::vector<uint8_t> boundary;

    bool empty() const { return layers.empty(); }
    void clear() { *this = DocumentState{}; }
};

class Document {
public:
    Document() = default;
    ~Document();

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // Crea el documento y su capa de fondo. Requiere contexto GL activo.
    bool create(int resolution);
    void destroy();

    bool valid() const { return resolution_ > 0 && !layers_.empty(); }
    int resolution() const { return resolution_; }

    // --- Capas ---------------------------------------------------------
    int layerCount() const { return static_cast<int>(layers_.size()); }
    Layer* layerAt(int index);
    const Layer* layerAt(int index) const;
    Layer* activeLayer() { return layerAt(activeIndex_); }
    int activeIndex() const { return activeIndex_; }
    int nextLayerId() const { return nextLayerId_; }
    void setActiveIndex(int index);

    int addLayer(const std::string& name, int insertAbove);
    bool removeLayer(int index);
    bool moveLayer(int from, int to);
    bool duplicateLayer(int index);
    void clearLayer(int index);
    void fillLayer(int index, Vec4 color);

    // --- Composicion ---------------------------------------------------
    void markDirty() { compositeDirty_ = true; }
    // Recompone la pila si hace falta y devuelve la textura resultante.
    const Texture2D& composite();
    const Texture2D& compositeTexture() const { return composite_; }

    // Rasteriza la malla en espacio UV para saber que texels tienen geometria.
    void buildUvMask(const Mesh& mesh);
    const Texture2D& uvMask() const { return uvMask_; }

    // Expande el color hacia fuera de las islas UV (arregla costuras).
    void dilate(Texture2D& target, int iterations);

    // Vuelca la composicion (ya dilatada) a RGBA8 en memoria de CPU.
    bool readComposite(std::vector<uint8_t>& out, bool dilateForExport, int dilatePasses);
    // Vuelca una capa concreta.
    bool readLayer(int index, std::vector<uint8_t>& out);
    // Miniatura RGBA8 de una capa, reducida con filtro de caja.
    bool readLayerThumbnail(int index, int size, std::vector<uint8_t>& out);

    // --- Limites dibujados a mano ---------------------------------------
    // El usuario traza una "pared" en el atlas; el etiquetado por regiones
    // convierte esas paredes en zonas estancas que el pincel no cruza.
    Texture2D& boundaryMask() { return boundaryMask_; }
    const Texture2D& regionMap() const { return regionMap_; }
    bool hasRegions() const { return hasRegions_; }
    /// True en cuanto se traza la primera pared. Evita pagar la lectura de la
    /// mascara entera (y guardarla) en el caso normal, que es no tener ninguna.
    bool hasBoundaryContent() const { return boundaryDrawn_; }
    void markRegionsDirty() { regionsDirty_ = true; }
    void clearBoundary();
    /// Reetiqueta las regiones si la pared cambio. Devuelve true si hay mapa.
    bool ensureRegions();

    // --- Instantanea completa -------------------------------------------
    // Las dos requieren contexto GL vivo: una lee de las texturas y la otra
    // las vuelve a crear.
    bool captureState(DocumentState& out);
    bool restoreState(const DocumentState& state);

    // --- Lectura sin esperas --------------------------------------------
    //
    // `glReadPixels` normal para el atlas entero (16 MB a 2K) planta el hilo de
    // render hasta que la GPU termina todo lo que tenia encolado: es un parón
    // seco de decimas de segundo, y con el autoguardado eso caia a media lamina.
    // Con un buffer de empaquetado la lectura se pide y se vuelve enseguida; la
    // GPU la sirve por su cuenta y un frame despues los bytes ya estan ahi sin
    // que nadie haya esperado.
    //
    // Se usan asi: `beginReadback` una vez, `readbackReady` cada frame hasta que
    // diga que si, y entonces `finishReadback`.
    bool beginReadback(const Texture2D& tex);
    bool readbackReady();
    bool finishReadback(std::vector<uint8_t>& out);
    bool readbackPending() const { return packFence_ != nullptr; }

    // Lee un rectangulo de una textura de capa (lo usa el sistema de deshacer).
    bool readRegion(const Texture2D& tex, int x, int y, int w, int h, std::vector<uint8_t>& out);
    void writeRegion(Texture2D& tex, int x, int y, int w, int h, const uint8_t* pixels);

    // Framebuffer de trabajo reutilizable, ya ligado a `tex`.
    void bindTargetTexture(const Texture2D& tex);
    Framebuffer& workFbo() { return workFbo_; }

private:
    bool ensureShaders();
    void ensureScratch();
    /// La pared se guarda a un byte por texel: se lee en RGBA porque es lo
    /// unico que garantiza glReadPixels, y se conserva solo el rojo.
    bool readBoundary(std::vector<uint8_t>& out);
    void uploadBoundary(const std::vector<uint8_t>& mask);

    int resolution_ = 0;
    int nextLayerId_ = 1;
    int activeIndex_ = 0;
    std::vector<std::unique_ptr<Layer>> layers_;

    Texture2D composite_;
    Texture2D scratchA_;
    Texture2D scratchB_;
    Texture2D uvMask_;
    bool compositeDirty_ = true;

    Texture2D thumbTex_;
    int thumbSize_ = 0;

    Texture2D boundaryMask_;
    bool boundaryDrawn_ = false;
    Texture2D regionMap_;
    Texture2D regionSource_;
    bool regionsDirty_ = true;
    bool hasRegions_ = false;
    int regionResolution_ = 0;

    /// Buffer de empaquetado y su valla, para las lecturas sin espera.
    unsigned int packBuffer_ = 0;
    void* packFence_ = nullptr;  // GLsync, sin arrastrar aqui las cabeceras GL
    size_t packBytes_ = 0;

    Framebuffer workFbo_;
    Shader compositeShader_;
    Shader dilateShader_;
    Shader uvMaskShader_;
    Shader copyShader_;
    Shader thumbShader_;
    Shader regionSourceShader_;
    bool shadersReady_ = false;
};

}  // namespace uvp
