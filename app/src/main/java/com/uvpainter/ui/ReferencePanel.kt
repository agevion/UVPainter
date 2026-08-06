package com.uvpainter.ui

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.DragIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt

/**
 * Panel de imagen de referencia: flotante, arrastrable y con cuentagotas.
 *
 * Se puede mover y escalar sin salir del lienzo, y tocar la imagen toma el color
 * bajo el dedo, que es para lo que sirve de verdad tener la referencia delante.
 */
@Composable
fun ReferencePanel(
    image: ImageBitmap,
    onClose: () -> Unit,
    onPickColor: (Color) -> Unit,
    modifier: Modifier = Modifier,
) {
    var offsetX by remember { mutableFloatStateOf(60f) }
    var offsetY by remember { mutableFloatStateOf(120f) }
    var panelWidth by remember { mutableFloatStateOf(360f) }
    var zoom by remember { mutableFloatStateOf(1f) }
    var imageBoxSize by remember { mutableStateOf(androidx.compose.ui.unit.IntSize.Zero) }
    var pickerEnabled by remember { mutableStateOf(true) }

    val androidBitmap = remember(image) { image.asAndroidBitmap() }

    Column(
        modifier = modifier
            .offset { IntOffset(offsetX.roundToInt(), offsetY.roundToInt()) }
            .width(panelWidth.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(UvpTokens.PanelBackground)
            .border(1.dp, UvpTokens.PanelBorder, RoundedCornerShape(12.dp)),
    ) {
        // Barra de título: es la zona por la que se arrastra el panel.
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(Color(0x22FFFFFF))
                .pointerInput(Unit) {
                    detectDragGestures { change, drag ->
                        change.consume()
                        offsetX += drag.x
                        offsetY += drag.y
                    }
                }
                .padding(horizontal = 8.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    Icons.Filled.DragIndicator,
                    "Mover",
                    modifier = Modifier.size(16.dp),
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    "Referencia",
                    style = MaterialTheme.typography.labelMedium,
                    modifier = Modifier.padding(start = 6.dp),
                )
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    if (pickerEnabled) "Cuentagotas" else "Solo ver",
                    style = MaterialTheme.typography.labelSmall,
                    color = if (pickerEnabled) {
                        MaterialTheme.colorScheme.primary
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant
                    },
                    modifier = Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(Color(0x22FFFFFF))
                        .pointerInput(Unit) {
                            detectTapGestures { pickerEnabled = !pickerEnabled }
                        }
                        .padding(horizontal = 8.dp, vertical = 3.dp),
                )
                Box(
                    modifier = Modifier
                        .padding(start = 6.dp)
                        .size(20.dp)
                        .pointerInput(Unit) { detectTapGestures { onClose() } },
                    contentAlignment = Alignment.Center,
                ) {
                    Icon(Icons.Filled.Close, "Cerrar", modifier = Modifier.size(15.dp))
                }
            }
        }

        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height((panelWidth * 0.75f).dp)
                .onSizeChanged { imageBoxSize = it }
                .pointerInput(Unit) {
                    // Pellizco para acercarse a un detalle de la referencia.
                    detectTransformGestures { _, _, gestureZoom, _ ->
                        zoom = (zoom * gestureZoom).coerceIn(0.5f, 8f)
                    }
                }
                .pointerInput(pickerEnabled, androidBitmap, zoom) {
                    if (!pickerEnabled) return@pointerInput
                    detectTapGestures { position ->
                        val color = sampleBitmap(
                            androidBitmap, position.x, position.y,
                            imageBoxSize.width, imageBoxSize.height, zoom,
                        )
                        if (color != null) onPickColor(color)
                    }
                },
        ) {
            Image(
                bitmap = image,
                contentDescription = "Imagen de referencia",
                contentScale = ContentScale.Fit,
                modifier = Modifier.fillMaxSize().graphicsLayer(scaleX = zoom, scaleY = zoom),
            )
        }

        // Tirador de tamaño en la esquina inferior.
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(18.dp)
                .pointerInput(Unit) {
                    detectDragGestures { change, drag ->
                        change.consume()
                        panelWidth = (panelWidth + drag.x * 0.6f).coerceIn(180f, 720f)
                    }
                },
            contentAlignment = Alignment.CenterEnd,
        ) {
            Text(
                "◢",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(end = 6.dp),
            )
        }
    }
}

/** Traduce un toque sobre el recuadro a un píxel de la imagen. */
private fun sampleBitmap(
    bitmap: Bitmap,
    x: Float,
    y: Float,
    boxWidth: Int,
    boxHeight: Int,
    zoom: Float,
): Color? {
    if (boxWidth <= 0 || boxHeight <= 0 || bitmap.width <= 0) return null

    // ContentScale.Fit centra la imagen y la escala por el lado que más limite.
    val fit = minOf(
        boxWidth.toFloat() / bitmap.width,
        boxHeight.toFloat() / bitmap.height,
    ) * zoom
    val drawnW = bitmap.width * fit
    val drawnH = bitmap.height * fit
    val originX = (boxWidth - drawnW) * 0.5f
    val originY = (boxHeight - drawnH) * 0.5f

    val px = ((x - originX) / fit).roundToInt()
    val py = ((y - originY) / fit).roundToInt()
    if (px < 0 || py < 0 || px >= bitmap.width || py >= bitmap.height) return null

    return Color(bitmap.getPixel(px, py))
}
