package com.uvpainter

import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.uvpainter.state.PainterController
import com.uvpainter.ui.PainterScreen
import com.uvpainter.ui.UVPainterTheme

class MainActivity : ComponentActivity() {

    private lateinit var controller: PainterController

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        // Modo inmersivo: el lienzo es la app. Las barras vuelven deslizando
        // desde el borde y no se quedan fijas robando espacio.
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
        // Dibujar durante horas con el lapiz no deberia apagar la pantalla.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        controller = PainterController(applicationContext)

        // Gancho de pruebas: permite activar el pintado con el dedo desde adb
        //   adb shell am start -n com.uvpainter/.MainActivity --ez finger_paint true
        // para poder validar el trazo sin un lapiz delante.
        if (intent?.getBooleanExtra(EXTRA_FINGER_PAINT, false) == true) {
            controller.updateInput { it.copy(fingerCanPaint = true, navigationMinFingers = 2) }
        }

        setContent {
            UVPainterTheme {
                PainterScreen(
                    controller = controller,
                    defaultModelAsset = DEFAULT_MODEL_ASSET,
                )
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (::controller.isInitialized) controller.destroy()
    }

    private companion object {
        /** Modelo de arranque para poder probar sin importar nada. */
        const val DEFAULT_MODEL_ASSET = "models/Boar.glb"
        const val EXTRA_FINGER_PAINT = "finger_paint"
    }
}
