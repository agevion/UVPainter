package com.uvpainter.ui

import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.graphics.Color

// Paleta oscura y sin saturar: cualquier tinte fuerte en la interfaz falsea
// como se percibe el color que estas pintando.
private val DarkColors = darkColorScheme(
    primary = Color(0xFF7FB2FF),
    onPrimary = Color(0xFF0A1526),
    primaryContainer = Color(0xFF23405F),
    onPrimaryContainer = Color(0xFFD6E6FF),
    secondary = Color(0xFF9AA5B5),
    background = Color(0xFF0D0E11),
    onBackground = Color(0xFFE6E8EC),
    surface = Color(0xFF16181D),
    onSurface = Color(0xFFE6E8EC),
    surfaceVariant = Color(0xFF23262D),
    onSurfaceVariant = Color(0xFFB6BCC7),
    outline = Color(0xFF3A3F49),
    error = Color(0xFFFF8A80),
)

object UvpTokens {
    val PanelBackground = Color(0xEE16181D)
    val PanelBorder = Color(0x33FFFFFF)
    val ToolbarBackground = Color(0xCC12141A)
    val CheckerLight = Color(0xFF3A3D44)
    val CheckerDark = Color(0xFF2A2D33)
}

/**
 * Siempre oscuro, sin mirar el ajuste del sistema: el visor es negro y una
 * interfaz clara al lado falsearia el color que se esta pintando.
 *
 * El [LocalContentColor] se provee a mano porque MaterialTheme no lo toca: sin
 * esto, todo el texto que no vive dentro de un contenedor con color propio se
 * queda con el negro por defecto y no se lee sobre los paneles oscuros.
 */
@Composable
fun UVPainterTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = DarkColors) {
        CompositionLocalProvider(
            LocalContentColor provides DarkColors.onSurface,
            content = content,
        )
    }
}
