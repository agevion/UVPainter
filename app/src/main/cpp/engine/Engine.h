// Fachada del motor. Todo lo que toca GL vive en el hilo de render; lo unico
// que entra desde el hilo de UI son comandos de entrada, que van por una cola
// con mutex para no pagar el salto de un Handler en el camino del lapiz.
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "gl/GLObjects.h"
#include "gl/RenderContext.h"
#include "io/ModelLoader.h"
#include "paint/Document.h"
#include "paint/PaintEngine.h"
#include "paint/Types.h"
#include "render/ViewportRenderer.h"
#include "scene/Camera.h"

struct ANativeWindow;

namespace uvp {

struct InputCommand {
    enum class Kind : int {
        StrokeBegin,
        StrokeMove,
        StrokePredict,
        StrokeEnd,
        Fill,
        StrokeCancel,
        Orbit,
        Pan,
        Zoom,
        Roll,
        Hover,
        HoverEnd,
    };

    Kind kind = Kind::Hover;
    float x = 0.0f;
    float y = 0.0f;
    float pressure = 1.0f;
    float tilt = 0.0f;
    float orientation = 0.0f;
    double timeMs = 0.0;
};

struct MeshStats {
    int vertexCount = 0;
    int triangleCount = 0;
    int submeshCount = 0;
    int islandCount = 0;
    bool hasUVs = false;
    bool uvsOutside01 = false;
};

class Engine {
public:
    Engine() = default;
    ~Engine();

    // --- Ciclo de vida de la superficie (hilo de render) ------------------
    bool onSurfaceCreated(ANativeWindow* window);
    void onSurfaceChanged(int width, int height);
    void onSurfaceDestroyed();
    bool hasSurface() const { return context_.valid(); }

    // Dibuja un frame completo. Devuelve false si no habia contexto.
    bool drawFrame();

    // --- Contenido (hilo de render) ---------------------------------------
    bool loadModel(const uint8_t* data, size_t size, const std::string& ext,
                   const std::string& name, std::string& error);
    bool createDocument(int resolution);

    // --- Proyecto (hilo de render) ----------------------------------------
    // El archivo lleva el modelo, las capas con sus pixeles y la pared dibujada
    // a mano, para poder retomar el trabajo tal cual se dejo.
    bool saveProject(const std::string& path, std::string& error);
    bool loadProject(const std::string& path, std::string& error);
    const std::string& modelName() const { return modelName_; }

    // Los modelos llegan con ejes distintos segun de donde salgan (Max y Blender
    // son Z arriba, Unity y glTF son Y arriba), asi que hace falta poder
    // recolocarlos sin salir de la app.
    void setOrientation(int upAxis, bool flipUp, int quarterTurns);
    int upAxis() const { return upAxis_; }
    bool flipUp() const { return flipUp_; }
    int quarterTurns() const { return quarterTurns_; }
    bool hasModel() const { return mesh_.valid(); }
    const MeshStats& meshStats() const { return meshStats_; }

    Document& document() { return document_; }
    PaintEngine& paint() { return paintEngine_; }
    Camera& camera() { return camera_; }
    ViewportSettings& viewportSettings() { return viewport_; }

    void setBrush(const BrushSettings& brush) { brush_ = brush; }
    const BrushSettings& brush() const { return brush_; }
    void setPaintMode(PaintMode mode) { paintMode_ = mode; }
    PaintMode paintMode() const { return paintMode_; }

    void resetView();
    // Cuentagotas: lee el pixel ya compuesto del visor. Devuelve 0xAARRGGBB.
    uint32_t pickScreenColor(int x, int y);
    bool exportComposite(std::vector<uint8_t>& rgba, int& outSize, bool dilateForExport);
    bool exportLayer(int index, std::vector<uint8_t>& rgba, int& outSize);
    bool importLayerPixels(int index, const uint8_t* rgba, int size);

    // --- Entrada (hilo de UI, seguro para concurrencia) -------------------
    void pushCommand(const InputCommand& cmd);
    void setStylusOnly(bool value) { stylusOnly_.store(value); }

    int viewportWidth() const { return viewportW_; }
    int viewportHeight() const { return viewportH_; }

private:
    void drainCommands();
    void handleCommand(const InputCommand& cmd);
    /// `frameCamera` en false conserva el encuadre: al recuperar el contexto la
    /// malla se vuelve a subir, pero mover la camara ahi seria desconcertante.
    void rebuildOrientedMesh(bool frameCamera = true);
    void adoptDocumentState(const DocumentState& state);

    RenderContext context_;
    ANativeWindow* window_ = nullptr;
    Camera camera_;
    Mesh mesh_;
    MeshData sourceMesh_;  // sin orientar: permite recolocar sin recargar
    // El archivo original se guarda tal cual: es lo que permite reabrir el
    // proyecto sin pedirle al usuario que vuelva a buscar el modelo.
    std::vector<uint8_t> modelBytes_;
    std::string modelExt_;
    std::string modelName_;
    MeshStats meshStats_;
    Aabb modelBounds_;
    int upAxis_ = 0;        // 0 = Y arriba (glTF), 1 = Z arriba
    bool flipUp_ = false;   // invierte el sentido del eje vertical
    int quarterTurns_ = 0;  // giros de 90 grados alrededor del eje vertical
    Document document_;
    PaintEngine paintEngine_;
    ViewportRenderer renderer_;
    ViewportSettings viewport_;

    BrushSettings brush_;
    PaintMode paintMode_ = PaintMode::Paint;

    int viewportW_ = 1;
    int viewportH_ = 1;
    bool resourcesReady_ = false;

    /// Documento a la espera de volver a GPU tras perder el contexto EGL.
    DocumentState pendingState_;

    Vec2 hoverPos_{0.0f, 0.0f};
    bool hoverVisible_ = false;
    float hoverPressure_ = 0.0f;

    std::mutex commandMutex_;
    std::vector<InputCommand> commandQueue_;
    std::vector<InputCommand> commandScratch_;
    std::atomic<bool> stylusOnly_{true};
};

}  // namespace uvp
