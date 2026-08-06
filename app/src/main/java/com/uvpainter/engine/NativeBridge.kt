package com.uvpainter.engine

import android.view.Surface

/**
 * Puente con el motor nativo.
 *
 * Reglas de hilos:
 *  - Todo lo marcado como "hilo de render" DEBE llamarse desde [RenderThread];
 *    toca OpenGL y el contexto EGL solo esta activo alli.
 *  - Las funciones de entrada (`nativeStroke*`, `nativeCameraGesture`,
 *    `nativeHover*`) se pueden llamar desde el hilo de UI: van a una cola
 *    protegida con mutex y se vacian al principio de cada frame. Es a proposito:
 *    saltar por un Handler antes de encolar el trazo anadiria latencia al lapiz.
 */
object NativeBridge {

    init {
        System.loadLibrary("uvpainter")
    }

    // Bits de `flags` en [nativeSetBrush].
    const val FLAG_PRESSURE_SIZE = 1 shl 0
    const val FLAG_PRESSURE_OPACITY = 1 shl 1
    const val FLAG_DEPTH_TEST = 1 shl 2
    const val FLAG_BACKFACE_CULL = 1 shl 3
    const val FLAG_ALPHA_LOCK = 1 shl 4
    const val FLAG_RESTRICT_ISLAND = 1 shl 5
    const val FLAG_RESTRICT_REGION = 1 shl 6
    const val FLAG_TIP_FOLLOWS_STROKE = 1 shl 7
    const val FLAG_SHAPE_FROM_CENTER = 1 shl 8
    const val FLAG_LOCK_SIZE_TO_SURFACE = 1 shl 9

    // Índices del array de floats de [nativeSetBrush]. Van como array y no como
    // parámetros sueltos para que añadir un ajuste no cambie la firma JNI.
    const val B_RADIUS = 0
    const val B_HARDNESS = 1
    const val B_OPACITY = 2
    const val B_FLOW = 3
    const val B_SPACING = 4
    const val B_RED = 5
    const val B_GREEN = 6
    const val B_BLUE = 7
    const val B_SMOOTHING = 8
    const val B_STABILIZER = 9
    const val B_PRESSURE_GAIN = 10
    const val B_PRESSURE_CURVE = 11
    const val B_SIZE_FLOOR = 12
    const val B_OPACITY_FLOOR = 13
    const val B_TILT_SIZE = 14
    const val B_FACING_CUTOFF = 15
    const val B_FACING_FULL = 16
    const val B_TIP_ASPECT = 17
    const val B_TIP_ANGLE = 18
    const val B_GRAIN_AMOUNT = 19
    const val B_GRAIN_SCALE = 20
    const val B_FLOAT_COUNT = 21

    // Índices del array de enteros.
    const val B_FLAGS = 0
    const val B_TIP_SHAPE = 1
    const val B_SHAPE = 2
    const val B_POLYGON_SIDES = 3
    const val B_INT_COUNT = 4

    // Tipos de gesto de camara en [nativeCameraGesture].
    const val GESTURE_ORBIT = 0
    const val GESTURE_PAN = 1
    const val GESTURE_ZOOM = 2
    const val GESTURE_ROLL = 3

    // --- Ciclo de vida (hilo de render) ------------------------------------
    external fun nativeCreate(): Long
    external fun nativeDestroy(handle: Long)
    external fun nativeSurfaceCreated(handle: Long, surface: Surface): Boolean
    external fun nativeSurfaceChanged(handle: Long, width: Int, height: Int)
    external fun nativeSurfaceDestroyed(handle: Long)
    external fun nativeDrawFrame(handle: Long): Boolean

    // --- Contenido (hilo de render) ----------------------------------------
    /** Devuelve null si todo fue bien, o el mensaje de error si no. */
    external fun nativeLoadModel(
        handle: Long,
        data: ByteArray,
        extension: String,
        name: String,
    ): String?
    external fun nativeCreateDocument(handle: Long, resolution: Int): Boolean

    // --- Proyecto (hilo de render) -----------------------------------------
    // El archivo lleva dentro el modelo original, las capas y la pared dibujada
    // a mano, asi que reabrirlo no depende de que el .glb siga donde estaba.
    /** Devuelve null si todo fue bien, o el mensaje de error si no. */
    external fun nativeSaveProject(handle: Long, path: String): String?
    external fun nativeLoadProject(handle: Long, path: String): String?
    /** Nombre del archivo de modelo cargado, o cadena vacia si no hay ninguno. */
    external fun nativeGetModelName(handle: Long): String
    /** [vertices, triangulos, submallas, tieneUV, uvFuera01, islasUV] */
    external fun nativeGetMeshStats(handle: Long): IntArray

    /** upAxis: 0 = Y arriba (glTF/Unity), 1 = Z arriba (Blender/Max). */
    external fun nativeSetOrientation(
        handle: Long,
        upAxis: Int,
        flipUp: Boolean,
        quarterTurns: Int,
    )

    /** [upAxis, flipUp, quarterTurns] */
    external fun nativeGetOrientation(handle: Long): IntArray

    // --- Entrada (hilo de UI) ----------------------------------------------
    external fun nativeStrokeBegin(
        handle: Long,
        x: Float,
        y: Float,
        pressure: Float,
        tilt: Float,
        orientation: Float,
        timeMs: Double,
    )

    external fun nativeStrokeMove(
        handle: Long,
        x: Float,
        y: Float,
        pressure: Float,
        tilt: Float,
        orientation: Float,
        timeMs: Double,
    )

    /** `points` son grupos de 6 floats: x, y, presion, tilt, orientacion, tiempo. */
    external fun nativeStrokeMoveBatch(handle: Long, points: FloatArray, count: Int)

    /** Igual que el anterior pero para puntos extrapolados: no se graban. */
    external fun nativeStrokePredictBatch(handle: Long, points: FloatArray, count: Int)

    external fun nativeStrokeEnd(handle: Long)
    external fun nativeStrokeCancel(handle: Long)
    external fun nativeCameraGesture(handle: Long, kind: Int, a: Float, b: Float)
    external fun nativeHover(handle: Long, x: Float, y: Float, pressure: Float)
    external fun nativeHoverEnd(handle: Long)

    // --- Pincel y visor (hilo de render) -----------------------------------
    external fun nativeSetBrush(handle: Long, values: FloatArray, options: IntArray)

    /** Bote de pintura sobre la isla UV bajo el punto. Hilo de UI. */
    external fun nativeFillAt(handle: Long, x: Float, y: Float)

    external fun nativeSetPaintMode(handle: Long, mode: Int)

    external fun nativeSetViewport(
        handle: Long,
        viewMode: Int,
        wireframe: Boolean,
        seams: Boolean,
        backfaceCull: Boolean,
        unpaintedTint: Boolean,
        roughness: Float,
        metallic: Float,
        uvCheckerDensity: Float,
        unlitShading: Float,
    )

    external fun nativeResetView(handle: Long)
    /** Cuentagotas sobre el visor ya compuesto. Devuelve 0xAARRGGBB. */
    external fun nativePickColor(handle: Long, x: Int, y: Int): Int

    // --- Capas (hilo de render) --------------------------------------------
    external fun nativeLayerCount(handle: Long): Int
    external fun nativeAddLayer(handle: Long, name: String): Int
    external fun nativeRemoveLayer(handle: Long, index: Int): Boolean
    external fun nativeDuplicateLayer(handle: Long, index: Int): Boolean
    external fun nativeMoveLayer(handle: Long, from: Int, to: Int): Boolean
    external fun nativeSetActiveLayer(handle: Long, index: Int)
    external fun nativeGetActiveLayer(handle: Long): Int

    external fun nativeSetLayerProps(
        handle: Long,
        index: Int,
        name: String?,
        opacity: Float,
        blend: Int,
        visible: Boolean,
        locked: Boolean,
        alphaLock: Boolean,
        clipToBelow: Boolean,
    )

    external fun nativeGetLayerNames(handle: Long): Array<String>
    /** Seis floats por capa: opacidad, fusion, visible, bloqueada, alfa, recorte. */
    external fun nativeGetLayerProps(handle: Long): FloatArray

    /** Miniatura RGBA8 de `size`×`size` de una capa. Hilo de render. */
    external fun nativeLayerThumbnail(handle: Long, index: Int, size: Int): ByteArray?

    /** Borra los límites dibujados a mano. Hilo de render. */
    external fun nativeClearBoundary(handle: Long)

    /** True si hay límites que separen la superficie en regiones. */
    external fun nativeHasBoundary(handle: Long): Boolean

    external fun nativeClearLayer(handle: Long, index: Int)
    external fun nativeFillLayer(handle: Long, index: Int, r: Float, g: Float, b: Float, a: Float)

    // --- Historial (hilo de render) ----------------------------------------
    external fun nativeUndo(handle: Long): Boolean
    external fun nativeRedo(handle: Long): Boolean
    /** [puedeDeshacer, puedeRehacer, memoriaMB] */
    external fun nativeHistoryState(handle: Long): IntArray

    // --- Exportacion (hilo de render) --------------------------------------
    external fun nativeDocumentResolution(handle: Long): Int
    external fun nativeExportComposite(handle: Long, dilate: Boolean): ByteArray?
    external fun nativeExportLayer(handle: Long, index: Int): ByteArray?
    external fun nativeImportLayerPixels(
        handle: Long,
        index: Int,
        pixels: ByteArray,
        size: Int,
    ): Boolean
}
