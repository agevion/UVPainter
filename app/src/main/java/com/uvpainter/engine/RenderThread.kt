package com.uvpainter.engine

import android.os.Handler
import android.os.HandlerThread
import android.os.Process
import java.util.concurrent.CountDownLatch
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Hilo propietario del contexto EGL. Todo el trabajo de GL pasa por aqui.
 *
 * El dibujado se pide bajo demanda ([requestRender]) y se deduplica: si llegan
 * 200 eventos del lapiz entre dos vsyncs no se encolan 200 frames, sino uno.
 * Los eventos no se pierden: viven en la cola nativa y se vacian todos juntos
 * al principio del frame.
 */
class RenderThread(private val handle: Long) {

    private val thread = HandlerThread("UVPainterRender", Process.THREAD_PRIORITY_DISPLAY)
    private lateinit var handler: Handler
    private val framePending = AtomicBoolean(false)
    @Volatile private var running = false

    /**
     * Se llama con los fotogramas por segundo medidos, desde el hilo de render.
     * En null no se mide nada: el contador solo interesa con el panel abierto.
     */
    @Volatile var onFps: ((Float) -> Unit)? = null

    private val continuous = AtomicBoolean(false)
    private var frameCount = 0
    private var windowStartNs = 0L

    private val drawRunnable = Runnable {
        framePending.set(false)
        NativeBridge.nativeDrawFrame(handle)
        measureFps()
        // El bucle continuo se reencola desde aqui, ya dibujado el frame, en vez
        // de con un temporizador: asi el ritmo lo marca lo que tarda la GPU y
        // nunca se apilan frames pendientes.
        if (continuous.get() && running) requestRender()
    }

    private fun measureFps() {
        val report = onFps
        if (report == null) {
            windowStartNs = 0L
            frameCount = 0
            return
        }
        val now = System.nanoTime()
        if (windowStartNs == 0L) {
            windowStartNs = now
            frameCount = 0
            return
        }
        frameCount++
        val elapsed = now - windowStartNs
        // Ventana de medio segundo: mas corta y el numero baila tanto que no se
        // puede leer.
        if (elapsed >= 500_000_000L) {
            report(frameCount * 1_000_000_000f / elapsed)
            windowStartNs = now
            frameCount = 0
        }
    }

    /**
     * Redibuja sin parar. Solo para el contador de fps: en reposo la app dibuja
     * bajo demanda justamente para no gastar bateria.
     */
    fun setContinuousRendering(value: Boolean) {
        if (!continuous.compareAndSet(!value, value)) return
        if (value) requestRender()
    }

    fun start() {
        if (running) return
        thread.start()
        handler = Handler(thread.looper)
        running = true
    }

    /** Encola trabajo de GL. Se ignora si el hilo ya no esta vivo. */
    fun post(block: () -> Unit) {
        if (!running) return
        handler.post(block)
    }

    /** Encola trabajo y espera a que termine. No llamar desde el propio hilo. */
    fun postAndWait(timeoutMs: Long = 5_000, block: () -> Unit) {
        if (!running) return
        val latch = CountDownLatch(1)
        handler.post {
            try {
                block()
            } finally {
                latch.countDown()
            }
        }
        latch.await(timeoutMs, java.util.concurrent.TimeUnit.MILLISECONDS)
    }

    /** Pide un frame. Varias llamadas seguidas se funden en una sola. */
    fun requestRender() {
        if (!running) return
        if (framePending.compareAndSet(false, true)) {
            handler.post(drawRunnable)
        }
    }

    fun shutdown(onQuit: () -> Unit) {
        if (!running) return
        running = false
        handler.removeCallbacksAndMessages(null)
        handler.post {
            onQuit()
            thread.quitSafely()
        }
    }
}
