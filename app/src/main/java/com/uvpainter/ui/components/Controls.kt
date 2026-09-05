package com.uvpainter.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt

/**
 * Deslizador vertical hecho a mano.
 *
 * Compose no trae uno y rotar el horizontal deja la zona tactil torcida.
 * Este ademas acepta el arrastre desde cualquier punto de la pista, que es como
 * se espera que funcione en una tablet: no hay que acertarle al pomo.
 */
@Composable
fun VerticalSlider(
    value: Float,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    valueRange: ClosedFloatingPointRange<Float> = 0f..1f,
    trackWidth: Int = 30,
    accent: Color = MaterialTheme.colorScheme.primary,
) {
    val span = (valueRange.endInclusive - valueRange.start).takeIf { it > 0f } ?: 1f
    val fraction = ((value - valueRange.start) / span).coerceIn(0f, 1f)

    Box(
        modifier = modifier
            .width(trackWidth.dp)
            .clip(RoundedCornerShape(trackWidth.dp / 2))
            .background(Color(0x66000000))
            .border(1.dp, Color(0x22FFFFFF), RoundedCornerShape(trackWidth.dp / 2))
            .pointerInput(valueRange) {
                fun report(y: Float) {
                    // Arriba = maximo, que es lo intuitivo para tamano y opacidad.
                    val f = 1f - (y / size.height).coerceIn(0f, 1f)
                    onValueChange(valueRange.start + f * span)
                }
                detectTapGestures(
                    onPress = { report(it.y) },
                    onTap = { report(it.y) },
                )
            }
            .pointerInput(valueRange) {
                detectDragGestures { change, _ ->
                    change.consume()
                    val f = 1f - (change.position.y / size.height).coerceIn(0f, 1f)
                    onValueChange(valueRange.start + f * span)
                }
            },
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val filledHeight = size.height * fraction
            drawRect(
                color = accent.copy(alpha = 0.85f),
                topLeft = Offset(0f, size.height - filledHeight),
                size = Size(size.width, filledHeight),
            )
            val knobY = size.height - filledHeight
            drawCircle(
                color = Color.White,
                radius = size.width * 0.34f,
                center = Offset(size.width / 2f, knobY.coerceIn(size.width * 0.4f, size.height - size.width * 0.4f)),
            )
        }
    }
}

@Composable
fun LabeledSlider(
    label: String,
    value: Float,
    onValueChange: (Float) -> Unit,
    modifier: Modifier = Modifier,
    valueRange: ClosedFloatingPointRange<Float> = 0f..1f,
    format: (Float) -> String = { "${(it * 100).roundToInt()}%" },
    /**
     * Con un valor aqui aparecen los botones -/+ a los lados. Un deslizador que
     * recorre de 1 a 300 no permite clavar un numero concreto con el dedo, y hay
     * ajustes (el tamano del pincel, sin ir mas lejos) donde uno arriba o uno
     * abajo se nota.
     */
    step: Float? = null,
) {
    Column(modifier = modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(label, style = MaterialTheme.typography.labelMedium)
            Text(
                format(value),
                style = MaterialTheme.typography.labelMedium,
                fontFamily = FontFamily.Monospace,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            if (step != null) {
                StepButton("−") {
                    onValueChange((value - step).coerceIn(valueRange.start, valueRange.endInclusive))
                }
            }
            Slider(
                value = value,
                onValueChange = onValueChange,
                valueRange = valueRange,
                modifier = Modifier.weight(1f).height(28.dp),
            )
            if (step != null) {
                StepButton("+") {
                    onValueChange((value + step).coerceIn(valueRange.start, valueRange.endInclusive))
                }
            }
        }
    }
}

@Composable
private fun StepButton(label: String, onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .size(26.dp)
            .clip(CircleShape)
            .background(Color(0x22FFFFFF))
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            label,
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

/** Muestra un color sobre cuadros de transparencia. */
@Composable
fun ColorSwatch(
    color: Color,
    modifier: Modifier = Modifier,
    selected: Boolean = false,
    onClick: (() -> Unit)? = null,
) {
    Box(
        modifier = modifier
            .clip(CircleShape)
            .background(
                Brush.linearGradient(
                    0f to Color(0xFF3A3D44),
                    0.5f to Color(0xFF2A2D33),
                    1f to Color(0xFF3A3D44),
                ),
            )
            .background(color, CircleShape)
            .border(
                width = if (selected) 2.5.dp else 1.dp,
                color = if (selected) Color.White else Color(0x55FFFFFF),
                shape = CircleShape,
            )
            .then(
                if (onClick != null) {
                    Modifier.pointerInput(color) { detectTapGestures { onClick() } }
                } else {
                    Modifier
                },
            ),
    )
}

@Composable
fun PanelTitle(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        style = MaterialTheme.typography.titleSmall,
        color = MaterialTheme.colorScheme.onSurface,
        modifier = modifier.padding(bottom = 6.dp),
    )
}

/**
 * Fila de fichas que se parte en varias lineas cuando no cabe.
 *
 * En un [Row] normal, la ultima ficha se lleva todo el aprieto: se queda sin
 * ancho y su texto cae en vertical, una letra por linea, dejando un boquete en
 * el panel. Con cuatro palabras cortas no se nota, pero «Pivoter 90°» al lado de
 * «Y vers le haut» ya no cabe, y la misma fila que en castellano estaba holgada
 * revienta en frances o en aleman. Aqui la ficha que no entra baja a la
 * siguiente linea y se lee entera.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun ChipRow(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    FlowRow(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        content()
    }
}

@Composable
fun ToggleChip(
    label: String,
    selected: Boolean,
    modifier: Modifier = Modifier,
    onClick: () -> Unit,
) {
    val background = if (selected) {
        MaterialTheme.colorScheme.primaryContainer
    } else {
        Color(0x22FFFFFF)
    }
    val content = if (selected) {
        MaterialTheme.colorScheme.onPrimaryContainer
    } else {
        MaterialTheme.colorScheme.onSurfaceVariant
    }
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .background(background)
            .pointerInput(selected) { detectTapGestures { onClick() } }
            .padding(horizontal = 10.dp, vertical = 6.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(label, style = MaterialTheme.typography.labelMedium, color = content, fontSize = 12.sp)
    }
}

@Composable
fun SmallToggleRow(
    label: String,
    checked: Boolean,
    modifier: Modifier = Modifier,
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .pointerInput(checked) { detectTapGestures { onCheckedChange(!checked) } }
            .padding(vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(label, style = MaterialTheme.typography.bodySmall)
        Box(
            modifier = Modifier
                .size(width = 34.dp, height = 20.dp)
                .clip(RoundedCornerShape(10.dp))
                .background(
                    if (checked) MaterialTheme.colorScheme.primary else Color(0x33FFFFFF),
                ),
            contentAlignment = if (checked) Alignment.CenterEnd else Alignment.CenterStart,
        ) {
            Box(
                modifier = Modifier
                    .padding(horizontal = 2.dp)
                    .size(16.dp)
                    .clip(CircleShape)
                    .background(Color.White),
            )
        }
    }
}
