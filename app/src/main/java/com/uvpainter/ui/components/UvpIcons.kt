package com.uvpainter.ui.components

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.path
import androidx.compose.ui.unit.dp

/**
 * Iconos que el juego de Material no trae.
 *
 * La goma es el caso claro: no hay ninguna, y usar la varita mágica
 * (`AutoFixNormal`) para el borrador no le dice nada a nadie. Se dibuja a mano:
 * un bloque inclinado apoyado en una línea, que es como se representa una goma
 * en cualquier editor.
 */
object UvpIcons {

    val Eraser: ImageVector by lazy {
        ImageVector.Builder(
            name = "uvp_eraser",
            defaultWidth = 24.dp,
            defaultHeight = 24.dp,
            viewportWidth = 24f,
            viewportHeight = 24f,
        ).apply {
            // Cuerpo: rectángulo girado 45 grados.
            path(fill = SolidColor(Color.Black)) {
                moveTo(12.8f, 5.3f)
                lineTo(17.7f, 10.2f)
                lineTo(9.2f, 18.7f)
                lineTo(4.3f, 13.8f)
                close()
            }
            // Franja que separa la parte gastada: es lo que hace que se lea como
            // una goma y no como un rombo cualquiera.
            path(
                stroke = SolidColor(Color.Black),
                strokeLineWidth = 1.3f,
            ) {
                moveTo(8.5f, 9.6f)
                lineTo(13.4f, 14.5f)
            }
            // Suelo sobre el que borra.
            path(fill = SolidColor(Color.Black)) {
                moveTo(3f, 19.6f)
                lineTo(21f, 19.6f)
                lineTo(21f, 21.4f)
                lineTo(3f, 21.4f)
                close()
            }
        }.build()
    }
}
