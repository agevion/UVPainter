// Motor de pintado por proyeccion.
//
// Un trazo se compone SIEMPRE contra una instantanea tomada al empezar, no de
// forma incremental: asi la opacidad es constante aunque el trazo se cruce
// consigo mismo, y deshacer se reduce a restaurar un rectangulo.
#pragma once

#include <vector>

#include "gl/GLObjects.h"
#include "paint/Types.h"
#include "paint/UndoStack.h"

namespace uvp {

class Camera;
class Document;

class PaintEngine {
public:
    PaintEngine() = default;
    ~PaintEngine();

    bool initialize();
    void shutdown();

    void setDocument(Document* doc);
    void setMesh(const Mesh* mesh) { mesh_ = mesh; }
    void resizeViewport(int width, int height);

    // --- Trazo ----------------------------------------------------------
    void beginStroke(const StrokePoint& point, const BrushSettings& brush, PaintMode mode,
                     const Camera& camera);
    void addPoint(const StrokePoint& point);
    // Punto extrapolado por androidx.input. Se dibuja en una mascara aparte que
    // se borra en cada frame, asi que jamas queda grabado en la capa: se gana
    // latencia percibida sin arriesgar que el trazo acabe donde no debia.
    void addPredictedPoint(const StrokePoint& point);
    void cancelStroke();
    void endStroke();
    bool strokeActive() const { return strokeActive_; }

    /// Donde esta cayendo la pintura de verdad. Con el regulador activo esto va
    /// por detras de la punta del lapiz: es el extremo de la cuerda.
    Vec2 paintPosition() const { return smoothed_; }
    /// Longitud de cuerda del trazo en curso, o 0 si el regulador esta apagado.
    float ropeLength() const {
        return strokeActive_ && brush_.stabilizerRadiusPx > 0.5f ? brush_.stabilizerRadiusPx
                                                                 : 0.0f;
    }

    /// Bote de pintura: rellena por completo la isla UV bajo el punto dado.
    void requestFill(Vec2 screenPoint, const BrushSettings& brush);

    // Procesa lo pendiente del trazo. Se llama una vez por frame, desde el
    // hilo de render, justo antes de dibujar el visor.
    void flush(const Camera& camera);

    // --- Historial ------------------------------------------------------
    bool undo();
    bool redo();
    bool canUndo() const { return undoStack_.canUndo(); }
    bool canRedo() const { return undoStack_.canRedo(); }
    void clearHistory() { undoStack_.clear(); }
    UndoStack& history() { return undoStack_; }

    // Registra un cambio hecho fuera del trazo (rellenar, limpiar...) para que
    // tambien se pueda deshacer.
    void captureFullLayerPatch(int layerIndex, const std::vector<uint8_t>& before,
                               const char* label);

    const BrushSettings& brush() const { return brush_; }

private:
    struct Segment {
        Vec2 a, b;
        float radiusA, radiusB;
        float alphaA, alphaB;
    };

    bool ensureResources();
    void renderDepthPrepass(const Camera& camera);
    /// Lee isla y region bajo el punto, tal como los dejo el prepaso.
    /// Devuelve false si el lapiz no estaba sobre la malla.
    /// Lee isla, region y coordenada del atlas bajo el punto, tal como los dejo
    /// el prepaso. Devuelve false si el lapiz no estaba sobre la malla.
    bool readSurfaceIdsAt(Vec2 screenPoint, int& island, int& region, Vec2& atlasUv);
    std::vector<Segment> buildShapeSegments() const;
    void drawSegments(const Camera& camera, const std::vector<Segment>& segments,
                      Texture2D& target, bool clearFirst, bool applyRestrictions);
    void performFill(const Camera& camera);
    /// Rellena desde `seedUv` hacia fuera hasta que el color deja de parecerse.
    /// Deja el resultado en la mascara del trazo. false si no se pudo leer el
    /// lienzo o si el punto caia fuera de la malla.
    bool fillClosedArea(Vec2 seedUv);
    void beginLayerEdit();
    void compositeStroke();
    bool computeStrokeBounds(int& x, int& y, int& w, int& h);
    void pushStrokePatch();
    float radiusForPoint(const StrokePoint& p) const;
    float alphaForPoint(const StrokePoint& p) const;
    void emitSegment(const StrokePoint& from, const StrokePoint& to);

    Document* doc_ = nullptr;
    const Mesh* mesh_ = nullptr;

    int viewportW_ = 1;
    int viewportH_ = 1;

    // Recursos por documento
    Texture2D strokeMask_;     // R8, resolucion del atlas
    Texture2D predictedMask_;  // R8, se limpia en cada frame
    Texture2D baseSnapshot_;   // RGBA8, capa activa al empezar el trazo
    std::vector<Texture2D> reducePyramid_;
    int docResolution_ = 0;

    // Recursos por viewport. sceneDepth_ guarda distancia LINEAL empaquetada en
    // RGBA8; depthBuffer_ es solo el z-buffer que ordena el prepaso.
    Texture2D sceneDepth_;
    Texture2D sceneIsland_;
    /// UV del atlas por pixel de pantalla. De aqui sale el texel donde arranca
    /// el relleno por area cerrada.
    Texture2D sceneAtlasUv_;
    Texture2D depthBuffer_;
    Framebuffer depthFbo_;

    Framebuffer paintFbo_;

    Shader paintShader_;
    Shader strokeCompositeShader_;
    Shader depthShader_;
    Shader reduceShader_;
    Shader copyShader_;
    bool initialized_ = false;

    // Estado del trazo en curso
    bool strokeActive_ = false;
    BrushSettings brush_;
    PaintMode mode_ = PaintMode::Paint;
    /// Factor que compensa el zoom cuando el pincel tiene tamano fijo. Se fija
    /// una vez al empezar el trazo (la camara no se mueve mientras se pinta,
    /// ya que gestos y trazo son mutuamente excluyentes) y es 1 si el ajuste
    /// esta desactivado.
    float sizeScale_ = 1.0f;
    int strokeLayerIndex_ = 0;
    int strokeLayerId_ = 0;
    StrokePoint lastRaw_;
    StrokePoint lastEmitted_;
    Vec2 smoothed_{0.0f, 0.0f};
    Vec2 ropeAnchor_{0.0f, 0.0f};
    bool hasEmitted_ = false;
    std::vector<Segment> pendingSegments_;
    std::vector<Segment> predictedSegments_;
    StrokePoint predictedTail_;
    bool hasPredicted_ = false;
    bool predictedDrawnLastFrame_ = false;
    bool strokeDirty_ = false;
    bool pendingEnd_ = false;

    // Isla y region activas. -1 significa "sin restriccion".
    int activeIslandId_ = -1;
    int activeRegionId_ = -1;
    bool pendingIslandPick_ = false;
    Vec2 islandPickPoint_{0.0f, 0.0f};

    // Herramienta de formas: el trazo describe la figura entre estos dos puntos.
    bool shapeActive_ = false;
    Vec2 shapeStart_{0.0f, 0.0f};
    Vec2 shapeEnd_{0.0f, 0.0f};

    bool fillRequested_ = false;
    Vec2 fillPoint_{0.0f, 0.0f};

    UndoStack undoStack_;
};

}  // namespace uvp
