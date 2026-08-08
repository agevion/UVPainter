#include "paint/PaintEngine.h"

#include <algorithm>
#include <cmath>

#include "core/Log.h"
#include "paint/Document.h"
#include "render/Shaders.h"
#include "scene/Camera.h"

namespace uvp {

namespace {
constexpr int kMaxSegmentsPerDraw = 48;
constexpr int kReduceTarget = 64;  // lado de la textura donde acaba la reduccion
}  // namespace

PaintEngine::~PaintEngine() { shutdown(); }

bool PaintEngine::initialize() {
    if (initialized_) return true;

    bool ok = true;
    ok &= paintShader_.compile(shaders::kPaintVS, shaders::kPaintFS, "paint");
    ok &= strokeCompositeShader_.compile(shaders::kFullscreenVS, shaders::kStrokeCompositeFS,
                                         "strokeComposite");
    ok &= depthShader_.compile(shaders::kDepthVS, shaders::kDepthFS, "depthPrepass");
    ok &= reduceShader_.compile(shaders::kFullscreenVS, shaders::kMaxReduceFS, "maxReduce");
    ok &= copyShader_.compile(shaders::kFullscreenVS, shaders::kCopyFS, "paintCopy");
    if (!ok) {
        LOGE("PaintEngine: fallo compilando shaders");
        return false;
    }

    depthFbo_.create();
    paintFbo_.create();
    initialized_ = true;
    return true;
}

void PaintEngine::shutdown() {
    strokeActive_ = false;
    strokeMask_.destroy();
    predictedMask_.destroy();
    baseSnapshot_.destroy();
    reducePyramid_.clear();
    sceneDepth_.destroy();
    // Estas dos faltaban: al perder el contexto hay que soltar todo lo que vive
    // en GPU mientras el contexto sigue vivo, no dejarlo al destructor.
    sceneIsland_.destroy();
    sceneAtlasUv_.destroy();
    depthBuffer_.destroy();
    depthFbo_.destroy();
    paintFbo_.destroy();
    paintShader_.destroy();
    strokeCompositeShader_.destroy();
    depthShader_.destroy();
    reduceShader_.destroy();
    copyShader_.destroy();
    undoStack_.clear();
    docResolution_ = 0;
    initialized_ = false;
}

void PaintEngine::setDocument(Document* doc) {
    doc_ = doc;
    strokeActive_ = false;
    undoStack_.clear();
    if (doc_ != nullptr && doc_->valid()) ensureResources();
}

bool PaintEngine::ensureResources() {
    if (doc_ == nullptr || !doc_->valid()) return false;
    const int res = doc_->resolution();
    if (res == docResolution_ && strokeMask_.valid()) return true;

    docResolution_ = res;
    strokeMask_.create(res, res, GL_R8, GL_LINEAR);
    predictedMask_.create(res, res, GL_R8, GL_LINEAR);
    baseSnapshot_.create(res, res, GL_RGBA8, GL_LINEAR);

    // Piramide de reduccion por maximos hasta 64x64: sirve para saber que
    // rectangulo toco el trazo sin leer de vuelta el atlas entero.
    reducePyramid_.clear();
    for (int size = res / 2; size >= kReduceTarget; size /= 2) {
        Texture2D tex;
        tex.create(size, size, GL_R8, GL_NEAREST);
        reducePyramid_.push_back(std::move(tex));
        if (size == kReduceTarget) break;
    }

    // El presupuesto de historial escala con el tamano del documento.
    const size_t budget = static_cast<size_t>(res) * static_cast<size_t>(res) * 4u * 24u;
    undoStack_.setMemoryBudget(std::min<size_t>(budget, 512u * 1024u * 1024u));

    LOGI("PaintEngine listo para atlas %dx%d (%zu niveles de reduccion)", res, res,
         reducePyramid_.size());
    return true;
}

void PaintEngine::resizeViewport(int width, int height) {
    const int w = std::max(width, 1);
    const int h = std::max(height, 1);
    if (w == viewportW_ && h == viewportH_ && sceneDepth_.valid()) return;

    viewportW_ = w;
    viewportH_ = h;
    sceneDepth_.create(viewportW_, viewportH_, GL_RGBA8, GL_NEAREST);
    sceneIsland_.create(viewportW_, viewportH_, GL_RGBA8, GL_NEAREST);
    sceneAtlasUv_.create(viewportW_, viewportH_, GL_RGBA8, GL_NEAREST);
    depthBuffer_.create(viewportW_, viewportH_, GL_DEPTH_COMPONENT24, GL_NEAREST);

    depthFbo_.bind();
    depthFbo_.attachColor(sceneDepth_, 0);
    depthFbo_.attachColor(sceneIsland_, 1);
    depthFbo_.attachColor(sceneAtlasUv_, 2);
    depthFbo_.attachDepth(depthBuffer_);
    const GLenum buffers[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                               GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, buffers);
    if (!depthFbo_.isComplete()) LOGE("FBO del prepaso de profundidad incompleto");
    Framebuffer::unbind();
}

// ---------------------------------------------------------------------------
// Respuesta del lapiz
// ---------------------------------------------------------------------------
namespace {
/// Presion util: se aplica primero la ganancia (el digitalizador rara vez
/// entrega 1.0 en un trazo normal) y despues la curva de respuesta.
float shapePressure(float raw, float gain, float curve) {
    const float boosted = clampf(raw * std::max(gain, 0.05f), 0.0f, 1.0f);
    return std::pow(boosted, std::max(curve, 0.05f));
}
}  // namespace

float PaintEngine::radiusForPoint(const StrokePoint& p) const {
    float scale = 1.0f;
    if (brush_.pressureAffectsSize) {
        const float curved = shapePressure(p.pressure, brush_.pressureGain, brush_.pressureCurve);
        scale *= lerpf(brush_.pressureSizeFloor, 1.0f, curved);
    }
    if (brush_.tiltAffectsSize > 0.0f) {
        // Mas inclinado = trazo mas ancho, como un lapiz de verdad.
        const float tiltNorm = clampf(p.tilt / (kPi * 0.5f), 0.0f, 1.0f);
        scale *= lerpf(1.0f, 1.0f + brush_.tiltAffectsSize * 1.5f, tiltNorm);
    }
    return std::max(brush_.radiusPx * scale * sizeScale_, 0.5f);
}

float PaintEngine::alphaForPoint(const StrokePoint& p) const {
    float a = brush_.flow;
    if (brush_.pressureAffectsOpacity) {
        const float curved = shapePressure(p.pressure, brush_.pressureGain, brush_.pressureCurve);
        a *= lerpf(brush_.pressureOpacityFloor, 1.0f, curved);
    }
    return clampf(a, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// Trazo
// ---------------------------------------------------------------------------
void PaintEngine::beginStroke(const StrokePoint& point, const BrushSettings& brush,
                              PaintMode mode, const Camera& camera) {
    if (doc_ == nullptr || !doc_->valid() || !ensureResources()) return;

    Layer* layer = doc_->activeLayer();
    if (layer == nullptr || layer->info.locked) return;

    brush_ = brush;
    mode_ = mode;
    // Cuanto mas cerca esta la camara del encuadre inicial, menos hace falta
    // agrandar el pincel en pantalla para que ocupe el mismo trozo de
    // superficie; cuanto mas se acerca la camara, mas hay que agrandarlo.
    sizeScale_ = brush_.lockSizeToSurface
                     ? camera.referenceDistance() / std::max(camera.distance(), 1e-4f)
                     : 1.0f;
    strokeLayerIndex_ = doc_->activeIndex();
    strokeLayerId_ = layer->id;

    // Dibujar un limite no toca la capa: va a su propia mascara.
    if (mode_ != PaintMode::Boundary) beginLayerEdit();

    // Isla y region se resuelven en el primer flush, cuando ya hay prepaso.
    activeIslandId_ = -1;
    activeRegionId_ = -1;
    pendingIslandPick_ = mode_ != PaintMode::Boundary &&
                         (brush_.restrictToIsland || brush_.restrictToRegion);
    islandPickPoint_ = point.screen;

    shapeActive_ = brush_.shape != ShapeKind::None && mode_ != PaintMode::Boundary;
    shapeStart_ = point.screen;
    shapeEnd_ = point.screen;

    lastRaw_ = point;
    lastEmitted_ = point;
    smoothed_ = point.screen;
    ropeAnchor_ = point.screen;
    hasEmitted_ = false;
    pendingSegments_.clear();
    predictedSegments_.clear();
    hasPredicted_ = false;
    predictedDrawnLastFrame_ = false;
    strokeActive_ = true;
    strokeDirty_ = true;

    LOGD("beginStroke (%.1f, %.1f) capa=%d doc=%d vp=%dx%d", point.screen.x, point.screen.y,
         strokeLayerIndex_, docResolution_, viewportW_, viewportH_);

    // Un toque sin arrastrar tambien debe dejar marca: emitimos un segmento
    // degenerado en el punto inicial.
    emitSegment(point, point);
}

/// Prepara la capa activa para una edicion: instantanea previa y mascaras a
/// cero. Lo comparten el trazo normal y el bote de pintura.
void PaintEngine::beginLayerEdit() {
    Layer* layer = doc_->layerAt(strokeLayerIndex_);
    if (layer == nullptr) return;

    doc_->bindTargetTexture(baseSnapshot_);
    glViewport(0, 0, docResolution_, docResolution_);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    copyShader_.bind();
    layer->texture.bind(0);
    copyShader_.set("uTex", 0);
    drawFullscreenTriangle();

    doc_->bindTargetTexture(strokeMask_);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    doc_->bindTargetTexture(predictedMask_);
    glClear(GL_COLOR_BUFFER_BIT);
    Framebuffer::unbind();
}

void PaintEngine::emitSegment(const StrokePoint& from, const StrokePoint& to) {
    Segment seg;
    seg.a = from.screen;
    seg.b = to.screen;
    seg.radiusA = radiusForPoint(from);
    seg.radiusB = radiusForPoint(to);
    seg.alphaA = alphaForPoint(from);
    seg.alphaB = alphaForPoint(to);
    pendingSegments_.push_back(seg);
    strokeDirty_ = true;
}

void PaintEngine::addPredictedPoint(const StrokePoint& point) {
    if (!strokeActive_) return;

    const StrokePoint& from = hasPredicted_ ? predictedTail_ : lastEmitted_;
    if (length(point.screen - from.screen) < 0.5f) return;

    Segment seg;
    seg.a = from.screen;
    seg.b = point.screen;
    seg.radiusA = radiusForPoint(from);
    seg.radiusB = radiusForPoint(point);
    seg.alphaA = alphaForPoint(from);
    seg.alphaB = alphaForPoint(point);
    predictedSegments_.push_back(seg);

    predictedTail_ = point;
    hasPredicted_ = true;
    strokeDirty_ = true;
}

void PaintEngine::addPoint(const StrokePoint& point) {
    if (!strokeActive_) return;

    if (shapeActive_) {
        // La figura se redibuja entera cada frame entre el punto inicial y el
        // actual, asi que se ve la vista previa mientras arrastras.
        shapeEnd_ = point.screen;
        lastRaw_ = point;
        strokeDirty_ = true;
        return;
    }

    // Dos etapas encadenadas. Primero el regularizador de cuerda, que es lo que
    // permite trazar lineas largas y limpias: el pincel solo se mueve cuando el
    // lapiz tira de una cuerda de longitud fija, asi que todo el temblor por
    // debajo de esa longitud se descarta entero.
    Vec2 target = point.screen;
    if (brush_.stabilizerRadiusPx > 0.5f) {
        const Vec2 delta = target - ropeAnchor_;
        const float dist = length(delta);
        if (dist > brush_.stabilizerRadiusPx) {
            ropeAnchor_ = target - delta * (brush_.stabilizerRadiusPx / dist);
        }
        target = ropeAnchor_;
    }

    // Y despues el suavizado exponencial, que redondea lo que quede.
    const float follow = 1.0f - clampf(brush_.smoothing, 0.0f, 0.95f);
    smoothed_ = smoothed_ + (target - smoothed_) * follow;

    StrokePoint emitted = point;
    emitted.screen = smoothed_;

    const float moved = length(emitted.screen - lastEmitted_.screen);
    const float minStep = std::max(brush_.spacingPx, 0.5f);
    if (hasEmitted_ && moved < minStep) {
        lastRaw_ = point;
        return;
    }

    emitSegment(lastEmitted_, emitted);
    lastEmitted_ = emitted;
    lastRaw_ = point;
    hasEmitted_ = true;
}

void PaintEngine::cancelStroke() {
    if (!strokeActive_ || doc_ == nullptr) return;

    // Palma detectada o gesto cancelado: devolvemos la capa a como estaba.
    Layer* layer = doc_->layerAt(strokeLayerIndex_);
    if (layer != nullptr && layer->id == strokeLayerId_) {
        doc_->bindTargetTexture(layer->texture);
        glViewport(0, 0, docResolution_, docResolution_);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        copyShader_.bind();
        baseSnapshot_.bind(0);
        copyShader_.set("uTex", 0);
        drawFullscreenTriangle();
        Framebuffer::unbind();
        doc_->markDirty();
    }

    strokeActive_ = false;
    pendingEnd_ = false;
    pendingSegments_.clear();
    predictedSegments_.clear();
    hasPredicted_ = false;
    predictedDrawnLastFrame_ = false;
    strokeDirty_ = false;
}

void PaintEngine::endStroke() {
    if (!strokeActive_) return;

    // El regularizador deja el pincel por detras del lapiz. Al levantar hay que
    // cerrar ese hueco, o el trazo se queda corto respecto a donde soltaste.
    if (!shapeActive_ && hasEmitted_ &&
        length(lastRaw_.screen - lastEmitted_.screen) > 0.75f) {
        StrokePoint tail = lastRaw_;
        emitSegment(lastEmitted_, tail);
        lastEmitted_ = tail;
    }
    // El cierre se completa en el siguiente flush: puede quedar algun segmento
    // sin dibujar si el dedo se levanto entre dos frames, y el parche del
    // historial tiene que capturar el trazo ya compuesto.
    strokeActive_ = false;
    pendingEnd_ = true;
    strokeDirty_ = true;
}

void PaintEngine::flush(const Camera& camera) {
    if (doc_ == nullptr || !doc_->valid() || mesh_ == nullptr || !mesh_->valid()) return;

    if (fillRequested_) {
        performFill(camera);
        fillRequested_ = false;
        return;
    }

    if (!strokeDirty_ && !pendingEnd_) return;
    if (!strokeActive_ && !pendingEnd_) return;

    // Etiquetar regiones antes del prepaso: el prepaso las escribe en su
    // segundo adjunto y de ahi sale la region bajo el lapiz.
    if (mode_ != PaintMode::Boundary && brush_.restrictToRegion) doc_->ensureRegions();

    const bool needsPrepass = !pendingSegments_.empty() || !predictedSegments_.empty() ||
                              pendingIslandPick_ || shapeActive_;
    if (needsPrepass) renderDepthPrepass(camera);

    if (pendingIslandPick_) {
        Vec2 ignoredUv;
        readSurfaceIdsAt(islandPickPoint_, activeIslandId_, activeRegionId_, ignoredUv);
        pendingIslandPick_ = false;
    }

    // Dibujar un limite va directo a su mascara, sin tocar la capa ni el
    // historial de pixeles, y sin restricciones (la pared puede cruzar donde
    // quiera).
    if (mode_ == PaintMode::Boundary) {
        if (!pendingSegments_.empty()) {
            drawSegments(camera, pendingSegments_, doc_->boundaryMask(), false, false);
            pendingSegments_.clear();
            doc_->markRegionsDirty();
        }
        strokeDirty_ = false;
        if (pendingEnd_) pendingEnd_ = false;
        return;
    }

    if (shapeActive_) {
        // Se limpia y se redibuja la figura completa: la vista previa siempre
        // refleja exactamente donde esta el lapiz ahora.
        const std::vector<Segment> shape = buildShapeSegments();
        drawSegments(camera, shape, strokeMask_, true, true);
        pendingSegments_.clear();
    } else if (!pendingSegments_.empty()) {
        drawSegments(camera, pendingSegments_, strokeMask_, false, true);
        pendingSegments_.clear();
    }

    // La prediccion se rehace entera cada frame: nunca se acumula.
    if (!shapeActive_ && !predictedSegments_.empty()) {
        drawSegments(camera, predictedSegments_, predictedMask_, true, true);
        predictedSegments_.clear();
        predictedDrawnLastFrame_ = true;
    } else if (predictedDrawnLastFrame_) {
        doc_->bindTargetTexture(predictedMask_);
        glViewport(0, 0, docResolution_, docResolution_);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Framebuffer::unbind();
        predictedDrawnLastFrame_ = false;
    }
    hasPredicted_ = false;

    compositeStroke();
    doc_->markDirty();
    strokeDirty_ = false;

    if (pendingEnd_) {
        pushStrokePatch();
        pendingEnd_ = false;
    }
}

void PaintEngine::renderDepthPrepass(const Camera& camera) {
    if (!sceneDepth_.valid() || mesh_ == nullptr || !mesh_->valid()) return;

    depthFbo_.bind();
    glViewport(0, 0, viewportW_, viewportH_);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);

    // Cada adjunto se limpia a un valor distinto: blanco en profundidad
    // significa "plano lejano", y cero en islas significa "sin geometria".
    const float farColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const float noIsland[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float noUv[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, farColor);
    glClearBufferfv(GL_COLOR, 1, noIsland);
    glClearBufferfv(GL_COLOR, 2, noUv);
    glClearDepthf(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    depthShader_.bind();
    depthShader_.set("uViewProj", camera.viewProjection());
    depthShader_.set("uView", camera.view());
    depthShader_.set("uFar", camera.farPlane());
    const bool hasRegions = doc_ != nullptr && doc_->hasRegions();
    depthShader_.set("uHasRegions", hasRegions ? 1 : 0);
    if (hasRegions) {
        doc_->regionMap().bind(0);
        depthShader_.set("uRegionMap", 0);
    }
    mesh_->draw();

    Framebuffer::unbind();
    GL_CHECK("PaintEngine::renderDepthPrepass");
}

bool PaintEngine::readSurfaceIdsAt(Vec2 screenPoint, int& island, int& region, Vec2& atlasUv) {
    island = -1;
    region = -1;
    atlasUv = {0.0f, 0.0f};
    if (!sceneIsland_.valid()) return false;

    const int x = static_cast<int>(clampf(screenPoint.x, 0.0f,
                                          static_cast<float>(viewportW_ - 1)));
    // El origen de glReadPixels esta abajo; el del MotionEvent, arriba.
    const int y = viewportH_ - 1 -
                  static_cast<int>(clampf(screenPoint.y, 0.0f,
                                          static_cast<float>(viewportH_ - 1)));

    depthFbo_.bind();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    uint8_t px[4] = {0, 0, 0, 0};
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);

    // Segunda lectura, de un pixel tambien: la coordenada del atlas bajo la
    // punta. Sale gratis del mismo prepaso y es donde arranca el relleno.
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    uint8_t uv[4] = {0, 0, 0, 0};
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, uv);

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    Framebuffer::unbind();

    // 16 bits por coordenada, tal y como los empaqueto kDepthFS.
    atlasUv = {
        (static_cast<float>(uv[0]) + static_cast<float>(uv[1]) / 255.0f) / 255.0f,
        (static_cast<float>(uv[2]) + static_cast<float>(uv[3]) / 255.0f) / 255.0f,
    };

    const int islandCode = static_cast<int>(px[0]) + static_cast<int>(px[1]) * 256;
    const int regionCode = static_cast<int>(px[2]) + static_cast<int>(px[3]) * 256;
    // 0 = el lapiz no estaba sobre la malla / no hay regiones definidas.
    if (islandCode == 0) return false;
    island = islandCode - 1;
    region = regionCode > 0 ? regionCode : -1;
    return true;
}

/// Convierte la figura activa en la lista de segmentos que la dibuja.
std::vector<PaintEngine::Segment> PaintEngine::buildShapeSegments() const {
    std::vector<Vec2> points;
    const Vec2 a = shapeStart_;
    const Vec2 b = shapeEnd_;

    switch (brush_.shape) {
        case ShapeKind::Line:
            points = {a, b};
            break;
        case ShapeKind::Rectangle: {
            const Vec2 min = brush_.shapeFromCenter ? Vec2(a.x - (b.x - a.x), a.y - (b.y - a.y))
                                                    : a;
            points = {min, Vec2(b.x, min.y), b, Vec2(min.x, b.y), min};
            break;
        }
        case ShapeKind::Ellipse: {
            const Vec2 center = brush_.shapeFromCenter ? a : Vec2((a.x + b.x) * 0.5f,
                                                                  (a.y + b.y) * 0.5f);
            const float rx = brush_.shapeFromCenter ? std::fabs(b.x - a.x)
                                                    : std::fabs(b.x - a.x) * 0.5f;
            const float ry = brush_.shapeFromCenter ? std::fabs(b.y - a.y)
                                                    : std::fabs(b.y - a.y) * 0.5f;
            // Mas segmentos cuanto mayor es la figura: una elipse grande con
            // pocos lados se ve poligonal.
            const int steps = std::clamp(static_cast<int>(std::max(rx, ry) * 0.5f), 24, 180);
            points.reserve(static_cast<size_t>(steps) + 1u);
            for (int i = 0; i <= steps; ++i) {
                const float t = (static_cast<float>(i) / static_cast<float>(steps)) * 2.0f * kPi;
                points.push_back({center.x + std::cos(t) * rx, center.y + std::sin(t) * ry});
            }
            break;
        }
        case ShapeKind::Polygon: {
            const int sides = std::clamp(brush_.polygonSides, 3, 24);
            const Vec2 delta = b - a;
            const float radius = length(delta);
            const float phase = std::atan2(delta.y, delta.x);
            points.reserve(static_cast<size_t>(sides) + 1u);
            for (int i = 0; i <= sides; ++i) {
                const float t = phase + (static_cast<float>(i) / static_cast<float>(sides)) *
                                            2.0f * kPi;
                points.push_back({a.x + std::cos(t) * radius, a.y + std::sin(t) * radius});
            }
            break;
        }
        case ShapeKind::None:
            break;
    }

    // Las figuras van a grosor y opacidad constantes: una linea recta que
    // adelgaza donde apretaste menos no es una linea recta.
    std::vector<PaintEngine::Segment> segments;
    if (points.size() < 2) return segments;
    segments.reserve(points.size() - 1);
    const float radius = std::max(brush_.radiusPx * sizeScale_, 0.5f);
    const float alpha = clampf(brush_.flow, 0.0f, 1.0f);
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        segments.push_back({points[i], points[i + 1], radius, radius, alpha, alpha});
    }
    return segments;
}

void PaintEngine::drawSegments(const Camera& camera, const std::vector<Segment>& segments,
                               Texture2D& target, bool clearFirst, bool applyRestrictions) {
    if (segments.empty() && !clearFirst) return;

    paintFbo_.bind();
    paintFbo_.attachColor(target, 0);
    const GLenum buffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, buffers);

    glViewport(0, 0, docResolution_, docResolution_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);

    if (clearFirst) {
        glDisable(GL_BLEND);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // GL_MAX acumula la cobertura del trazo sin sumarla: cruzar el trazo
    // consigo mismo no oscurece de mas.
    glEnable(GL_BLEND);
    glBlendEquation(GL_MAX);
    glBlendFunc(GL_ONE, GL_ONE);

    paintShader_.bind();
    paintShader_.set("uViewProj", camera.viewProjection());
    paintShader_.set("uView", camera.view());
    paintShader_.set("uCameraPos", camera.position());
    paintShader_.set("uViewportSize", Vec2(static_cast<float>(viewportW_),
                                           static_cast<float>(viewportH_)));
    paintShader_.set("uFar", camera.farPlane());
    paintShader_.set("uOrthographic", camera.isOrthographic() ? 1 : 0);
    paintShader_.set("uUvOffset", Vec2(0.0f, 0.0f));
    paintShader_.set("uHardness", clampf(brush_.hardness, 0.0f, 1.0f));
    paintShader_.set("uFillMode", 0);

    paintShader_.set("uTipShape", static_cast<int>(brush_.tipShape));
    paintShader_.set("uTipAspect", brush_.tipAspect);
    paintShader_.set("uTipAngle", brush_.tipAngle);
    paintShader_.set("uTipFollowsStroke", brush_.tipFollowsStroke ? 1 : 0);
    paintShader_.set("uGrainAmount", brush_.grainAmount);
    paintShader_.set("uGrainScale", std::max(brush_.grainScale, 1.0f));

    // Solo se restringe si el trazo empezo realmente sobre la malla; si arranco
    // en el vacio, restringir dejaria el pincel muerto sin explicacion.
    const bool restrictIsland =
        applyRestrictions && brush_.restrictToIsland && activeIslandId_ >= 0;
    paintShader_.set("uRestrictIsland", restrictIsland ? 1 : 0);
    paintShader_.set("uIslandId", static_cast<float>(activeIslandId_));

    const bool restrictRegion = applyRestrictions && brush_.restrictToRegion &&
                                activeRegionId_ > 0 && doc_->hasRegions();
    paintShader_.set("uRestrictRegion", restrictRegion ? 1 : 0);
    paintShader_.set("uRegionId", static_cast<float>(activeRegionId_));
    doc_->regionMap().bind(1);
    paintShader_.set("uRegionMap", 1);

    paintShader_.set("uBackfaceCull", brush_.backfaceCull ? 1 : 0);
    paintShader_.set("uFacingCutoff", brush_.facingCutoff);
    paintShader_.set("uFacingFull", std::max(brush_.facingFull, brush_.facingCutoff + 0.01f));

    const bool useDepth = brush_.depthTest && sceneDepth_.valid();
    paintShader_.set("uUseDepthTest", useDepth ? 1 : 0);
    if (useDepth) {
        sceneDepth_.bind(0);
        paintShader_.set("uSceneDepth", 0);
        // La malla se compara consigo misma: hace falta holgura para el ruido de
        // cuantizacion, pero no tanta como para atravesar geometria fina.
        paintShader_.set("uDepthBias", camera.distance() * 0.004f);
    }

    // Los segmentos van en tandas: el shader tiene un tope de uniformes.
    std::vector<float> segA(static_cast<size_t>(kMaxSegmentsPerDraw) * 4u);
    std::vector<float> segB(static_cast<size_t>(kMaxSegmentsPerDraw) * 4u);

    size_t offset = 0;
    while (offset < segments.size()) {
        const size_t count = std::min<size_t>(kMaxSegmentsPerDraw, segments.size() - offset);
        for (size_t i = 0; i < count; ++i) {
            const Segment& s = segments[offset + i];
            segA[i * 4 + 0] = s.a.x;
            segA[i * 4 + 1] = s.a.y;
            segA[i * 4 + 2] = s.radiusA;
            segA[i * 4 + 3] = s.alphaA;
            segB[i * 4 + 0] = s.b.x;
            segB[i * 4 + 1] = s.b.y;
            segB[i * 4 + 2] = s.radiusB;
            segB[i * 4 + 3] = s.alphaB;
        }
        paintShader_.set("uSegmentCount", static_cast<int>(count));
        paintShader_.setVec4Array("uSegA", segA.data(), kMaxSegmentsPerDraw);
        paintShader_.setVec4Array("uSegB", segB.data(), kMaxSegmentsPerDraw);
        mesh_->draw();
        offset += count;
    }

    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_BLEND);
    Framebuffer::unbind();
    GL_CHECK("PaintEngine::drawSegments");
}

void PaintEngine::requestFill(Vec2 screenPoint, const BrushSettings& brush) {
    if (doc_ == nullptr || !doc_->valid() || !ensureResources()) return;
    brush_ = brush;
    fillPoint_ = screenPoint;
    fillRequested_ = true;
    strokeDirty_ = true;
}

void PaintEngine::performFill(const Camera& camera) {
    Layer* layer = doc_->activeLayer();
    if (layer == nullptr || layer->info.locked) return;

    strokeLayerIndex_ = doc_->activeIndex();
    strokeLayerId_ = layer->id;
    mode_ = PaintMode::Paint;
    beginLayerEdit();

    if (brush_.restrictToRegion) doc_->ensureRegions();
    renderDepthPrepass(camera);
    Vec2 seedUv{0.0f, 0.0f};
    readSurfaceIdsAt(fillPoint_, activeIslandId_, activeRegionId_, seedUv);
    if (activeIslandId_ < 0) {
        // El toque cayo fuera del modelo: no se rellena nada.
        return;
    }

    // Relleno por area cerrada: no se dibuja nada con el shader, la mascara se
    // calcula en CPU recorriendo el atlas desde el texel tocado.
    if (brush_.fillClosedArea) {
        if (!fillClosedArea(seedUv)) {
            LOGW("El relleno por area no pudo arrancar en (%.4f, %.4f)", seedUv.x, seedUv.y);
            return;
        }
        predictedDrawnLastFrame_ = false;
        compositeStroke();
        doc_->dilate(layer->texture, 2);
        doc_->markDirty();
        pushStrokePatch();
        GL_CHECK("PaintEngine::performFill(area)");
        return;
    }

    paintFbo_.bind();
    paintFbo_.attachColor(strokeMask_, 0);
    const GLenum buffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, buffers);
    glViewport(0, 0, docResolution_, docResolution_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    paintShader_.bind();
    paintShader_.set("uFillMode", 1);
    paintShader_.set("uRestrictIsland", 1);
    paintShader_.set("uIslandId", static_cast<float>(activeIslandId_));
    // Si hay limites dibujados a mano, el bote respeta tambien esa frontera.
    const bool restrictRegion =
        brush_.restrictToRegion && activeRegionId_ > 0 && doc_->hasRegions();
    paintShader_.set("uRestrictRegion", restrictRegion ? 1 : 0);
    paintShader_.set("uRegionId", static_cast<float>(activeRegionId_));
    doc_->regionMap().bind(1);
    paintShader_.set("uRegionMap", 1);
    paintShader_.set("uUvOffset", Vec2(0.0f, 0.0f));
    paintShader_.set("uSegmentCount", 0);
    mesh_->draw();
    Framebuffer::unbind();

    predictedDrawnLastFrame_ = false;
    compositeStroke();
    // Un par de pasadas de dilatado para que el relleno no deje un halo del
    // color anterior justo en el borde de la isla.
    doc_->dilate(layer->texture, 2);
    doc_->markDirty();
    pushStrokePatch();
    GL_CHECK("PaintEngine::performFill");
}

/// Relleno por area cerrada, al modo de un editor de fotos.
///
/// Se extiende desde el texel tocado y se para donde el color deja de
/// parecerse, sin mirar islas ni paredes dibujadas a mano: eso es justo lo que
/// hace falta para rellenar una figura trazada a pulso, que no tiene mas
/// frontera que su propio contorno.
///
/// Se compara contra la COMPOSICION y no contra la capa activa, para que un
/// contorno dibujado en otra capa siga frenando el relleno: si se ve, corta.
bool PaintEngine::fillClosedArea(Vec2 seedUv) {
    const int res = docResolution_;
    if (res <= 0) return false;

    std::vector<uint8_t> canvas;
    if (!doc_->readComposite(canvas, false, 0)) return false;

    // La mascara de UV evita que el relleno se escape por el hueco vacio del
    // atlas y aparezca en una isla que solo esta al lado por casualidad.
    std::vector<uint8_t> uvMask;
    const bool hasMask = doc_->readRegion(doc_->uvMask(), 0, 0, res, res, uvMask);

    const size_t texels = static_cast<size_t>(res) * static_cast<size_t>(res);
    if (canvas.size() < texels * 4u) return false;

    const int sx = static_cast<int>(clampf(seedUv.x * static_cast<float>(res), 0.0f,
                                           static_cast<float>(res - 1)));
    const int sy = static_cast<int>(clampf(seedUv.y * static_cast<float>(res), 0.0f,
                                           static_cast<float>(res - 1)));
    const size_t seed = static_cast<size_t>(sy) * static_cast<size_t>(res) + sx;

    const int r0 = canvas[seed * 4u + 0];
    const int g0 = canvas[seed * 4u + 1];
    const int b0 = canvas[seed * 4u + 2];
    const int a0 = canvas[seed * 4u + 3];
    const int tol = static_cast<int>(clampf(brush_.fillTolerance, 0.0f, 1.0f) * 255.0f);

    std::vector<uint8_t> out(texels, 0u);

    auto matches = [&](size_t i) {
        if (hasMask && uvMask[i * 4u] < 128u) return false;
        const uint8_t* c = &canvas[i * 4u];
        return std::abs(static_cast<int>(c[0]) - r0) <= tol &&
               std::abs(static_cast<int>(c[1]) - g0) <= tol &&
               std::abs(static_cast<int>(c[2]) - b0) <= tol &&
               std::abs(static_cast<int>(c[3]) - a0) <= tol;
    };

    if (!matches(seed)) return false;

    // Pila explicita: una version recursiva desborda la pila del hilo en cuanto
    // el area pasa de unos miles de texels, y aqui pueden ser millones.
    std::vector<uint32_t> pending;
    pending.reserve(4096);
    out[seed] = 255u;
    pending.push_back(static_cast<uint32_t>(seed));

    while (!pending.empty()) {
        const uint32_t index = pending.back();
        pending.pop_back();
        const int x = static_cast<int>(index % static_cast<uint32_t>(res));
        const int y = static_cast<int>(index / static_cast<uint32_t>(res));

        const int nx[4] = {x - 1, x + 1, x, x};
        const int ny[4] = {y, y, y - 1, y + 1};
        for (int k = 0; k < 4; ++k) {
            if (nx[k] < 0 || nx[k] >= res || ny[k] < 0 || ny[k] >= res) continue;
            const size_t n = static_cast<size_t>(ny[k]) * static_cast<size_t>(res) + nx[k];
            if (out[n] != 0u || !matches(n)) continue;
            out[n] = 255u;
            pending.push_back(static_cast<uint32_t>(n));
        }
    }

    strokeMask_.upload(out.data(), GL_RED);
    return true;
}

void PaintEngine::compositeStroke() {
    Layer* layer = doc_->layerAt(strokeLayerIndex_);
    if (layer == nullptr || layer->id != strokeLayerId_) {
        LOGW("compositeStroke: la capa del trazo ya no existe (idx=%d id=%d)", strokeLayerIndex_,
             strokeLayerId_);
        return;
    }

    doc_->bindTargetTexture(layer->texture);
    glViewport(0, 0, docResolution_, docResolution_);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    strokeCompositeShader_.bind();
    baseSnapshot_.bind(0);
    strokeMask_.bind(1);
    predictedMask_.bind(2);
    strokeCompositeShader_.set("uBaseTex", 0);
    strokeCompositeShader_.set("uStrokeMask", 1);
    strokeCompositeShader_.set("uPredictedMask", 2);
    strokeCompositeShader_.set("uUsePredicted", predictedDrawnLastFrame_ ? 1 : 0);
    strokeCompositeShader_.set(
        "uBrushColor",
        Vec4(brush_.color.x, brush_.color.y, brush_.color.z, clampf(brush_.opacity, 0.0f, 1.0f)));
    strokeCompositeShader_.set("uMode", static_cast<int>(mode_));
    strokeCompositeShader_.set("uAlphaLock",
                               (brush_.alphaLock || layer->info.alphaLock) ? 1 : 0);
    drawFullscreenTriangle();

    Framebuffer::unbind();
    GL_CHECK("PaintEngine::compositeStroke");
}

// ---------------------------------------------------------------------------
// Rectangulo sucio e historial
// ---------------------------------------------------------------------------
bool PaintEngine::computeStrokeBounds(int& outX, int& outY, int& outW, int& outH) {
    if (reducePyramid_.empty()) return false;

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    reduceShader_.bind();
    const Texture2D* src = &strokeMask_;
    for (Texture2D& level : reducePyramid_) {
        doc_->bindTargetTexture(level);
        glViewport(0, 0, level.width(), level.height());
        src->bind(0);
        reduceShader_.set("uTex", 0);
        const float ts = 1.0f / static_cast<float>(src->width());
        reduceShader_.set("uSrcTexelSize", Vec2(ts, ts));
        drawFullscreenTriangle();
        src = &level;
    }

    const int side = src->width();
    std::vector<uint8_t> pixels(static_cast<size_t>(side) * static_cast<size_t>(side) * 4u);
    doc_->bindTargetTexture(*src);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    Framebuffer::unbind();

    int minX = side, minY = side, maxX = -1, maxY = -1;
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            if (pixels[(static_cast<size_t>(y) * side + x) * 4u] > 0) {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }
    // Mascara vacia: el trazo cayo fuera del modelo, no hay nada que registrar.
    if (maxX < 0) return false;

    // Del espacio reducido al del atlas, con una celda de margen porque la
    // reduccion redondea hacia dentro.
    const int cell = docResolution_ / side;
    outX = std::max((minX - 1) * cell, 0);
    outY = std::max((minY - 1) * cell, 0);
    const int x1 = std::min((maxX + 2) * cell, docResolution_);
    const int y1 = std::min((maxY + 2) * cell, docResolution_);
    outW = x1 - outX;
    outH = y1 - outY;
    return outW > 0 && outH > 0;
}

void PaintEngine::pushStrokePatch() {
    Layer* layer = doc_->layerAt(strokeLayerIndex_);
    if (layer == nullptr || layer->id != strokeLayerId_) return;

    int x = 0, y = 0, w = 0, h = 0;
    if (!computeStrokeBounds(x, y, w, h)) return;

    PixelPatch patch;
    patch.layerId = strokeLayerId_;
    patch.x = x;
    patch.y = y;
    patch.w = w;
    patch.h = h;
    patch.label = mode_ == PaintMode::Erase ? "Borrar" : "Pintar";

    if (!doc_->readRegion(baseSnapshot_, x, y, w, h, patch.before)) return;
    if (!doc_->readRegion(layer->texture, x, y, w, h, patch.after)) return;

    undoStack_.push(std::move(patch));
}

void PaintEngine::captureFullLayerPatch(int layerIndex, const std::vector<uint8_t>& before,
                                        const char* label) {
    Layer* layer = doc_->layerAt(layerIndex);
    if (layer == nullptr || before.empty()) return;

    PixelPatch patch;
    patch.layerId = layer->id;
    patch.x = 0;
    patch.y = 0;
    patch.w = docResolution_;
    patch.h = docResolution_;
    patch.label = label != nullptr ? label : "";
    patch.before = before;
    if (!doc_->readRegion(layer->texture, 0, 0, docResolution_, docResolution_, patch.after)) {
        return;
    }
    undoStack_.push(std::move(patch));
}

namespace {

int findLayerIndexById(Document* doc, int layerId) {
    for (int i = 0; i < doc->layerCount(); ++i) {
        const Layer* l = doc->layerAt(i);
        if (l != nullptr && l->id == layerId) return i;
    }
    return -1;
}

}  // namespace

bool PaintEngine::undo() {
    if (doc_ == nullptr) return false;
    const PixelPatch* patch = undoStack_.undo();
    if (patch == nullptr) return false;

    const int index = findLayerIndexById(doc_, patch->layerId);
    Layer* layer = doc_->layerAt(index);
    if (layer == nullptr) return false;

    doc_->writeRegion(layer->texture, patch->x, patch->y, patch->w, patch->h,
                      patch->before.data());
    doc_->markDirty();
    return true;
}

bool PaintEngine::redo() {
    if (doc_ == nullptr) return false;
    const PixelPatch* patch = undoStack_.redo();
    if (patch == nullptr) return false;

    const int index = findLayerIndexById(doc_, patch->layerId);
    Layer* layer = doc_->layerAt(index);
    if (layer == nullptr) return false;

    doc_->writeRegion(layer->texture, patch->x, patch->y, patch->w, patch->h, patch->after.data());
    doc_->markDirty();
    return true;
}

}  // namespace uvp
