// Puente JNI. Todas las funciones marcadas como "hilo de render" deben
// invocarse desde el RenderThread de Kotlin; las de entrada son seguras desde
// el hilo de UI porque van a una cola con mutex.
#include <jni.h>

#include <android/native_window_jni.h>

#include <cstring>
#include <string>
#include <vector>

#include "core/Log.h"
#include "engine/Engine.h"

using uvp::Engine;
using uvp::InputCommand;

namespace {

Engine* engineFrom(jlong handle) { return reinterpret_cast<Engine*>(handle); }

std::string toStdString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    std::string result = chars != nullptr ? chars : "";
    if (chars != nullptr) env->ReleaseStringUTFChars(value, chars);
    return result;
}

// Bits de `flags` en nativeSetBrush.
constexpr int kFlagPressureSize = 1 << 0;
constexpr int kFlagPressureOpacity = 1 << 1;
constexpr int kFlagDepthTest = 1 << 2;
constexpr int kFlagBackfaceCull = 1 << 3;
constexpr int kFlagAlphaLock = 1 << 4;
constexpr int kFlagRestrictIsland = 1 << 5;
constexpr int kFlagRestrictRegion = 1 << 6;
constexpr int kFlagTipFollowsStroke = 1 << 7;
constexpr int kFlagShapeFromCenter = 1 << 8;
constexpr int kFlagLockSizeToSurface = 1 << 9;
constexpr int kFlagFillClosedArea = 1 << 10;

}  // namespace

extern "C" {

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------
JNIEXPORT jlong JNICALL Java_com_uvpainter_engine_NativeBridge_nativeCreate(JNIEnv*, jobject) {
    return reinterpret_cast<jlong>(new Engine());
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeDestroy(JNIEnv*, jobject,
                                                                           jlong handle) {
    delete engineFrom(handle);
}

JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSurfaceCreated(
    JNIEnv* env, jobject, jlong handle, jobject surface) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr || surface == nullptr) return JNI_FALSE;
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        LOGE("ANativeWindow_fromSurface devolvio nulo");
        return JNI_FALSE;
    }
    return engine->onSurfaceCreated(window) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSurfaceChanged(
    JNIEnv*, jobject, jlong handle, jint width, jint height) {
    Engine* engine = engineFrom(handle);
    if (engine != nullptr) engine->onSurfaceChanged(width, height);
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSurfaceDestroyed(
    JNIEnv*, jobject, jlong handle) {
    Engine* engine = engineFrom(handle);
    if (engine != nullptr) engine->onSurfaceDestroyed();
}

JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeDrawFrame(JNIEnv*, jobject,
                                                                                  jlong handle) {
    Engine* engine = engineFrom(handle);
    return (engine != nullptr && engine->drawFrame()) ? JNI_TRUE : JNI_FALSE;
}

// ---------------------------------------------------------------------------
// Contenido
// ---------------------------------------------------------------------------
JNIEXPORT jstring JNICALL Java_com_uvpainter_engine_NativeBridge_nativeLoadModel(
    JNIEnv* env, jobject, jlong handle, jbyteArray data, jstring extension, jstring name) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr || data == nullptr) return env->NewStringUTF("Motor no inicializado");

    const jsize size = env->GetArrayLength(data);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    if (bytes == nullptr) return env->NewStringUTF("No se pudo leer el archivo");

    std::string error;
    const bool ok = engine->loadModel(reinterpret_cast<const uint8_t*>(bytes),
                                      static_cast<size_t>(size), toStdString(env, extension),
                                      toStdString(env, name), error);
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
    return ok ? nullptr : env->NewStringUTF(error.c_str());
}

JNIEXPORT jstring JNICALL Java_com_uvpainter_engine_NativeBridge_nativeGetModelName(
    JNIEnv* env, jobject, jlong handle) {
    Engine* engine = engineFrom(handle);
    return env->NewStringUTF(engine != nullptr ? engine->modelName().c_str() : "");
}

// ---------------------------------------------------------------------------
// Proyecto
// ---------------------------------------------------------------------------
JNIEXPORT jstring JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSaveProject(
    JNIEnv* env, jobject, jlong handle, jstring path) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return env->NewStringUTF("Motor no inicializado");
    std::string error;
    if (engine->saveProject(toStdString(env, path), error)) return nullptr;
    return env->NewStringUTF(error.c_str());
}

JNIEXPORT jstring JNICALL Java_com_uvpainter_engine_NativeBridge_nativeLoadProject(
    JNIEnv* env, jobject, jlong handle, jstring path) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return env->NewStringUTF("Motor no inicializado");
    std::string error;
    if (engine->loadProject(toStdString(env, path), error)) return nullptr;
    return env->NewStringUTF(error.c_str());
}

JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeCreateDocument(
    JNIEnv*, jobject, jlong handle, jint resolution) {
    Engine* engine = engineFrom(handle);
    return (engine != nullptr && engine->createDocument(resolution)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSetOrientation(
    JNIEnv*, jobject, jlong handle, jint upAxis, jboolean flipUp, jint quarterTurns) {
    Engine* engine = engineFrom(handle);
    if (engine != nullptr) engine->setOrientation(upAxis, flipUp == JNI_TRUE, quarterTurns);
}

JNIEXPORT jintArray JNICALL Java_com_uvpainter_engine_NativeBridge_nativeGetOrientation(
    JNIEnv* env, jobject, jlong handle) {
    Engine* engine = engineFrom(handle);
    jint values[3] = {0, 0, 0};
    if (engine != nullptr) {
        values[0] = engine->upAxis();
        values[1] = engine->flipUp() ? 1 : 0;
        values[2] = engine->quarterTurns();
    }
    jintArray result = env->NewIntArray(3);
    env->SetIntArrayRegion(result, 0, 3, values);
    return result;
}

JNIEXPORT jintArray JNICALL Java_com_uvpainter_engine_NativeBridge_nativeGetMeshStats(
    JNIEnv* env, jobject, jlong handle) {
    Engine* engine = engineFrom(handle);
    jint values[6] = {0, 0, 0, 0, 0, 0};
    if (engine != nullptr) {
        const uvp::MeshStats& stats = engine->meshStats();
        values[0] = stats.vertexCount;
        values[1] = stats.triangleCount;
        values[2] = stats.submeshCount;
        values[3] = stats.hasUVs ? 1 : 0;
        values[4] = stats.uvsOutside01 ? 1 : 0;
        values[5] = stats.islandCount;
    }
    jintArray result = env->NewIntArray(6);
    env->SetIntArrayRegion(result, 0, 6, values);
    return result;
}

// ---------------------------------------------------------------------------
// Entrada (hilo de UI)
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeStrokeBegin(
    JNIEnv*, jobject, jlong handle, jfloat x, jfloat y, jfloat pressure, jfloat tilt,
    jfloat orientation, jdouble timeMs) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    InputCommand cmd;
    cmd.kind = InputCommand::Kind::StrokeBegin;
    cmd.x = x;
    cmd.y = y;
    cmd.pressure = pressure;
    cmd.tilt = tilt;
    cmd.orientation = orientation;
    cmd.timeMs = timeMs;
    engine->pushCommand(cmd);
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeStrokeMove(
    JNIEnv*, jobject, jlong handle, jfloat x, jfloat y, jfloat pressure, jfloat tilt,
    jfloat orientation, jdouble timeMs) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    InputCommand cmd;
    cmd.kind = InputCommand::Kind::StrokeMove;
    cmd.x = x;
    cmd.y = y;
    cmd.pressure = pressure;
    cmd.tilt = tilt;
    cmd.orientation = orientation;
    cmd.timeMs = timeMs;
    engine->pushCommand(cmd);
}

// Version por lotes: los puntos historicos del MotionEvent llegan de golpe y
// no queremos pagar una llamada JNI por cada uno.
JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeStrokeMoveBatch(
    JNIEnv* env, jobject, jlong handle, jfloatArray points, jint count) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr || points == nullptr || count <= 0) return;

    jfloat* values = env->GetFloatArrayElements(points, nullptr);
    if (values == nullptr) return;

    // Cada punto ocupa 6 floats: x, y, presion, tilt, orientacion, tiempo.
    for (jint i = 0; i < count; ++i) {
        const jfloat* p = values + static_cast<size_t>(i) * 6u;
        InputCommand cmd;
        cmd.kind = InputCommand::Kind::StrokeMove;
        cmd.x = p[0];
        cmd.y = p[1];
        cmd.pressure = p[2];
        cmd.tilt = p[3];
        cmd.orientation = p[4];
        cmd.timeMs = static_cast<double>(p[5]);
        engine->pushCommand(cmd);
    }
    env->ReleaseFloatArrayElements(points, values, JNI_ABORT);
}

// Puntos extrapolados por androidx.input: van a una mascara que se descarta en
// cada frame, nunca se graban en la capa.
JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeStrokePredictBatch(
    JNIEnv* env, jobject, jlong handle, jfloatArray points, jint count) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr || points == nullptr || count <= 0) return;

    jfloat* values = env->GetFloatArrayElements(points, nullptr);
    if (values == nullptr) return;

    for (jint i = 0; i < count; ++i) {
        const jfloat* p = values + static_cast<size_t>(i) * 6u;
        InputCommand cmd;
        cmd.kind = InputCommand::Kind::StrokePredict;
        cmd.x = p[0];
        cmd.y = p[1];
        cmd.pressure = p[2];
        cmd.tilt = p[3];
        cmd.orientation = p[4];
        cmd.timeMs = static_cast<double>(p[5]);
        engine->pushCommand(cmd);
    }
    env->ReleaseFloatArrayElements(points, values, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeStrokeEnd(JNIEnv*, jobject,
                                                                             jlong handle) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    InputCommand cmd;
    cmd.kind = InputCommand::Kind::StrokeEnd;
    engine->pushCommand(cmd);
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeStrokeCancel(JNIEnv*, jobject,
                                                                                jlong handle) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    InputCommand cmd;
    cmd.kind = InputCommand::Kind::StrokeCancel;
    engine->pushCommand(cmd);
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeCameraGesture(
    JNIEnv*, jobject, jlong handle, jint kind, jfloat a, jfloat b) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    InputCommand cmd;
    switch (kind) {
        case 0: cmd.kind = InputCommand::Kind::Orbit; break;
        case 1: cmd.kind = InputCommand::Kind::Pan; break;
        case 2: cmd.kind = InputCommand::Kind::Zoom; break;
        case 3: cmd.kind = InputCommand::Kind::Roll; break;
        default: return;
    }
    cmd.x = a;
    cmd.y = b;
    engine->pushCommand(cmd);
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeHover(JNIEnv*, jobject,
                                                                         jlong handle, jfloat x,
                                                                         jfloat y,
                                                                         jfloat pressure) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    InputCommand cmd;
    cmd.kind = InputCommand::Kind::Hover;
    cmd.x = x;
    cmd.y = y;
    cmd.pressure = pressure;
    engine->pushCommand(cmd);
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeHoverEnd(JNIEnv*, jobject,
                                                                            jlong handle) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    InputCommand cmd;
    cmd.kind = InputCommand::Kind::HoverEnd;
    engine->pushCommand(cmd);
}

// ---------------------------------------------------------------------------
// Pincel y visor (hilo de render)
// ---------------------------------------------------------------------------
/**
 * El pincel viaja como dos arrays en vez de treinta parametros sueltos: la
 * firma no cambia cada vez que se anade un ajuste, y el orden esta documentado
 * en NativeBridge.kt junto a las constantes que lo indexan.
 *
 * floats: 0 radio, 1 dureza, 2 opacidad, 3 flujo, 4 espaciado, 5-7 color rgb,
 *         8 suavizado, 9 regularizador, 10 ganancia de presion, 11 curva,
 *         12 suelo de tamano, 13 suelo de opacidad, 14 inclinacion,
 *         15 corte de orientacion, 16 orientacion plena, 17 aspecto de punta,
 *         18 angulo de punta, 19 grano, 20 escala de grano
 * ints:   0 flags, 1 forma de punta, 2 figura, 3 lados del poligono
 */
JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSetBrush(
    JNIEnv* env, jobject, jlong handle, jfloatArray values, jintArray options) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr || values == nullptr || options == nullptr) return;
    if (env->GetArrayLength(values) < 21 || env->GetArrayLength(options) < 4) return;

    jfloat* f = env->GetFloatArrayElements(values, nullptr);
    jint* i = env->GetIntArrayElements(options, nullptr);
    if (f == nullptr || i == nullptr) return;

    const int flags = i[0];
    uvp::BrushSettings brush;
    brush.radiusPx = f[0];
    brush.hardness = f[1];
    brush.opacity = f[2];
    brush.flow = f[3];
    brush.spacingPx = f[4];
    brush.color = {f[5], f[6], f[7]};
    brush.smoothing = f[8];
    brush.stabilizerRadiusPx = f[9];
    brush.pressureGain = f[10];
    brush.pressureCurve = f[11];
    brush.pressureSizeFloor = f[12];
    brush.pressureOpacityFloor = f[13];
    brush.tiltAffectsSize = f[14];
    brush.facingCutoff = f[15];
    brush.facingFull = f[16];
    brush.tipAspect = f[17];
    brush.tipAngle = f[18];
    brush.grainAmount = f[19];
    brush.grainScale = f[20];
    brush.fillTolerance = f[21];

    brush.tipShape = static_cast<uvp::TipShape>(i[1]);
    brush.shape = static_cast<uvp::ShapeKind>(i[2]);
    brush.polygonSides = i[3];

    brush.pressureAffectsSize = (flags & kFlagPressureSize) != 0;
    brush.pressureAffectsOpacity = (flags & kFlagPressureOpacity) != 0;
    brush.depthTest = (flags & kFlagDepthTest) != 0;
    brush.backfaceCull = (flags & kFlagBackfaceCull) != 0;
    brush.alphaLock = (flags & kFlagAlphaLock) != 0;
    brush.restrictToIsland = (flags & kFlagRestrictIsland) != 0;
    brush.restrictToRegion = (flags & kFlagRestrictRegion) != 0;
    brush.tipFollowsStroke = (flags & kFlagTipFollowsStroke) != 0;
    brush.shapeFromCenter = (flags & kFlagShapeFromCenter) != 0;
    brush.lockSizeToSurface = (flags & kFlagLockSizeToSurface) != 0;
    brush.fillClosedArea = (flags & kFlagFillClosedArea) != 0;

    env->ReleaseFloatArrayElements(values, f, JNI_ABORT);
    env->ReleaseIntArrayElements(options, i, JNI_ABORT);
    engine->setBrush(brush);
}

// --- Limites dibujados a mano ---------------------------------------------
JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeClearBoundary(JNIEnv*, jobject,
                                                                                 jlong handle) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    engine->document().clearBoundary();
}

// True en cuanto hay algo trazado, sea un area cerrada o una linea suelta: las
// dos frenan al pincel, aunque solo la cerrada genere regiones etiquetadas.
JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeHasBoundary(
    JNIEnv*, jobject, jlong handle) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return JNI_FALSE;
    return engine->document().hasBoundaryContent() ? JNI_TRUE : JNI_FALSE;
}

// --- Miniaturas de capa ----------------------------------------------------
JNIEXPORT jbyteArray JNICALL Java_com_uvpainter_engine_NativeBridge_nativeLayerThumbnail(
    JNIEnv* env, jobject, jlong handle, jint index, jint size) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return nullptr;

    std::vector<uint8_t> rgba;
    if (!engine->document().readLayerThumbnail(index, size, rgba) || rgba.empty()) return nullptr;

    jbyteArray result = env->NewByteArray(static_cast<jsize>(rgba.size()));
    env->SetByteArrayRegion(result, 0, static_cast<jsize>(rgba.size()),
                            reinterpret_cast<const jbyte*>(rgba.data()));
    return result;
}

/// Bote de pintura sobre la isla UV que haya bajo el punto indicado.
JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeFillAt(JNIEnv*, jobject,
                                                                          jlong handle, jfloat x,
                                                                          jfloat y) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    InputCommand cmd;
    cmd.kind = InputCommand::Kind::Fill;
    cmd.x = x;
    cmd.y = y;
    engine->pushCommand(cmd);
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSetPaintMode(JNIEnv*, jobject,
                                                                                jlong handle,
                                                                                jint mode) {
    Engine* engine = engineFrom(handle);
    if (engine != nullptr) engine->setPaintMode(static_cast<uvp::PaintMode>(mode));
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSetViewport(
    JNIEnv*, jobject, jlong handle, jint viewMode, jboolean wireframe, jboolean seams,
    jboolean backfaceCull, jboolean unpaintedTint, jfloat roughness, jfloat metallic,
    jfloat uvCheckerDensity, jfloat unlitShading) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    uvp::ViewportSettings& settings = engine->viewportSettings();
    settings.mode = static_cast<uvp::ViewMode>(viewMode);
    settings.showWireframe = wireframe == JNI_TRUE;
    settings.showSeams = seams == JNI_TRUE;
    settings.backfaceCull = backfaceCull == JNI_TRUE;
    settings.showUnpaintedTint = unpaintedTint == JNI_TRUE;
    settings.roughness = roughness;
    settings.metallic = metallic;
    settings.uvCheckerDensity = uvCheckerDensity;
    settings.unlitShading = unlitShading;
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeResetView(JNIEnv*, jobject,
                                                                             jlong handle) {
    Engine* engine = engineFrom(handle);
    if (engine != nullptr) engine->resetView();
}

JNIEXPORT jint JNICALL Java_com_uvpainter_engine_NativeBridge_nativePickColor(JNIEnv*, jobject,
                                                                             jlong handle, jint x,
                                                                             jint y) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return 0;
    return static_cast<jint>(engine->pickScreenColor(x, y));
}

// ---------------------------------------------------------------------------
// Capas (hilo de render)
// ---------------------------------------------------------------------------
JNIEXPORT jint JNICALL Java_com_uvpainter_engine_NativeBridge_nativeLayerCount(JNIEnv*, jobject,
                                                                              jlong handle) {
    Engine* engine = engineFrom(handle);
    return engine != nullptr ? engine->document().layerCount() : 0;
}

JNIEXPORT jint JNICALL Java_com_uvpainter_engine_NativeBridge_nativeAddLayer(JNIEnv* env, jobject,
                                                                            jlong handle,
                                                                            jstring name) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return -1;
    const int above = engine->document().activeIndex();
    return engine->document().addLayer(toStdString(env, name), above);
}

JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeRemoveLayer(
    JNIEnv*, jobject, jlong handle, jint index) {
    Engine* engine = engineFrom(handle);
    return (engine != nullptr && engine->document().removeLayer(index)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeDuplicateLayer(
    JNIEnv*, jobject, jlong handle, jint index) {
    Engine* engine = engineFrom(handle);
    return (engine != nullptr && engine->document().duplicateLayer(index)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeMoveLayer(
    JNIEnv*, jobject, jlong handle, jint from, jint to) {
    Engine* engine = engineFrom(handle);
    return (engine != nullptr && engine->document().moveLayer(from, to)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSetActiveLayer(
    JNIEnv*, jobject, jlong handle, jint index) {
    Engine* engine = engineFrom(handle);
    if (engine != nullptr) engine->document().setActiveIndex(index);
}

JNIEXPORT jint JNICALL Java_com_uvpainter_engine_NativeBridge_nativeGetActiveLayer(JNIEnv*, jobject,
                                                                                  jlong handle) {
    Engine* engine = engineFrom(handle);
    return engine != nullptr ? engine->document().activeIndex() : 0;
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeSetLayerProps(
    JNIEnv* env, jobject, jlong handle, jint index, jstring name, jfloat opacity, jint blend,
    jboolean visible, jboolean locked, jboolean alphaLock, jboolean clipToBelow) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    uvp::Layer* layer = engine->document().layerAt(index);
    if (layer == nullptr) return;
    if (name != nullptr) layer->info.name = toStdString(env, name);
    layer->info.opacity = opacity;
    layer->info.blend = static_cast<uvp::BlendMode>(blend);
    layer->info.visible = visible == JNI_TRUE;
    layer->info.locked = locked == JNI_TRUE;
    layer->info.alphaLock = alphaLock == JNI_TRUE;
    layer->info.clipToBelow = clipToBelow == JNI_TRUE;
    engine->document().markDirty();
}

JNIEXPORT jobjectArray JNICALL Java_com_uvpainter_engine_NativeBridge_nativeGetLayerNames(
    JNIEnv* env, jobject, jlong handle) {
    Engine* engine = engineFrom(handle);
    const int count = engine != nullptr ? engine->document().layerCount() : 0;
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray result = env->NewObjectArray(count, stringClass, nullptr);
    for (int i = 0; i < count; ++i) {
        const uvp::Layer* layer = engine->document().layerAt(i);
        jstring name = env->NewStringUTF(layer != nullptr ? layer->info.name.c_str() : "");
        env->SetObjectArrayElement(result, i, name);
        env->DeleteLocalRef(name);
    }
    return result;
}

// Devuelve, por capa: opacidad, modo de fusion, visible, bloqueada,
// bloqueo de alfa, recorte. Seis valores consecutivos.
JNIEXPORT jfloatArray JNICALL Java_com_uvpainter_engine_NativeBridge_nativeGetLayerProps(
    JNIEnv* env, jobject, jlong handle) {
    Engine* engine = engineFrom(handle);
    const int count = engine != nullptr ? engine->document().layerCount() : 0;
    std::vector<jfloat> values(static_cast<size_t>(count) * 6u, 0.0f);
    for (int i = 0; i < count; ++i) {
        const uvp::Layer* layer = engine->document().layerAt(i);
        if (layer == nullptr) continue;
        const size_t base = static_cast<size_t>(i) * 6u;
        values[base + 0] = layer->info.opacity;
        values[base + 1] = static_cast<jfloat>(static_cast<int>(layer->info.blend));
        values[base + 2] = layer->info.visible ? 1.0f : 0.0f;
        values[base + 3] = layer->info.locked ? 1.0f : 0.0f;
        values[base + 4] = layer->info.alphaLock ? 1.0f : 0.0f;
        values[base + 5] = layer->info.clipToBelow ? 1.0f : 0.0f;
    }
    jfloatArray result = env->NewFloatArray(static_cast<jsize>(values.size()));
    if (!values.empty()) {
        env->SetFloatArrayRegion(result, 0, static_cast<jsize>(values.size()), values.data());
    }
    return result;
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeClearLayer(JNIEnv*, jobject,
                                                                              jlong handle,
                                                                              jint index) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    std::vector<uint8_t> before;
    engine->document().readLayer(index, before);
    engine->document().clearLayer(index);
    engine->paint().captureFullLayerPatch(index, before, "Vaciar capa");
}

JNIEXPORT void JNICALL Java_com_uvpainter_engine_NativeBridge_nativeFillLayer(
    JNIEnv*, jobject, jlong handle, jint index, jfloat r, jfloat g, jfloat b, jfloat a) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return;
    std::vector<uint8_t> before;
    engine->document().readLayer(index, before);
    engine->document().fillLayer(index, {r, g, b, a});
    engine->paint().captureFullLayerPatch(index, before, "Rellenar capa");
}

// ---------------------------------------------------------------------------
// Historial
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeUndo(JNIEnv*, jobject,
                                                                            jlong handle) {
    Engine* engine = engineFrom(handle);
    return (engine != nullptr && engine->paint().undo()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeRedo(JNIEnv*, jobject,
                                                                            jlong handle) {
    Engine* engine = engineFrom(handle);
    return (engine != nullptr && engine->paint().redo()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jintArray JNICALL Java_com_uvpainter_engine_NativeBridge_nativeHistoryState(
    JNIEnv* env, jobject, jlong handle) {
    Engine* engine = engineFrom(handle);
    jint values[3] = {0, 0, 0};
    if (engine != nullptr) {
        values[0] = engine->paint().canUndo() ? 1 : 0;
        values[1] = engine->paint().canRedo() ? 1 : 0;
        values[2] = static_cast<jint>(engine->paint().history().memoryUsed() / (1024u * 1024u));
    }
    jintArray result = env->NewIntArray(3);
    env->SetIntArrayRegion(result, 0, 3, values);
    return result;
}

// ---------------------------------------------------------------------------
// Exportacion / importacion
// ---------------------------------------------------------------------------
JNIEXPORT jint JNICALL Java_com_uvpainter_engine_NativeBridge_nativeDocumentResolution(
    JNIEnv*, jobject, jlong handle) {
    Engine* engine = engineFrom(handle);
    return engine != nullptr ? engine->document().resolution() : 0;
}

JNIEXPORT jbyteArray JNICALL Java_com_uvpainter_engine_NativeBridge_nativeExportComposite(
    JNIEnv* env, jobject, jlong handle, jboolean dilate) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return nullptr;

    std::vector<uint8_t> rgba;
    int size = 0;
    if (!engine->exportComposite(rgba, size, dilate == JNI_TRUE) || rgba.empty()) return nullptr;

    jbyteArray result = env->NewByteArray(static_cast<jsize>(rgba.size()));
    env->SetByteArrayRegion(result, 0, static_cast<jsize>(rgba.size()),
                            reinterpret_cast<const jbyte*>(rgba.data()));
    return result;
}

JNIEXPORT jbyteArray JNICALL Java_com_uvpainter_engine_NativeBridge_nativeExportLayer(
    JNIEnv* env, jobject, jlong handle, jint index) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr) return nullptr;

    std::vector<uint8_t> rgba;
    int size = 0;
    if (!engine->exportLayer(index, rgba, size) || rgba.empty()) return nullptr;

    jbyteArray result = env->NewByteArray(static_cast<jsize>(rgba.size()));
    env->SetByteArrayRegion(result, 0, static_cast<jsize>(rgba.size()),
                            reinterpret_cast<const jbyte*>(rgba.data()));
    return result;
}

JNIEXPORT jboolean JNICALL Java_com_uvpainter_engine_NativeBridge_nativeImportLayerPixels(
    JNIEnv* env, jobject, jlong handle, jint index, jbyteArray pixels, jint size) {
    Engine* engine = engineFrom(handle);
    if (engine == nullptr || pixels == nullptr) return JNI_FALSE;

    jbyte* data = env->GetByteArrayElements(pixels, nullptr);
    if (data == nullptr) return JNI_FALSE;
    const bool ok =
        engine->importLayerPixels(index, reinterpret_cast<const uint8_t*>(data), size);
    env->ReleaseByteArrayElements(pixels, data, JNI_ABORT);
    return ok ? JNI_TRUE : JNI_FALSE;
}

}  // extern "C"
