package com.uvpainter.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material.icons.filled.Visibility
import androidx.compose.material.icons.filled.VisibilityOff
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import com.uvpainter.state.BlendMode
import com.uvpainter.state.LayerUi
import com.uvpainter.state.PainterController
import com.uvpainter.state.PenButtonAction
import com.uvpainter.state.ProjectEntry
import com.uvpainter.state.ShapeKind
import com.uvpainter.state.TipShape
import com.uvpainter.state.Tool
import com.uvpainter.state.ViewMode
import java.text.DateFormat
import java.util.Date
import com.uvpainter.ui.components.ColorPicker
import com.uvpainter.ui.components.ColorSwatch
import com.uvpainter.ui.components.LabeledSlider
import com.uvpainter.ui.components.PanelTitle
import com.uvpainter.ui.components.SmallToggleRow
import com.uvpainter.ui.components.ToggleChip
import kotlin.math.roundToInt

@Composable
fun Panel(
    modifier: Modifier = Modifier,
    width: Int = 300,
    content: @Composable ColumnScope.() -> Unit,
) {
    Column(
        modifier = modifier
            .width(width.dp)
            .clip(RoundedCornerShape(14.dp))
            .background(UvpTokens.PanelBackground)
            .border(1.dp, UvpTokens.PanelBorder, RoundedCornerShape(14.dp))
            // Absorbe los toques para que no lleguen al visor que hay debajo.
            .pointerInput(Unit) { detectTapGestures { } }
            .padding(14.dp),
        content = content,
    )
}

// ---------------------------------------------------------------------------
// Pincel
// ---------------------------------------------------------------------------
@Composable
fun BrushPanel(controller: PainterController, modifier: Modifier = Modifier) {
    val brush = controller.brush
    Panel(modifier = modifier, width = 320) {
        Column(modifier = Modifier.heightIn(max = 560.dp).verticalScroll(rememberScrollState())) {
            PanelTitle("Pincel")

            LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                items(controller.brushPresets) { preset ->
                    ToggleChip(
                        label = preset.name,
                        selected = false,
                        onClick = { controller.applyPreset(preset) },
                    )
                }
            }

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            LabeledSlider(
                "Tamaño", brush.radiusPx,
                { controller.updateBrush { b -> b.copy(radiusPx = it) } },
                valueRange = 1f..300f,
                format = { "${it.roundToInt()} px" },
                step = 1f,
            )
            SmallToggleRow("Tamaño fijo al hacer zoom", brush.lockSizeToSurface) { value ->
                controller.updateBrush { b -> b.copy(lockSizeToSurface = value) }
            }
            Text(
                "El número de arriba deja de ser píxeles de pantalla y pasa a " +
                    "ser el tamaño real sobre el modelo: acercar la cámara para " +
                    "afinar el detalle ya no encoge el trazo hasta dejarlo sin " +
                    "fuerza, solo lo ve más grande en pantalla.",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            LabeledSlider(
                "Dureza", brush.hardness,
                { controller.updateBrush { b -> b.copy(hardness = it) } },
            )
            LabeledSlider(
                "Opacidad", brush.opacity,
                { controller.updateBrush { b -> b.copy(opacity = it) } },
            )
            LabeledSlider(
                "Flujo", brush.flow,
                { controller.updateBrush { b -> b.copy(flow = it) } },
            )
            // El regulador de trazo vive en la barra, no aquí: se enciende y se
            // apaga a media lámina, y bajar a un panel para eso rompe el ritmo.
            LabeledSlider(
                "Suavizado fino", brush.smoothing,
                { controller.updateBrush { b -> b.copy(smoothing = it) } },
                valueRange = 0f..0.95f,
            )
            LabeledSlider(
                "Espaciado", brush.spacingPx,
                { controller.updateBrush { b -> b.copy(spacingPx = it) } },
                valueRange = 0.5f..24f,
                format = { "${"%.1f".format(it)} px" },
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle("Punta")

            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                TipShape.entries.forEach { shape ->
                    ToggleChip(shape.label, brush.tipShape == shape) {
                        controller.updateBrush { b -> b.copy(tipShape = shape) }
                    }
                }
            }
            if (brush.tipShape == TipShape.FLAT) {
                LabeledSlider(
                    "Aplanado", brush.tipAspect,
                    { controller.updateBrush { b -> b.copy(tipAspect = it) } },
                    valueRange = 0.05f..1f,
                )
                LabeledSlider(
                    "Ángulo", brush.tipAngle,
                    { controller.updateBrush { b -> b.copy(tipAngle = it) } },
                    valueRange = 0f..3.1416f,
                    format = { "${(it * 180f / 3.1416f).roundToInt()}°" },
                )
                SmallToggleRow("El ángulo sigue al trazo", brush.tipFollowsStroke) { value ->
                    controller.updateBrush { b -> b.copy(tipFollowsStroke = value) }
                }
            }
            if (brush.tipShape != TipShape.SPRAY) {
                LabeledSlider(
                    "Grano", brush.grainAmount,
                    { controller.updateBrush { b -> b.copy(grainAmount = it) } },
                )
            }
            LabeledSlider(
                "Escala del grano", brush.grainScale,
                { controller.updateBrush { b -> b.copy(grainScale = it) } },
                valueRange = 100f..2500f,
                format = { "${it.roundToInt()}" },
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle("Respuesta del S-Pen")

            SmallToggleRow("Presión → tamaño", brush.pressureAffectsSize) { value ->
                controller.updateBrush { b -> b.copy(pressureAffectsSize = value) }
            }
            SmallToggleRow("Presión → opacidad", brush.pressureAffectsOpacity) { value ->
                controller.updateBrush { b -> b.copy(pressureAffectsOpacity = value) }
            }
            LabeledSlider(
                "Sensibilidad", brush.pressureGain,
                { controller.updateBrush { b -> b.copy(pressureGain = it) } },
                valueRange = 0.6f..3f,
                format = { "%.2f×".format(it) },
            )
            Text(
                "Sube la sensibilidad si tienes que apretar para que pinte a tope.",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            LabeledSlider(
                "Curva de presión", brush.pressureCurve,
                { controller.updateBrush { b -> b.copy(pressureCurve = it) } },
                valueRange = 0.25f..2.5f,
                format = { "%.2f".format(it) },
            )
            LabeledSlider(
                "Tamaño mínimo", brush.pressureSizeFloor,
                { controller.updateBrush { b -> b.copy(pressureSizeFloor = it) } },
            )
            LabeledSlider(
                "Inclinación → tamaño", brush.tiltSize,
                { controller.updateBrush { b -> b.copy(tiltSize = it) } },
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle("Proyección")

            SmallToggleRow("Solo la isla UV donde empiezo", brush.restrictToIsland) { value ->
                controller.updateBrush { b -> b.copy(restrictToIsland = value) }
            }
            Text(
                "Con esto puedes pintar pegado al borde sin miedo: el trazo no " +
                    "salta a la zona de al lado aunque en pantalla estén juntas.",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            SmallToggleRow("No pintar lo tapado", brush.depthTest) { value ->
                controller.updateBrush { b -> b.copy(depthTest = value) }
            }
            SmallToggleRow("No pintar caras traseras", brush.backfaceCull) { value ->
                controller.updateBrush { b -> b.copy(backfaceCull = value) }
            }
            SmallToggleRow("Bloquear alfa", brush.alphaLock) { value ->
                controller.updateBrush { b -> b.copy(alphaLock = value) }
            }
            LabeledSlider(
                "Desvanecido en bordes", brush.facingFull,
                { controller.updateBrush { b -> b.copy(facingFull = it) } },
                valueRange = 0.05f..0.9f,
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle("Bote de pintura")

            SmallToggleRow("Rellenar solo el área cerrada", brush.fillClosedArea) { value ->
                controller.updateBrush { b -> b.copy(fillClosedArea = value) }
            }
            Text(
                if (brush.fillClosedArea) {
                    "El bote se extiende desde donde tocas y se para donde cambia " +
                        "el color, como en un editor de fotos. Sirve para rellenar " +
                        "una figura dibujada a pulso, sin tener que delimitarla antes."
                } else {
                    "El bote llena la isla UV entera bajo el dedo."
                },
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            if (brush.fillClosedArea) {
                LabeledSlider(
                    "Tolerancia", brush.fillTolerance,
                    { controller.updateBrush { b -> b.copy(fillTolerance = it) } },
                    valueRange = 0f..0.6f,
                    format = { "${(it * 100).roundToInt()}%" },
                )
                Text(
                    "Cuánto puede variar el color y seguir contando como la misma " +
                        "zona. Si el relleno se escapa por un hueco del contorno, " +
                        "baja esto; si se queda corto contra un borde suavizado, súbelo.",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------
@Composable
fun ColorPanel(controller: PainterController, modifier: Modifier = Modifier) {
    Panel(modifier = modifier, width = 300) {
        PanelTitle("Color")
        ColorPicker(
            color = controller.brush.color,
            onColorChange = { controller.setColor(it) },
            onColorCommitted = { controller.commitColor(it) },
            recentColors = controller.recentColors,
            favoriteColors = controller.favoriteColors,
            isFavorite = { controller.isFavorite(it) },
            onToggleFavorite = { controller.toggleFavorite(it) },
        )
        Text(
            "El color solo se ve exacto en la vista «Plano». PBR y matcap pasan " +
                "por iluminación y tonemap, así que desplazan el tono.",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 10.dp),
        )
    }
}

// ---------------------------------------------------------------------------
// Formas geometricas y limites dibujados a mano
// ---------------------------------------------------------------------------
@Composable
fun ShapePanel(controller: PainterController, modifier: Modifier = Modifier) {
    val brush = controller.brush
    Panel(modifier = modifier, width = 320) {
        PanelTitle("Formas")
        Text(
            "Apoya el lápiz donde empieza la figura y arrastra: la ves en vivo " +
                "y se fija al levantar.",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(bottom = 6.dp),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            ShapeKind.entries.take(3).forEach { kind ->
                ToggleChip(kind.label, brush.shape == kind) {
                    controller.updateBrush { b -> b.copy(shape = kind) }
                    if (kind != ShapeKind.NONE) controller.changeTool(Tool.SHAPE)
                }
            }
        }
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(top = 6.dp),
        ) {
            ShapeKind.entries.drop(3).forEach { kind ->
                ToggleChip(kind.label, brush.shape == kind) {
                    controller.updateBrush { b -> b.copy(shape = kind) }
                    controller.changeTool(Tool.SHAPE)
                }
            }
        }

        if (brush.shape == ShapeKind.POLYGON) {
            LabeledSlider(
                "Lados", brush.polygonSides.toFloat(),
                { controller.updateBrush { b -> b.copy(polygonSides = it.roundToInt()) } },
                valueRange = 3f..24f,
                format = { "${it.roundToInt()}" },
            )
        }
        if (brush.shape == ShapeKind.RECTANGLE || brush.shape == ShapeKind.ELLIPSE) {
            SmallToggleRow("Dibujar desde el centro", brush.shapeFromCenter) { value ->
                controller.updateBrush { b -> b.copy(shapeFromCenter = value) }
            }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 12.dp), color = Color(0x22FFFFFF))
        PanelTitle("Límites de pintura")
        Text(
            "Para modelos con las UVs mal cortadas: traza a mano una frontera y " +
                "el pincel dejará de cruzarla. Si empiezas un trazo a un lado, " +
                "no pintará al otro aunque el pincel lo solape.\n\n" +
                "No hace falta cerrar el área: una línea suelta también frena el " +
                "pincel en todo su recorrido.",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(top = 8.dp),
        ) {
            ToggleChip("Trazar límite", controller.tool == Tool.BOUNDARY) {
                controller.changeTool(
                    if (controller.tool == Tool.BOUNDARY) Tool.BRUSH else Tool.BOUNDARY,
                )
            }
            ToggleChip("Borrar límites", false) { controller.clearBoundary() }
        }
        SmallToggleRow("Respetar límites al pintar", brush.restrictToRegion) { value ->
            controller.updateBrush { b -> b.copy(restrictToRegion = value) }
        }
        Text(
            if (controller.hasBoundary) {
                "Hay límites activos. El bote de pintura solo respeta los que " +
                    "cierran un área; el pincel respeta también los abiertos."
            } else {
                "Todavía no hay ningún límite trazado."
            },
            style = MaterialTheme.typography.labelSmall,
            color = if (controller.hasBoundary) {
                MaterialTheme.colorScheme.primary
            } else {
                MaterialTheme.colorScheme.onSurfaceVariant
            },
            modifier = Modifier.padding(top = 4.dp),
        )
        Text(
            "El límite se traza más grueso que el pincel con el que luego pintas; " +
                "con el regularizador subido sale recto sin pulso.",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 4.dp),
        )
    }
}

// ---------------------------------------------------------------------------
// Entrada: rechazo de palma y gestos
// ---------------------------------------------------------------------------
@Composable
fun InputPanel(controller: PainterController, modifier: Modifier = Modifier) {
    val input = controller.input
    Panel(modifier = modifier, width = 330) {
        Column(modifier = Modifier.heightIn(max = 560.dp).verticalScroll(rememberScrollState())) {
            PanelTitle("Lápiz y rechazo de palma")

            Text(
                "Dedos necesarios para mover la vista",
                style = MaterialTheme.typography.labelMedium,
            )
            Row(
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                modifier = Modifier.padding(top = 4.dp),
            ) {
                ToggleChip("1 dedo", input.navigationMinFingers == 1) {
                    controller.updateInput { it.copy(navigationMinFingers = 1) }
                }
                ToggleChip("2 dedos", input.navigationMinFingers == 2) {
                    controller.updateInput { it.copy(navigationMinFingers = 2) }
                }
            }
            Text(
                "Con dos dedos, una palma apoyada no mueve nada: una mano que " +
                    "descansa deja una mancha, no dos contactos deliberados.",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 2.dp),
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            SmallToggleRow("Descartar contactos anchos", input.palmSizeRejection) { value ->
                controller.updateInput { it.copy(palmSizeRejection = value) }
            }
            LabeledSlider(
                "Umbral de palma", input.palmTouchMajorMm,
                { controller.updateInput { s -> s.copy(palmTouchMajorMm = it) } },
                valueRange = 8f..30f,
                format = { "${it.roundToInt()} mm" },
            )
            Text(
                "Un dedo deja una huella de 8–12 mm; el canto de la mano, bastante " +
                    "más. Baja el umbral si aún se te cuela la palma.",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            SmallToggleRow("Modo solo lápiz (ignora el tacto)", input.stylusOnlyMode) { value ->
                controller.updateInput { it.copy(stylusOnlyMode = value) }
            }
            SmallToggleRow("El dedo también pinta", input.fingerCanPaint) { value ->
                controller.updateInput { it.copy(fingerCanPaint = value) }
            }
            SmallToggleRow("Girar la vista con dos dedos", input.twistToRotateView) { value ->
                controller.updateInput { it.copy(twistToRotateView = value) }
            }
            LabeledSlider(
                "Sensibilidad al orbitar", input.orbitSensitivity,
                { controller.updateInput { s -> s.copy(orbitSensitivity = it) } },
                valueRange = 0.25f..2.5f,
                format = { "%.2f×".format(it) },
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle("Botón del S-Pen")
            Text(
                "El lápiz solo tiene un botón, así que elige para qué lo quieres. " +
                    "Las de «mientras lo mantengo» vuelven solas al soltarlo.",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(bottom = 6.dp),
            )
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                PenButtonAction.entries.forEach { action ->
                    ToggleChip(
                        label = action.label,
                        selected = input.penButtonAction == action,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        controller.updateInput { it.copy(penButtonAction = action) }
                    }
                }
            }

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle("Gestos")
            Text(
                "2 dedos: orbitar, pellizcar para zoom y girar\n" +
                    "3 dedos: desplazar\n" +
                    "Toque con 2 dedos: deshacer\n" +
                    "Toque con 3 dedos: rehacer\n" +
                    "Toque con 4 dedos: ocultar la interfaz",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

// ---------------------------------------------------------------------------
// Capas
// ---------------------------------------------------------------------------
@Composable
fun LayersPanel(controller: PainterController, modifier: Modifier = Modifier) {
    Panel(modifier = modifier, width = 330) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            PanelTitle("Capas")
            Row {
                IconButton(onClick = { controller.addLayer() }, modifier = Modifier.size(30.dp)) {
                    Icon(Icons.Filled.Add, "Añadir capa", modifier = Modifier.size(18.dp))
                }
                IconButton(
                    onClick = { controller.duplicateLayer(controller.activeLayer) },
                    modifier = Modifier.size(30.dp),
                ) {
                    Icon(Icons.Filled.ContentCopy, "Duplicar", modifier = Modifier.size(16.dp))
                }
                IconButton(
                    onClick = { controller.removeLayer(controller.activeLayer) },
                    modifier = Modifier.size(30.dp),
                ) {
                    Icon(Icons.Filled.Delete, "Eliminar", modifier = Modifier.size(18.dp))
                }
            }
        }

        Text(
            "Se pinta siempre en la capa marcada. Toca una para activarla.",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(bottom = 6.dp),
        )

        // Se listan de arriba abajo, que es como se leen en cualquier editor:
        // la ultima capa de la pila arriba del todo.
        val ordered = controller.layers.reversed()
        LazyColumn(
            modifier = Modifier.heightIn(max = 420.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            itemsIndexed(ordered) { _, layer ->
                LayerRow(
                    layer = layer,
                    isActive = layer.index == controller.activeLayer,
                    canMoveUp = layer.index < controller.layers.lastIndex,
                    canMoveDown = layer.index > 0,
                    onSelect = { controller.selectLayer(layer.index) },
                    onUpdate = { controller.updateLayer(it) },
                    onMove = { delta -> controller.moveLayer(layer.index, layer.index + delta) },
                    onClear = { controller.clearLayer(layer.index) },
                )
            }
        }
    }
}

@Composable
private fun LayerRow(
    layer: LayerUi,
    isActive: Boolean,
    canMoveUp: Boolean,
    canMoveDown: Boolean,
    onSelect: () -> Unit,
    onUpdate: (LayerUi) -> Unit,
    onMove: (Int) -> Unit,
    onClear: () -> Unit,
) {
    var blendMenuOpen by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(if (isActive) Color(0x332F80FF) else Color(0x14FFFFFF))
            .border(
                1.dp,
                if (isActive) MaterialTheme.colorScheme.primary else Color(0x1AFFFFFF),
                RoundedCornerShape(8.dp),
            )
            .pointerInput(layer.index) { detectTapGestures { onSelect() } }
            .padding(horizontal = 8.dp, vertical = 6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            // La miniatura es lo que convierte la lista de capas en algo
            // legible: sin ella son cuatro nombres iguales.
            Box(
                modifier = Modifier
                    .size(34.dp)
                    .clip(RoundedCornerShape(5.dp))
                    .background(UvpTokens.CheckerDark)
                    .border(1.dp, Color(0x33FFFFFF), RoundedCornerShape(5.dp)),
            ) {
                val thumb = layer.thumbnail
                if (thumb != null) {
                    androidx.compose.foundation.Image(
                        bitmap = thumb,
                        contentDescription = null,
                        modifier = Modifier.fillMaxSize(),
                    )
                }
            }
            IconButton(
                onClick = { onUpdate(layer.copy(visible = !layer.visible)) },
                modifier = Modifier.size(26.dp),
            ) {
                Icon(
                    if (layer.visible) Icons.Filled.Visibility else Icons.Filled.VisibilityOff,
                    contentDescription = "Visibilidad",
                    modifier = Modifier.size(16.dp),
                    tint = if (layer.visible) {
                        MaterialTheme.colorScheme.onSurface
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f)
                    },
                )
            }
            Text(
                layer.name,
                style = MaterialTheme.typography.bodySmall,
                modifier = Modifier.weight(1f).padding(start = 4.dp),
            )
            IconButton(
                onClick = { if (canMoveUp) onMove(1) },
                modifier = Modifier.size(24.dp),
                enabled = canMoveUp,
            ) {
                Icon(Icons.Filled.KeyboardArrowUp, "Subir", modifier = Modifier.size(16.dp))
            }
            IconButton(
                onClick = { if (canMoveDown) onMove(-1) },
                modifier = Modifier.size(24.dp),
                enabled = canMoveDown,
            ) {
                Icon(Icons.Filled.KeyboardArrowDown, "Bajar", modifier = Modifier.size(16.dp))
            }
        }

        if (isActive) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Box {
                    ToggleChip(
                        label = layer.blend.label,
                        selected = layer.blend != BlendMode.NORMAL,
                        onClick = { blendMenuOpen = true },
                    )
                    DropdownMenu(
                        expanded = blendMenuOpen,
                        onDismissRequest = { blendMenuOpen = false },
                    ) {
                        BlendMode.entries.forEach { mode ->
                            DropdownMenuItem(
                                text = { Text(mode.label) },
                                onClick = {
                                    blendMenuOpen = false
                                    onUpdate(layer.copy(blend = mode))
                                },
                            )
                        }
                    }
                }
                Text(
                    "${(layer.opacity * 100).roundToInt()}%",
                    style = MaterialTheme.typography.labelSmall,
                    fontFamily = FontFamily.Monospace,
                    modifier = Modifier.padding(start = 8.dp),
                )
            }
            LabeledSlider(
                "Opacidad", layer.opacity,
                { onUpdate(layer.copy(opacity = it)) },
                modifier = Modifier.padding(top = 2.dp),
            )
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                ToggleChip("Alfa", layer.alphaLock) { onUpdate(layer.copy(alphaLock = !layer.alphaLock)) }
                ToggleChip("Recorte", layer.clipToBelow) { onUpdate(layer.copy(clipToBelow = !layer.clipToBelow)) }
                ToggleChip("Bloq.", layer.locked) { onUpdate(layer.copy(locked = !layer.locked)) }
                ToggleChip("Vaciar", false) { onClear() }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Visor
// ---------------------------------------------------------------------------
@Composable
fun ViewPanel(controller: PainterController, modifier: Modifier = Modifier) {
    val viewport = controller.viewport
    Panel(modifier = modifier, width = 300) {
        PanelTitle("Vista")
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), modifier = Modifier.fillMaxWidth()) {
            ViewMode.entries.take(2).forEach { mode ->
                ToggleChip(mode.label, viewport.mode == mode) {
                    controller.updateViewport { it.copy(mode = mode) }
                }
            }
        }
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.fillMaxWidth().padding(top = 6.dp),
        ) {
            ViewMode.entries.drop(2).forEach { mode ->
                ToggleChip(mode.label, viewport.mode == mode) {
                    controller.updateViewport { it.copy(mode = mode) }
                }
            }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        SmallToggleRow("Costuras UV (naranja)", viewport.seams) { value ->
            controller.updateViewport { s -> s.copy(seams = value) }
        }
        SmallToggleRow("Malla superpuesta", viewport.wireframe) { value ->
            controller.updateViewport { s -> s.copy(wireframe = value) }
        }
        SmallToggleRow("Ocultar caras traseras", viewport.backfaceCull) { value ->
            controller.updateViewport { s -> s.copy(backfaceCull = value) }
        }
        SmallToggleRow("Marcar zonas sin pintar", viewport.unpaintedTint) { value ->
            controller.updateViewport { s -> s.copy(unpaintedTint = value) }
        }

        if (viewport.mode == ViewMode.UNLIT) {
            LabeledSlider(
                "Volumen en vista plana", viewport.unlitShading,
                { controller.updateViewport { s -> s.copy(unlitShading = it) } },
            )
            Text(
                "Oscurece por igual los tres canales, así que da forma sin " +
                    "cambiar el tono. A 0 el color es exacto al del selector.",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        if (viewport.mode == ViewMode.PBR) {
            LabeledSlider("Rugosidad", viewport.roughness, {
                controller.updateViewport { s -> s.copy(roughness = it) }
            })
            LabeledSlider("Metalicidad", viewport.metallic, {
                controller.updateViewport { s -> s.copy(metallic = it) }
            })
        }
        if (viewport.mode == ViewMode.UV_CHECKER) {
            LabeledSlider(
                "Densidad de la cuadrícula", viewport.uvCheckerDensity,
                { controller.updateViewport { s -> s.copy(uvCheckerDensity = it) } },
                valueRange = 4f..128f,
                format = { "${it.roundToInt()}" },
            )
        }

        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(top = 8.dp),
        ) {
            ToggleChip("Encuadrar modelo", false) { controller.resetView() }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        SmallToggleRow("Mostrar FPS", controller.showFps) { value ->
            controller.setFpsVisible(value)
        }
        Text(
            "Mientras está activo, la app redibuja en bucle en vez de bajo " +
                "demanda, para que el número sea real también en reposo. Se " +
                "apaga entero al quitar el interruptor.",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

// ---------------------------------------------------------------------------
// Menu / documento
// ---------------------------------------------------------------------------
@Composable
fun DocumentPanel(
    controller: PainterController,
    onImportModel: () -> Unit,
    onImportImage: () -> Unit,
    modifier: Modifier = Modifier,
    onPickReference: (() -> Unit)? = null,
) {
    val info = controller.meshInfo
    Panel(modifier = modifier, width = 320) {
        PanelTitle("Documento")

        Text(
            if (info.loaded) {
                "${info.name.ifBlank { "Modelo" }}\n" +
                    "${info.triangles} triángulos · ${info.vertices} vértices · " +
                    "${info.islands} islas UV" +
                    if (info.submeshes > 1) " · ${info.submeshes} materiales" else ""
            } else {
                "Ningún modelo cargado"
            },
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        if (info.uvsOutside01) {
            Text(
                "Aviso: hay UVs fuera del rango 0-1. Puede ser un modelo UDIM; " +
                    "por ahora solo se pinta el primer tile.",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.error,
                modifier = Modifier.padding(top = 4.dp),
            )
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        Text("Orientación del modelo", style = MaterialTheme.typography.labelMedium)
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(top = 4.dp),
        ) {
            ToggleChip("Y arriba", controller.upAxis == 0) {
                controller.setOrientation(newUpAxis = 0)
            }
            ToggleChip("Z arriba", controller.upAxis == 1) {
                controller.setOrientation(newUpAxis = 1)
            }
            ToggleChip("Invertir", controller.flipUp) {
                controller.setOrientation(newFlipUp = !controller.flipUp)
            }
            ToggleChip("Girar 90°", false) {
                controller.setOrientation(newQuarterTurns = controller.quarterTurns + 1)
            }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        Text("Resolución del atlas", style = MaterialTheme.typography.labelMedium)
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(top = 4.dp),
        ) {
            listOf(1024, 2048, 4096).forEach { resolution ->
                ToggleChip(
                    "$resolution",
                    controller.documentResolution == resolution,
                ) { controller.changeDocumentResolution(resolution) }
            }
        }
        Text(
            "Cambiar la resolución reinicia las capas.",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 2.dp),
        )

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            ToggleChip("Importar modelo (.glb / .obj)", false) { onImportModel() }
            ToggleChip("Importar imagen a la capa activa", false) { onImportImage() }
            if (onPickReference != null) {
                ToggleChip("Imagen de referencia", false) { onPickReference() }
            }
            ToggleChip("Exportar PNG a Descargas", false) { controller.exportTexture(true) }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        Text("Barra de herramientas", style = MaterialTheme.typography.labelMedium)
        Text(
            "Se arrastra por el tirador de la izquierda y se agranda o encoge " +
                "pellizcando sobre él.",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 2.dp, bottom = 6.dp),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            ToggleChip("Devolverla a su sitio", false) { controller.resetToolbarPlacement() }
            Text(
                "${(controller.toolbarScale * 100).roundToInt()}%",
                style = MaterialTheme.typography.labelSmall,
                fontFamily = FontFamily.Monospace,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 6.dp),
            )
        }

        Text(
            "Historial: ${controller.historyMemoryMb} MB",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 10.dp),
        )
    }
}

// ---------------------------------------------------------------------------
// Proyectos
// ---------------------------------------------------------------------------
/**
 * Guardar y abrir trabajo. El .uvp lleva el modelo, las capas con sus píxeles y
 * los límites trazados, así que cerrar la app ya no obliga a empezar de cero ni
 * a acordarse de exportar el PNG antes de salir.
 */
@Composable
fun ProjectsPanel(controller: PainterController, modifier: Modifier = Modifier) {
    // El nombre sigue al proyecto abierto: guardar encima es lo más habitual.
    val openName = controller.currentProjectName
    var name by remember(openName) { mutableStateOf(openName.orEmpty()) }

    Panel(modifier = modifier, width = 340) {
        PanelTitle("Proyectos")
        Text(
            "Al salir de la app se guarda sola la sesión y se recupera al " +
                "volver. Guarda con nombre para tener varios trabajos a la vez.",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(bottom = 8.dp),
        )

        OutlinedTextField(
            value = name,
            onValueChange = { name = it },
            label = { Text("Nombre del proyecto") },
            singleLine = true,
            textStyle = MaterialTheme.typography.bodySmall,
            modifier = Modifier.fillMaxWidth(),
        )

        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(top = 8.dp),
        ) {
            ToggleChip("Guardar", false) {
                controller.saveProjectAs(name)
            }
            ToggleChip("Proyecto nuevo", false) { controller.newProject() }
            ToggleChip("Actualizar lista", false) { controller.refreshProjects() }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        SmallToggleRow("Autoguardado", controller.autoSaveEnabled) { value ->
            controller.setAutoSave(enabled = value)
        }
        Text(
            if (openName != null) {
                "Cada cierto tiempo actualiza «$openName» y deja el mismo punto " +
                    "de control en la sesión, que es lo que se recupera al abrir."
            } else {
                "Escribe un punto de control en la sesión cada cierto tiempo, que " +
                    "es lo que se recupera al abrir la app. Guarda con nombre y " +
                    "pasará a mantener ese proyecto al día."
            },
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        if (controller.autoSaveEnabled) {
            Row(
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                modifier = Modifier.padding(top = 6.dp),
            ) {
                listOf(1, 3, 5, 10, 15).forEach { minutes ->
                    ToggleChip("$minutes min", controller.autoSaveMinutes == minutes) {
                        controller.setAutoSave(minutes = minutes)
                    }
                }
            }
            Text(
                if (controller.lastAutoSaveMs > 0L) {
                    "Último punto de control a las " +
                        DateFormat.getTimeInstance(DateFormat.MEDIUM)
                            .format(Date(controller.lastAutoSaveMs))
                } else {
                    "Todavía sin puntos de control en esta sesión."
                },
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(top = 4.dp),
            )
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        if (controller.projects.isEmpty()) {
            Text(
                "Todavía no hay proyectos guardados.",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        } else {
            LazyColumn(
                modifier = Modifier.heightIn(max = 320.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                items(controller.projects) { project ->
                    ProjectRow(
                        project = project,
                        isOpen = project.path == controller.currentProjectPath,
                        onOpen = { controller.openProject(project.path) },
                        onDelete = { controller.deleteProject(project.path) },
                    )
                }
            }
        }

        Text(
            "Se guardan en ${controller.projectsDir.absolutePath}",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 8.dp),
        )
    }
}

@Composable
private fun ProjectRow(
    project: ProjectEntry,
    isOpen: Boolean,
    onOpen: () -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(if (isOpen) Color(0x332F80FF) else Color(0x14FFFFFF))
            .padding(horizontal = 8.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                if (isOpen) "${project.name} · abierto" else project.name,
                style = MaterialTheme.typography.bodySmall,
            )
            Text(
                formatSize(project.sizeBytes) + " · " +
                    DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT)
                        .format(Date(project.modifiedMs)),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        ToggleChip("Abrir", false) { onOpen() }
        IconButton(onClick = onDelete, modifier = Modifier.size(30.dp)) {
            Icon(Icons.Filled.Delete, "Borrar proyecto", modifier = Modifier.size(16.dp))
        }
    }
}

/** Un proyecto de 190 KB no es «0 MB»: por debajo del mega se cuenta en KB. */
private fun formatSize(bytes: Long): String =
    if (bytes < 1024L * 1024L) "${bytes / 1024} KB" else "%.1f MB".format(bytes / 1024.0 / 1024.0)

// ---------------------------------------------------------------------------
// Pincel reducido para el modo pantalla completa
// ---------------------------------------------------------------------------
/**
 * Lo justo para no tener que salir de pantalla completa: tamaño, opacidad,
 * dureza y la punta, que es lo que de verdad cambia el trazo.
 */
@Composable
fun QuickBrushPanel(controller: PainterController, modifier: Modifier = Modifier) {
    val brush = controller.brush
    Panel(modifier = modifier, width = 270) {
        PanelTitle("Pincel")
        LabeledSlider(
            "Tamaño", brush.radiusPx,
            { controller.updateBrush { b -> b.copy(radiusPx = it) } },
            valueRange = 1f..300f,
            format = { "${it.roundToInt()} px" },
            step = 1f,
        )
        LabeledSlider(
            "Opacidad", brush.opacity,
            { controller.updateBrush { b -> b.copy(opacity = it) } },
        )
        LabeledSlider(
            "Dureza", brush.hardness,
            { controller.updateBrush { b -> b.copy(hardness = it) } },
        )

        Text(
            "Punta",
            style = MaterialTheme.typography.labelMedium,
            modifier = Modifier.padding(top = 6.dp, bottom = 4.dp),
        )
        // En fila desplazable: las cuatro puntas no caben de una vez en un panel
        // estrecho, y en un Row normal la última se aplasta hasta ser ilegible.
        LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            items(TipShape.entries) { shape ->
                ToggleChip(shape.label, brush.tipShape == shape) {
                    controller.updateBrush { b -> b.copy(tipShape = shape) }
                }
            }
        }

        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(top = 8.dp),
        ) {
            ToggleChip("Pincel", controller.tool == Tool.BRUSH) {
                controller.changeTool(Tool.BRUSH)
            }
            ToggleChip("Borrador", controller.tool == Tool.ERASER) {
                controller.changeTool(Tool.ERASER)
            }
        }

        LazyRow(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(top = 8.dp),
        ) {
            items(controller.brushPresets) { preset ->
                ToggleChip(preset.name, false) { controller.applyPreset(preset) }
            }
        }
    }
}
