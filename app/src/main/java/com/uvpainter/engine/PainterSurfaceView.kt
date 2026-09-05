package com.uvpainter.engine

import android.content.Context
import android.os.Build
import android.os.SystemClock
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.input.motionprediction.MotionEventPredictor
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.hypot

/** Gestos rapidos con varios dedos, al estilo Procreate. */
enum class QuickGesture { UNDO, REDO, TOGGLE_UI }

/** Que hace el lapiz al tocar: pintar, rellenar o tomar color. */
enum class PenAction { PAINT, FILL, PICK }

/**
 * Superficie de dibujo. Aqui vive todo el manejo del S-Pen.
 *
 * ## Rechazo de palma
 *
 * Enrutar por tipo de herramienta (lapiz pinta, dedo navega) no basta: al
 * apoyar la mano para dibujar, el canto de la palma suele tocar la pantalla
 * ANTES que la punta del lapiz, asi que en ese instante no hay ningun lapiz
 * "cerca" con el que descartarla. Se combinan cuatro defensas:
 *
 *  1. **Dos dedos para navegar.** Una palma apoyada es una mancha, no dos
 *     contactos deliberados. Es la defensa que mas casos cubre.
 *  2. **Filtro por tamano de contacto.** El canto de la mano deja una huella
 *     mucho mas ancha que un dedo; `touchMajor` la delata.
 *  3. **Proximidad del lapiz.** Mientras el S-Pen esta apoyado o flotando en
 *     rango, ningun contacto tactil llega ni al pincel ni a la camara.
 *  4. **Ventana de armado.** Un contacto no mueve la camara hasta que lleva
 *     unos milisegundos en pantalla. Si el lapiz aterriza dentro de esa
 *     ventana, los contactos previos se descartan retroactivamente: era la
 *     mano acomodandose.
 */
class PainterSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : SurfaceView(context, attrs), SurfaceHolder.Callback {

    var engineHandle: Long = 0L
    var renderThread: RenderThread? = null

    // --- Preferencias de entrada -------------------------------------------
    var fingerCanPaint = false

    /** Dedos necesarios para mover la camara. 2 por defecto: rechaza la palma. */
    var navigationMinFingers = 2

    /** Ignora por completo la entrada tactil. Util apoyando la mano entera. */
    var stylusOnlyMode = false

    /** Descarta contactos mas anchos que [palmTouchMajorMm]. */
    var palmSizeRejection = true
    var palmTouchMajorMm = 17f

    var penButtonErases = true
    var twistToRotateView = true

    /** El pellizco acerca hacia su propio centro en vez de hacia el de la pantalla. */
    var zoomToPinchCenter = true

    /** Intercambia los gestos: dos dedos desplazan y tres orbitan. */
    var twoFingerPan = false

    var invertOrbitX = false
    var invertOrbitY = false
    var orbitSensitivity = 1.0f
    var panSensitivity = 1.0f
    var zoomSensitivity = 1.0f

    // --- Avisos a la UI -----------------------------------------------------
    var onQuickGesture: ((QuickGesture) -> Unit)? = null
    var onStylusButtonChanged: ((Boolean) -> Unit)? = null
    var onSurfaceReady: ((width: Int, height: Int) -> Unit)? = null
    var onStrokeFinished: (() -> Unit)? = null
    var onPalmRejected: (() -> Unit)? = null
    var onFillRequested: ((Float, Float) -> Unit)? = null
    var onPickRequested: ((Float, Float) -> Unit)? = null

    /**
     * Un contacto de verdad sobre el lienzo: lapiz que aterriza o dedo que no
     * se ha descartado como palma. Sirve para cerrar los paneles abiertos, que
     * es lo que espera cualquiera que toca fuera de un menu.
     */
    var onViewportTouch: (() -> Unit)? = null

    /** Herramienta activa del lapiz. */
    var penAction = PenAction.PAINT

    private var predictor: MotionEventPredictor? = null

    // --- Estado del trazo ---------------------------------------------------
    private var strokePointerId = -1
    private var strokeStartTimeMs = 0L
    private var stylusButtonDown = false

    /** El lapiz sigue "presente" un rato despues de dejar de detectarlo. */
    private var stylusPresentUntilMs = 0L

    private val strokeBuffer = FloatArray(6 * MAX_POINTS_PER_BATCH)
    private val predictBuffer = FloatArray(6 * MAX_POINTS_PER_BATCH)

    // --- Estado del gesto de dedos ------------------------------------------
    /** Punteros descartados: palma, o contactos presentes cuando llego el pen. */
    private val rejectedPointers = HashSet<Int>()
    private var fingerGestureStartMs = 0L

    private var anchorValid = false
    private var anchorCount = 0
    private var anchorCx = 0f
    private var anchorCy = 0f
    private var anchorDistance = 0f
    private var anchorAngle = 0f

    private var gestureStartTimeMs = 0L
    private var gestureMaxFingers = 0
    private var gestureTravel = 0f

    private val density = resources.displayMetrics.density
    private val xdpi = resources.displayMetrics.xdpi.takeIf { it > 1f } ?: (density * 160f)
    private val tapSlopPx = 24f * density

    init {
        holder.addCallback(this)
        isFocusable = true
        isFocusableInTouchMode = true
        setZOrderMediaOverlay(false)
    }

    // -----------------------------------------------------------------------
    // Ciclo de vida de la superficie
    // -----------------------------------------------------------------------
    override fun surfaceCreated(holder: SurfaceHolder) {
        val handle = engineHandle
        val surface: Surface = holder.surface
        renderThread?.post { NativeBridge.nativeSurfaceCreated(handle, surface) }
        predictor = runCatching { MotionEventPredictor.newInstance(this) }.getOrNull()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        val handle = engineHandle
        renderThread?.post { NativeBridge.nativeSurfaceChanged(handle, width, height) }
        onSurfaceReady?.invoke(width, height)
        renderThread?.requestRender()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        // Hay que bloquear: en cuanto esta funcion retorna la Surface deja de
        // ser valida y el contexto EGL apuntaria a memoria muerta.
        val handle = engineHandle
        renderThread?.postAndWait { NativeBridge.nativeSurfaceDestroyed(handle) }
    }

    // -----------------------------------------------------------------------
    // Entrada
    // -----------------------------------------------------------------------
    override fun onTouchEvent(event: MotionEvent): Boolean {
        val handle = engineHandle
        if (handle == 0L) return false

        predictor?.record(event)
        updateStylusButton(event)

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> onPointerDown(event, handle)
            MotionEvent.ACTION_MOVE -> onPointerMove(event, handle)
            MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_UP -> onPointerUp(event, handle)
            MotionEvent.ACTION_CANCEL -> {
                if (strokePointerId >= 0) {
                    NativeBridge.nativeStrokeCancel(handle)
                    strokePointerId = -1
                }
                resetGesture()
                rejectedPointers.clear()
            }
        }

        renderThread?.requestRender()
        return true
    }

    private fun onPointerDown(event: MotionEvent, handle: Long) {
        val index = event.actionIndex
        val id = event.getPointerId(index)

        if (isPenTool(event.getToolType(index))) {
            // Sin buffer: los eventos dejan de agruparse por frame.
            requestUnbufferedDispatch(event)
            markStylusPresent()

            // Todo lo que ya estaba tocando la pantalla cuando llego el lapiz
            // era la mano acomodandose. Se descarta retroactivamente.
            var rejectedAny = false
            for (i in 0 until event.pointerCount) {
                val otherId = event.getPointerId(i)
                if (otherId != id && !isPenTool(event.getToolType(i))) {
                    rejectedPointers.add(otherId)
                    rejectedAny = true
                }
            }
            resetGesture()
            if (rejectedAny) onPalmRejected?.invoke()
            onViewportTouch?.invoke()

            when (penAction) {
                PenAction.FILL -> onFillRequested?.invoke(event.getX(index), event.getY(index))
                PenAction.PICK -> onPickRequested?.invoke(event.getX(index), event.getY(index))
                PenAction.PAINT -> beginStroke(event, index, id, handle)
            }
            return
        }

        // A partir de aqui es un dedo (o una palma).
        if (stylusOnlyMode || isStylusPresent() || strokePointerId >= 0) {
            rejectedPointers.add(id)
            return
        }
        if (palmSizeRejection && isPalmSized(event, index)) {
            rejectedPointers.add(id)
            onPalmRejected?.invoke()
            return
        }
        onViewportTouch?.invoke()

        if (fingerCanPaint && countActiveFingers(event) == 1) {
            requestUnbufferedDispatch(event)
            when (penAction) {
                PenAction.FILL -> onFillRequested?.invoke(event.getX(index), event.getY(index))
                PenAction.PICK -> onPickRequested?.invoke(event.getX(index), event.getY(index))
                PenAction.PAINT -> beginStroke(event, index, id, handle)
            }
            return
        }

        if (gestureMaxFingers == 0) {
            gestureStartTimeMs = SystemClock.uptimeMillis()
            fingerGestureStartMs = gestureStartTimeMs
            gestureTravel = 0f
        }
        gestureMaxFingers = maxOf(gestureMaxFingers, countActiveFingers(event))
        anchorValid = false
    }

    private fun onPointerMove(event: MotionEvent, handle: Long) {
        if (strokePointerId >= 0) {
            markStylusPresent()
            sendStrokeMove(event, handle)
        } else {
            updateFingerGesture(event, handle)
        }
    }

    private fun onPointerUp(event: MotionEvent, handle: Long) {
        val index = event.actionIndex
        val id = event.getPointerId(index)
        val canceled = isCanceled(event)

        if (id == strokePointerId) {
            if (canceled) {
                // Android decidio a posteriori que era la palma: se revierte.
                NativeBridge.nativeStrokeCancel(handle)
                onPalmRejected?.invoke()
            } else {
                sendStrokeMove(event, handle)
                NativeBridge.nativeStrokeEnd(handle)
                onStrokeFinished?.invoke()
            }
            strokePointerId = -1
            // Un frame extra para que el motor cierre el trazo y grabe el historial.
            renderThread?.requestRender()
        }

        rejectedPointers.remove(id)

        if (event.actionMasked == MotionEvent.ACTION_UP) {
            finishGesture(canceled)
            rejectedPointers.clear()
        } else {
            anchorValid = false
        }
    }

    // -----------------------------------------------------------------------
    // Trazo
    // -----------------------------------------------------------------------
    private fun beginStroke(event: MotionEvent, index: Int, id: Int, handle: Long) {
        strokePointerId = id
        strokeStartTimeMs = event.eventTime
        NativeBridge.nativeStrokeBegin(
            handle,
            event.getX(index),
            event.getY(index),
            event.getPressure(index),
            event.getAxisValue(MotionEvent.AXIS_TILT, index),
            event.getOrientation(index),
            0.0,
        )
    }

    private fun sendStrokeMove(event: MotionEvent, handle: Long) {
        val index = event.findPointerIndex(strokePointerId)
        if (index < 0) return

        var count = 0
        // Los puntos historicos son los que el sensor capto entre frames. Son la
        // diferencia entre una curva suave y una linea quebrada.
        for (h in 0 until event.historySize) {
            if (count >= MAX_POINTS_PER_BATCH) break
            writePoint(
                strokeBuffer, count++,
                event.getHistoricalX(index, h),
                event.getHistoricalY(index, h),
                event.getHistoricalPressure(index, h),
                event.getHistoricalAxisValue(MotionEvent.AXIS_TILT, index, h),
                event.getHistoricalOrientation(index, h),
                (event.getHistoricalEventTime(h) - strokeStartTimeMs).toFloat(),
            )
        }
        if (count < MAX_POINTS_PER_BATCH) {
            writePoint(
                strokeBuffer, count++,
                event.getX(index),
                event.getY(index),
                event.getPressure(index),
                event.getAxisValue(MotionEvent.AXIS_TILT, index),
                event.getOrientation(index),
                (event.eventTime - strokeStartTimeMs).toFloat(),
            )
        }

        NativeBridge.nativeStrokeMoveBatch(handle, strokeBuffer, count)
        sendPrediction(handle)
    }

    private fun sendPrediction(handle: Long) {
        val predicted = predictor?.predict() ?: return
        try {
            val index = predicted.findPointerIndex(strokePointerId)
            if (index < 0) return

            var count = 0
            for (h in 0 until predicted.historySize) {
                if (count >= MAX_POINTS_PER_BATCH) break
                writePoint(
                    predictBuffer, count++,
                    predicted.getHistoricalX(index, h),
                    predicted.getHistoricalY(index, h),
                    predicted.getHistoricalPressure(index, h),
                    predicted.getHistoricalAxisValue(MotionEvent.AXIS_TILT, index, h),
                    predicted.getHistoricalOrientation(index, h),
                    (predicted.getHistoricalEventTime(h) - strokeStartTimeMs).toFloat(),
                )
            }
            if (count < MAX_POINTS_PER_BATCH) {
                writePoint(
                    predictBuffer, count++,
                    predicted.getX(index),
                    predicted.getY(index),
                    predicted.getPressure(index),
                    predicted.getAxisValue(MotionEvent.AXIS_TILT, index),
                    predicted.getOrientation(index),
                    (predicted.eventTime - strokeStartTimeMs).toFloat(),
                )
            }
            if (count > 0) NativeBridge.nativeStrokePredictBatch(handle, predictBuffer, count)
        } finally {
            predicted.recycle()
        }
    }

    // -----------------------------------------------------------------------
    // Gestos de camara
    // -----------------------------------------------------------------------
    private fun updateFingerGesture(event: MotionEvent, handle: Long) {
        if (stylusOnlyMode || isStylusPresent()) {
            anchorValid = false
            return
        }

        var cx = 0f
        var cy = 0f
        var count = 0
        var x0 = 0f; var y0 = 0f; var x1 = 0f; var y1 = 0f

        for (i in 0 until event.pointerCount) {
            if (isPenTool(event.getToolType(i))) continue
            val id = event.getPointerId(i)
            if (id == strokePointerId || id in rejectedPointers) continue
            // El tamano del contacto puede crecer a mitad del gesto: la palma se
            // va apoyando poco a poco.
            if (palmSizeRejection && isPalmSized(event, i)) {
                rejectedPointers.add(id)
                continue
            }
            val x = event.getX(i)
            val y = event.getY(i)
            cx += x
            cy += y
            if (count == 0) { x0 = x; y0 = y } else if (count == 1) { x1 = x; y1 = y }
            count++
        }

        if (count == 0) {
            anchorValid = false
            return
        }
        gestureMaxFingers = maxOf(gestureMaxFingers, count)

        // Ventana de armado: nada se mueve hasta que los contactos llevan un
        // instante en pantalla, para que un apoyo fugaz no desplace la vista.
        if (SystemClock.uptimeMillis() - fingerGestureStartMs < ARM_DELAY_MS) {
            anchorValid = false
            return
        }
        if (count < navigationMinFingers) {
            anchorValid = false
            return
        }

        cx /= count
        cy /= count
        var distance = 0f
        var angle = 0f
        if (count >= 2) {
            val dx = x1 - x0
            val dy = y1 - y0
            distance = hypot(dx, dy)
            angle = atan2(dy, dx)
        }

        // Al anadir o levantar un dedo se reancla sin mover nada: si no, el
        // cambio de centroide da un salton muy desagradable.
        if (!anchorValid || count != anchorCount) {
            anchorValid = true
            anchorCount = count
            anchorCx = cx
            anchorCy = cy
            anchorDistance = distance
            anchorAngle = angle
            return
        }

        val dx = cx - anchorCx
        val dy = cy - anchorCy
        gestureTravel += hypot(dx, dy)

        // Reparto de gestos:
        //   1 dedo  -> orbitar (solo si el usuario baja el minimo a 1)
        //   2 dedos -> orbitar, con pellizco para zoom y giro para rodar
        //   3 dedos -> desplazar
        // Con [twoFingerPan] los dos ultimos se cambian el sitio: dos dedos
        // desplazan (subir y bajar la camara sin darle la vuelta al modelo) y
        // tres orbitan. El pellizco sigue siendo el pellizco en los dos casos.
        val panning = if (twoFingerPan) count == 2 else count >= 3
        if (panning) {
            NativeBridge.nativeCameraGesture(
                handle, NativeBridge.GESTURE_PAN, dx * panSensitivity, dy * panSensitivity,
            )
        } else {
            val perPixel = (2f * Math.PI.toFloat()) / maxOf(width, 1).toFloat()
            val yaw = dx * perPixel * orbitSensitivity * if (invertOrbitX) 1f else -1f
            val pitch = dy * perPixel * orbitSensitivity * if (invertOrbitY) -1f else 1f
            NativeBridge.nativeCameraGesture(handle, NativeBridge.GESTURE_ORBIT, yaw, pitch)
        }

        if (count >= 2 && anchorDistance > 8f && distance > 8f) {
            val ratio = 1f + (distance / anchorDistance - 1f) * zoomSensitivity
            if (ratio > 0.01f) {
                // El centro del pellizco es el centroide de los dedos, que es el
                // punto que el usuario esta mirando: acercar hacia ahi ahorra
                // tener que encuadrar antes de cada zoom.
                if (zoomToPinchCenter) {
                    NativeBridge.nativeCameraZoomAt(handle, ratio, cx, cy)
                } else {
                    NativeBridge.nativeCameraGesture(handle, NativeBridge.GESTURE_ZOOM, ratio, 0f)
                }
            }
            if (twistToRotateView) {
                var delta = angle - anchorAngle
                while (delta > Math.PI) delta -= (2.0 * Math.PI).toFloat()
                while (delta < -Math.PI) delta += (2.0 * Math.PI).toFloat()
                // Zona muerta: sin ella la vista gira sola al hacer pinza.
                if (abs(delta) > TWIST_DEADZONE_RAD) {
                    NativeBridge.nativeCameraGesture(handle, NativeBridge.GESTURE_ROLL, delta, 0f)
                }
            }
        }

        anchorCx = cx
        anchorCy = cy
        anchorDistance = distance
        anchorAngle = angle
    }

    private fun finishGesture(canceled: Boolean) {
        val duration = SystemClock.uptimeMillis() - gestureStartTimeMs
        if (!canceled && gestureTravel < tapSlopPx && duration < TAP_MAX_MS) {
            when (gestureMaxFingers) {
                2 -> onQuickGesture?.invoke(QuickGesture.UNDO)
                3 -> onQuickGesture?.invoke(QuickGesture.REDO)
                4 -> onQuickGesture?.invoke(QuickGesture.TOGGLE_UI)
            }
        }
        resetGesture()
    }

    private fun resetGesture() {
        anchorValid = false
        anchorCount = 0
        gestureMaxFingers = 0
        gestureTravel = 0f
    }

    // -----------------------------------------------------------------------
    // Hover del lapiz: cursor del pincel antes de tocar
    // -----------------------------------------------------------------------
    override fun onHoverEvent(event: MotionEvent): Boolean {
        val handle = engineHandle
        if (handle == 0L) return false
        updateStylusButton(event)

        when (event.actionMasked) {
            MotionEvent.ACTION_HOVER_ENTER, MotionEvent.ACTION_HOVER_MOVE -> {
                if (isPenTool(event.getToolType(0))) {
                    markStylusPresent()
                    NativeBridge.nativeHover(handle, event.x, event.y, 0f)
                    renderThread?.requestRender()
                }
            }
            MotionEvent.ACTION_HOVER_EXIT -> {
                NativeBridge.nativeHoverEnd(handle)
                renderThread?.requestRender()
            }
        }
        return true
    }

    /** Rueda del raton en DeX o con teclado-trackpad: zoom. */
    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        val handle = engineHandle
        if (handle == 0L) return false
        if (event.actionMasked == MotionEvent.ACTION_SCROLL) {
            val scroll = event.getAxisValue(MotionEvent.AXIS_VSCROLL)
            if (scroll != 0f) {
                val ratio = 1f + scroll * 0.12f
                if (zoomToPinchCenter) {
                    NativeBridge.nativeCameraZoomAt(handle, ratio, event.x, event.y)
                } else {
                    NativeBridge.nativeCameraGesture(handle, NativeBridge.GESTURE_ZOOM, ratio, 0f)
                }
                renderThread?.requestRender()
                return true
            }
        }
        return super.onGenericMotionEvent(event)
    }

    // -----------------------------------------------------------------------
    // Utilidades
    // -----------------------------------------------------------------------
    private fun markStylusPresent() {
        stylusPresentUntilMs = SystemClock.uptimeMillis() + STYLUS_PRESENCE_MS
    }

    private fun isStylusPresent(): Boolean =
        strokePointerId >= 0 || SystemClock.uptimeMillis() < stylusPresentUntilMs

    /**
     * Un dedo deja una huella de unos 8-12 mm; el canto de la mano, bastante
     * mas. `touchMajor` viene en pixeles, asi que se convierte a milimetros con
     * la densidad fisica real de la pantalla.
     */
    private fun isPalmSized(event: MotionEvent, index: Int): Boolean {
        val majorPx = event.getTouchMajor(index)
        if (majorPx <= 0f) return false
        val majorMm = majorPx / xdpi * 25.4f
        return majorMm > palmTouchMajorMm
    }

    private fun updateStylusButton(event: MotionEvent) {
        if (!penButtonErases) return
        val mask = MotionEvent.BUTTON_STYLUS_PRIMARY or MotionEvent.BUTTON_SECONDARY
        val down = (event.buttonState and mask) != 0
        if (down != stylusButtonDown) {
            stylusButtonDown = down
            onStylusButtonChanged?.invoke(down)
        }
    }

    private fun countActiveFingers(event: MotionEvent): Int {
        var count = 0
        for (i in 0 until event.pointerCount) {
            if (isPenTool(event.getToolType(i))) continue
            if (event.getPointerId(i) in rejectedPointers) continue
            count++
        }
        return count
    }

    private fun isCanceled(event: MotionEvent): Boolean {
        // FLAG_CANCELED en un POINTER_UP es como Android avisa, a posteriori,
        // de que aquel contacto era la palma. Solo existe desde Android 13.
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            (event.flags and MotionEvent.FLAG_CANCELED) != 0
    }

    private fun isPenTool(toolType: Int): Boolean =
        toolType == MotionEvent.TOOL_TYPE_STYLUS || toolType == MotionEvent.TOOL_TYPE_ERASER

    private fun writePoint(
        buffer: FloatArray,
        slot: Int,
        x: Float,
        y: Float,
        pressure: Float,
        tilt: Float,
        orientation: Float,
        timeMs: Float,
    ) {
        val base = slot * 6
        buffer[base] = x
        buffer[base + 1] = y
        // Algunos digitalizadores dan 0 de presion en el primer evento; un trazo
        // que empieza invisible se percibe como que la app "no responde".
        buffer[base + 2] = if (pressure <= 0f) 0.06f else pressure
        buffer[base + 3] = tilt
        buffer[base + 4] = orientation
        buffer[base + 5] = timeMs
    }

    private companion object {
        const val MAX_POINTS_PER_BATCH = 96
        const val TAP_MAX_MS = 320L
        const val TWIST_DEADZONE_RAD = 0.06f

        /** Cuanto sigue considerandose "presente" el lapiz tras el ultimo evento. */
        const val STYLUS_PRESENCE_MS = 320L

        /** Espera antes de que un contacto pueda mover la camara. */
        const val ARM_DELAY_MS = 70L
    }
}
