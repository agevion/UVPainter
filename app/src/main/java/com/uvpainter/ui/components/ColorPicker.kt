package com.uvpainter.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import com.uvpainter.i18n.LocalStrings
import kotlin.math.roundToInt

private fun Color.toHsv(): Triple<Float, Float, Float> {
    val hsv = FloatArray(3)
    android.graphics.Color.RGBToHSV(
        (red * 255f).roundToInt().coerceIn(0, 255),
        (green * 255f).roundToInt().coerceIn(0, 255),
        (blue * 255f).roundToInt().coerceIn(0, 255),
        hsv,
    )
    return Triple(hsv[0], hsv[1], hsv[2])
}

private fun hsvToColor(hue: Float, saturation: Float, value: Float): Color =
    Color(android.graphics.Color.HSVToColor(floatArrayOf(hue, saturation, value)))

fun Color.toHex(): String = String.format(
    "#%02X%02X%02X",
    (red * 255f).roundToInt().coerceIn(0, 255),
    (green * 255f).roundToInt().coerceIn(0, 255),
    (blue * 255f).roundToInt().coerceIn(0, 255),
)

/**
 * Selector HSV: cuadrado de saturacion/valor mas barra de tono.
 *
 * El tono se guarda aparte del color resultante a proposito: si se recalculara
 * desde el RGB, al llegar a negro o blanco puro el tono se perderia y el
 * cuadrado saltaria a rojo, que es un clasico muy molesto.
 */
@Composable
fun ColorPicker(
    color: Color,
    onColorChange: (Color) -> Unit,
    onColorCommitted: (Color) -> Unit,
    recentColors: List<Color>,
    favoriteColors: List<Color>,
    isFavorite: (Color) -> Boolean,
    onToggleFavorite: (Color) -> Unit,
    modifier: Modifier = Modifier,
) {
    val t = LocalStrings.current
    val (initialHue, initialSat, initialValue) = remember(Unit) { color.toHsv() }
    var hue by remember { mutableFloatStateOf(initialHue) }
    var saturation by remember { mutableFloatStateOf(initialSat) }
    var value by remember { mutableFloatStateOf(initialValue) }

    fun emit() = onColorChange(hsvToColor(hue, saturation, value))
    // El historial solo se toca al soltar: mientras arrastras esto se dispara
    // decenas de veces por segundo y llenaria los recientes de un mismo tono.
    fun commit() = onColorCommitted(hsvToColor(hue, saturation, value))

    Column(modifier = modifier) {
        // Cuadrado de saturacion (X) y luminosidad (Y).
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .aspectRatio(1.25f)
                .clip(RoundedCornerShape(10.dp))
                .pointerInput(Unit) {
                    fun apply(position: Offset) {
                        saturation = (position.x / size.width).coerceIn(0f, 1f)
                        value = 1f - (position.y / size.height).coerceIn(0f, 1f)
                        emit()
                    }
                    detectTapGestures(onPress = { apply(it) }, onTap = { apply(it); commit() })
                }
                .pointerInput(Unit) {
                    detectDragGestures(
                        onDragEnd = { commit() },
                    ) { change, _ ->
                        change.consume()
                        saturation = (change.position.x / size.width).coerceIn(0f, 1f)
                        value = 1f - (change.position.y / size.height).coerceIn(0f, 1f)
                        emit()
                    }
                },
        ) {
            Canvas(modifier = Modifier.fillMaxSize()) {
                drawRect(
                    brush = Brush.horizontalGradient(
                        listOf(Color.White, hsvToColor(hue, 1f, 1f)),
                    ),
                )
                drawRect(brush = Brush.verticalGradient(listOf(Color.Transparent, Color.Black)))

                val cx = saturation * size.width
                val cy = (1f - value) * size.height
                drawCircle(Color.White, radius = 9f, center = Offset(cx, cy))
                drawCircle(Color.Black, radius = 7f, center = Offset(cx, cy))
                drawCircle(
                    hsvToColor(hue, saturation, value),
                    radius = 6f,
                    center = Offset(cx, cy),
                )
            }
        }

        // Barra de tono.
        Box(
            modifier = Modifier
                .padding(top = 10.dp)
                .fillMaxWidth()
                .height(26.dp)
                .clip(RoundedCornerShape(13.dp))
                .pointerInput(Unit) {
                    fun apply(position: Offset) {
                        hue = ((position.x / size.width).coerceIn(0f, 1f)) * 360f
                        emit()
                    }
                    detectTapGestures(onPress = { apply(it) }, onTap = { apply(it); commit() })
                }
                .pointerInput(Unit) {
                    detectDragGestures(
                        onDragEnd = { commit() },
                    ) { change, _ ->
                        change.consume()
                        hue = ((change.position.x / size.width).coerceIn(0f, 1f)) * 360f
                        emit()
                    }
                },
        ) {
            Canvas(modifier = Modifier.fillMaxSize()) {
                drawRect(
                    brush = Brush.horizontalGradient(
                        (0..12).map { hsvToColor(it * 30f, 1f, 1f) },
                    ),
                )
                val x = (hue / 360f) * size.width
                drawCircle(Color.White, radius = size.height * 0.36f, center = Offset(x, size.height / 2f))
                drawCircle(
                    hsvToColor(hue, 1f, 1f),
                    radius = size.height * 0.28f,
                    center = Offset(x, size.height / 2f),
                )
            }
        }

        val current = hsvToColor(hue, saturation, value)

        Row(
            modifier = Modifier.padding(top = 10.dp).fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Box(
                modifier = Modifier
                    .size(width = 62.dp, height = 26.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(current)
                    .border(1.dp, Color(0x44FFFFFF), RoundedCornerShape(6.dp)),
            )
            Text(
                current.toHex(),
                style = MaterialTheme.typography.labelMedium,
                fontFamily = FontFamily.Monospace,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(
                        if (isFavorite(current)) {
                            MaterialTheme.colorScheme.primaryContainer
                        } else {
                            Color(0x22FFFFFF)
                        },
                    )
                    .pointerInput(current.value) {
                        detectTapGestures { onToggleFavorite(current) }
                    }
                    .padding(horizontal = 10.dp, vertical = 5.dp),
            ) {
                Text(
                    if (isFavorite(current)) t.color.favoriteOn else t.color.favoriteOff,
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurface,
                )
            }
        }

        fun applySwatch(swatch: Color) {
            val hsv = swatch.toHsv()
            hue = hsv.first
            saturation = hsv.second
            value = hsv.third
            onColorChange(swatch)
        }

        if (favoriteColors.isNotEmpty()) {
            SwatchRow(t.color.favorites, favoriteColors) { applySwatch(it) }
        }
        if (recentColors.isNotEmpty()) {
            SwatchRow(t.color.recents, recentColors) { applySwatch(it) }
        }
    }
}

@Composable
private fun SwatchRow(title: String, colors: List<Color>, onPick: (Color) -> Unit) {
    Text(
        title,
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.padding(top = 10.dp, bottom = 4.dp),
    )
    LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        items(colors) { swatch ->
            ColorSwatch(
                color = swatch,
                modifier = Modifier.size(28.dp),
                onClick = { onPick(swatch) },
            )
        }
    }
}
