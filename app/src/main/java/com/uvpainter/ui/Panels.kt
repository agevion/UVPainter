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
import com.uvpainter.i18n.AppLanguage
import com.uvpainter.i18n.LocalAppLanguage
import com.uvpainter.i18n.LocalStrings
import com.uvpainter.i18n.translateLayerName
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
import com.uvpainter.ui.components.ChipRow
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
    val t = LocalStrings.current
    Panel(modifier = modifier, width = 320) {
        Column(modifier = Modifier.heightIn(max = 560.dp).verticalScroll(rememberScrollState())) {
            PanelTitle(t.brush.title)

            LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                items(controller.brushPresets) { preset ->
                    ToggleChip(
                        label = preset.name(t),
                        selected = false,
                        onClick = { controller.applyPreset(preset) },
                    )
                }
            }

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            LabeledSlider(
                t.brush.size, brush.radiusPx,
                { controller.updateBrush { b -> b.copy(radiusPx = it) } },
                valueRange = 1f..300f,
                format = { "${it.roundToInt()} px" },
                step = 1f,
            )
            SmallToggleRow(t.brush.lockSize, brush.lockSizeToSurface) { value ->
                controller.updateBrush { b -> b.copy(lockSizeToSurface = value) }
            }
            Text(
                t.brush.lockSizeHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            LabeledSlider(
                t.brush.hardness, brush.hardness,
                { controller.updateBrush { b -> b.copy(hardness = it) } },
            )
            LabeledSlider(
                t.brush.opacity, brush.opacity,
                { controller.updateBrush { b -> b.copy(opacity = it) } },
            )
            LabeledSlider(
                t.brush.flow, brush.flow,
                { controller.updateBrush { b -> b.copy(flow = it) } },
            )
            // El regulador de trazo vive en la barra, no aquí: se enciende y se
            // apaga a media lámina, y bajar a un panel para eso rompe el ritmo.
            LabeledSlider(
                t.brush.smoothing, brush.smoothing,
                { controller.updateBrush { b -> b.copy(smoothing = it) } },
                valueRange = 0f..0.95f,
            )
            LabeledSlider(
                t.brush.spacing, brush.spacingPx,
                { controller.updateBrush { b -> b.copy(spacingPx = it) } },
                valueRange = 0.5f..24f,
                format = { "${"%.1f".format(it)} px" },
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle(t.brush.tipTitle)

            ChipRow {
                TipShape.entries.forEach { shape ->
                    ToggleChip(shape.label(t), brush.tipShape == shape) {
                        controller.updateBrush { b -> b.copy(tipShape = shape) }
                    }
                }
            }
            if (brush.tipShape == TipShape.FLAT) {
                LabeledSlider(
                    t.brush.tipFlatten, brush.tipAspect,
                    { controller.updateBrush { b -> b.copy(tipAspect = it) } },
                    valueRange = 0.05f..1f,
                )
                LabeledSlider(
                    t.brush.tipAngle, brush.tipAngle,
                    { controller.updateBrush { b -> b.copy(tipAngle = it) } },
                    valueRange = 0f..3.1416f,
                    format = { "${(it * 180f / 3.1416f).roundToInt()}°" },
                )
                SmallToggleRow(t.brush.tipFollowsStroke, brush.tipFollowsStroke) { value ->
                    controller.updateBrush { b -> b.copy(tipFollowsStroke = value) }
                }
            }
            if (brush.tipShape != TipShape.SPRAY) {
                LabeledSlider(
                    t.brush.grain, brush.grainAmount,
                    { controller.updateBrush { b -> b.copy(grainAmount = it) } },
                )
            }
            LabeledSlider(
                t.brush.grainScale, brush.grainScale,
                { controller.updateBrush { b -> b.copy(grainScale = it) } },
                valueRange = 100f..2500f,
                format = { "${it.roundToInt()}" },
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle(t.brush.penTitle)

            SmallToggleRow(t.brush.pressureToSize, brush.pressureAffectsSize) { value ->
                controller.updateBrush { b -> b.copy(pressureAffectsSize = value) }
            }
            SmallToggleRow(t.brush.pressureToOpacity, brush.pressureAffectsOpacity) { value ->
                controller.updateBrush { b -> b.copy(pressureAffectsOpacity = value) }
            }
            LabeledSlider(
                t.brush.sensitivity, brush.pressureGain,
                { controller.updateBrush { b -> b.copy(pressureGain = it) } },
                valueRange = 0.6f..3f,
                format = { "%.2f×".format(it) },
            )
            Text(
                t.brush.sensitivityHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            LabeledSlider(
                t.brush.pressureCurve, brush.pressureCurve,
                { controller.updateBrush { b -> b.copy(pressureCurve = it) } },
                valueRange = 0.25f..2.5f,
                format = { "%.2f".format(it) },
            )
            LabeledSlider(
                t.brush.minSize, brush.pressureSizeFloor,
                { controller.updateBrush { b -> b.copy(pressureSizeFloor = it) } },
            )
            LabeledSlider(
                t.brush.tiltToSize, brush.tiltSize,
                { controller.updateBrush { b -> b.copy(tiltSize = it) } },
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle(t.brush.projectionTitle)

            SmallToggleRow(t.brush.restrictToIsland, brush.restrictToIsland) { value ->
                controller.updateBrush { b -> b.copy(restrictToIsland = value) }
            }
            Text(
                t.brush.restrictToIslandHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            SmallToggleRow(t.brush.noPaintHidden, brush.depthTest) { value ->
                controller.updateBrush { b -> b.copy(depthTest = value) }
            }
            SmallToggleRow(t.brush.noPaintBackfaces, brush.backfaceCull) { value ->
                controller.updateBrush { b -> b.copy(backfaceCull = value) }
            }
            SmallToggleRow(t.brush.alphaLock, brush.alphaLock) { value ->
                controller.updateBrush { b -> b.copy(alphaLock = value) }
            }
            LabeledSlider(
                t.brush.edgeFade, brush.facingFull,
                { controller.updateBrush { b -> b.copy(facingFull = it) } },
                valueRange = 0.05f..0.9f,
            )
        }
    }
}

// ---------------------------------------------------------------------------
// Bote de pintura
// ---------------------------------------------------------------------------
/**
 * Lo del bote tiene panel propio: colgaba del de pincel, y ahí no se encuentra
 * quien acaba de tocar el icono del cubo.
 */
@Composable
fun FillPanel(controller: PainterController, modifier: Modifier = Modifier) {
    val brush = controller.brush
    val t = LocalStrings.current
    Panel(modifier = modifier, width = 320) {
        PanelTitle(t.fill.title)

        SmallToggleRow(t.fill.closedArea, brush.fillClosedArea) { value ->
            controller.updateBrush { b -> b.copy(fillClosedArea = value) }
        }
        Text(
            if (brush.fillClosedArea) t.fill.closedAreaHelp else t.fill.islandHelp,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        if (brush.fillClosedArea) {
            LabeledSlider(
                t.fill.tolerance, brush.fillTolerance,
                { controller.updateBrush { b -> b.copy(fillTolerance = it) } },
                valueRange = 0f..0.6f,
                format = { "${(it * 100).roundToInt()}%" },
            )
            Text(
                t.fill.toleranceHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        Text(
            t.fill.footer,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------
@Composable
fun ColorPanel(controller: PainterController, modifier: Modifier = Modifier) {
    val t = LocalStrings.current
    Panel(modifier = modifier, width = 300) {
        PanelTitle(t.color.title)
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
            t.color.help,
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
    val t = LocalStrings.current
    Panel(modifier = modifier, width = 320) {
        PanelTitle(t.shape.title)
        Text(
            t.shape.help,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(bottom = 6.dp),
        )
        ChipRow {
            ShapeKind.entries.forEach { kind ->
                ToggleChip(kind.label(t), brush.shape == kind) {
                    controller.updateBrush { b -> b.copy(shape = kind) }
                    if (kind != ShapeKind.NONE) controller.changeTool(Tool.SHAPE)
                }
            }
        }

        if (brush.shape == ShapeKind.POLYGON) {
            LabeledSlider(
                t.shape.polygonSides, brush.polygonSides.toFloat(),
                { controller.updateBrush { b -> b.copy(polygonSides = it.roundToInt()) } },
                valueRange = 3f..24f,
                format = { "${it.roundToInt()}" },
            )
        }
        if (brush.shape == ShapeKind.RECTANGLE || brush.shape == ShapeKind.ELLIPSE) {
            SmallToggleRow(t.shape.fromCenter, brush.shapeFromCenter) { value ->
                controller.updateBrush { b -> b.copy(shapeFromCenter = value) }
            }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 12.dp), color = Color(0x22FFFFFF))
        PanelTitle(t.shape.boundsTitle)
        Text(
            t.shape.boundsHelp,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        ChipRow(modifier = Modifier.padding(top = 8.dp)) {
            ToggleChip(t.shape.drawBoundary, controller.tool == Tool.BOUNDARY) {
                controller.changeTool(
                    if (controller.tool == Tool.BOUNDARY) Tool.BRUSH else Tool.BOUNDARY,
                )
            }
            ToggleChip(t.shape.clearBoundary, false) { controller.clearBoundary() }
        }
        SmallToggleRow(t.shape.respectBounds, brush.restrictToRegion) { value ->
            controller.updateBrush { b -> b.copy(restrictToRegion = value) }
        }
        Text(
            if (controller.hasBoundary) t.shape.boundsActive else t.shape.boundsNone,
            style = MaterialTheme.typography.labelSmall,
            color = if (controller.hasBoundary) {
                MaterialTheme.colorScheme.primary
            } else {
                MaterialTheme.colorScheme.onSurfaceVariant
            },
            modifier = Modifier.padding(top = 4.dp),
        )
        Text(
            t.shape.boundsThickness,
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
    val t = LocalStrings.current
    Panel(modifier = modifier, width = 330) {
        Column(modifier = Modifier.heightIn(max = 560.dp).verticalScroll(rememberScrollState())) {
            PanelTitle(t.input.title)

            Text(
                t.input.fingersToNavigate,
                style = MaterialTheme.typography.labelMedium,
            )
            ChipRow(modifier = Modifier.padding(top = 4.dp)) {
                ToggleChip(t.input.oneFinger, input.navigationMinFingers == 1) {
                    controller.updateInput { it.copy(navigationMinFingers = 1) }
                }
                ToggleChip(t.input.twoFingers, input.navigationMinFingers == 2) {
                    controller.updateInput { it.copy(navigationMinFingers = 2) }
                }
            }
            Text(
                t.input.twoFingersHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 2.dp),
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            SmallToggleRow(t.input.rejectWide, input.palmSizeRejection) { value ->
                controller.updateInput { it.copy(palmSizeRejection = value) }
            }
            LabeledSlider(
                t.input.palmThreshold, input.palmTouchMajorMm,
                { controller.updateInput { s -> s.copy(palmTouchMajorMm = it) } },
                valueRange = 8f..30f,
                format = { "${it.roundToInt()} mm" },
            )
            Text(
                t.input.palmThresholdHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            SmallToggleRow(t.input.stylusOnly, input.stylusOnlyMode) { value ->
                controller.updateInput { it.copy(stylusOnlyMode = value) }
            }
            SmallToggleRow(t.input.fingerPaints, input.fingerCanPaint) { value ->
                controller.updateInput { it.copy(fingerCanPaint = value) }
            }
            SmallToggleRow(t.input.twistRotates, input.twistToRotateView) { value ->
                controller.updateInput { it.copy(twistToRotateView = value) }
            }

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle(t.input.cameraTitle)

            SmallToggleRow(t.input.zoomToPinch, input.zoomToPinchCenter) { value ->
                controller.updateInput { it.copy(zoomToPinchCenter = value) }
            }
            Text(
                t.input.zoomToPinchHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            SmallToggleRow(t.input.twoFingerPan, input.twoFingerPan) { value ->
                controller.updateInput { it.copy(twoFingerPan = value) }
            }
            Text(
                t.input.twoFingerPanHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            LabeledSlider(
                t.input.orbitSensitivity, input.orbitSensitivity,
                { controller.updateInput { s -> s.copy(orbitSensitivity = it) } },
                valueRange = 0.25f..2.5f,
                format = { "%.2f×".format(it) },
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle(t.input.penButtonTitle)
            Text(
                t.input.penButtonHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(bottom = 6.dp),
            )
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                PenButtonAction.entries.forEach { action ->
                    ToggleChip(
                        label = action.label(t),
                        selected = input.penButtonAction == action,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        controller.updateInput { it.copy(penButtonAction = action) }
                    }
                }
            }

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))
            PanelTitle(t.input.gesturesTitle)
            Text(
                t.input.gesturesHelp,
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
    val t = LocalStrings.current
    Panel(modifier = modifier, width = 330) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            PanelTitle(t.layers.title)
            Row {
                IconButton(onClick = { controller.addLayer() }, modifier = Modifier.size(30.dp)) {
                    Icon(Icons.Filled.Add, t.layers.add, modifier = Modifier.size(18.dp))
                }
                IconButton(
                    onClick = { controller.duplicateLayer(controller.activeLayer) },
                    modifier = Modifier.size(30.dp),
                ) {
                    Icon(Icons.Filled.ContentCopy, t.layers.duplicate, modifier = Modifier.size(16.dp))
                }
                IconButton(
                    onClick = { controller.removeLayer(controller.activeLayer) },
                    modifier = Modifier.size(30.dp),
                ) {
                    Icon(Icons.Filled.Delete, t.layers.delete, modifier = Modifier.size(18.dp))
                }
            }
        }

        Text(
            t.layers.help,
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
    val t = LocalStrings.current
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
                    contentDescription = t.layers.visibility,
                    modifier = Modifier.size(16.dp),
                    tint = if (layer.visible) {
                        MaterialTheme.colorScheme.onSurface
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f)
                    },
                )
            }
            Text(
                translateLayerName(layer.name, t),
                style = MaterialTheme.typography.bodySmall,
                modifier = Modifier.weight(1f).padding(start = 4.dp),
            )
            IconButton(
                onClick = { if (canMoveUp) onMove(1) },
                modifier = Modifier.size(24.dp),
                enabled = canMoveUp,
            ) {
                Icon(Icons.Filled.KeyboardArrowUp, t.layers.moveUp, modifier = Modifier.size(16.dp))
            }
            IconButton(
                onClick = { if (canMoveDown) onMove(-1) },
                modifier = Modifier.size(24.dp),
                enabled = canMoveDown,
            ) {
                Icon(Icons.Filled.KeyboardArrowDown, t.layers.moveDown, modifier = Modifier.size(16.dp))
            }
        }

        if (isActive) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(top = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Box {
                    ToggleChip(
                        label = layer.blend.label(t),
                        selected = layer.blend != BlendMode.NORMAL,
                        onClick = { blendMenuOpen = true },
                    )
                    DropdownMenu(
                        expanded = blendMenuOpen,
                        onDismissRequest = { blendMenuOpen = false },
                    ) {
                        BlendMode.entries.forEach { mode ->
                            DropdownMenuItem(
                                text = { Text(mode.label(t)) },
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
                t.layers.opacity, layer.opacity,
                { onUpdate(layer.copy(opacity = it)) },
                modifier = Modifier.padding(top = 2.dp),
            )
            ChipRow {
                ToggleChip(t.layers.alphaShort, layer.alphaLock) { onUpdate(layer.copy(alphaLock = !layer.alphaLock)) }
                ToggleChip(t.layers.clipShort, layer.clipToBelow) { onUpdate(layer.copy(clipToBelow = !layer.clipToBelow)) }
                ToggleChip(t.layers.lockShort, layer.locked) { onUpdate(layer.copy(locked = !layer.locked)) }
                ToggleChip(t.layers.clearShort, false) { onClear() }
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
    val t = LocalStrings.current
    Panel(modifier = modifier, width = 300) {
        PanelTitle(t.view.title)
        ChipRow {
            ViewMode.entries.forEach { mode ->
                ToggleChip(mode.label(t), viewport.mode == mode) {
                    controller.updateViewport { it.copy(mode = mode) }
                }
            }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        SmallToggleRow(t.view.seams, viewport.seams) { value ->
            controller.updateViewport { s -> s.copy(seams = value) }
        }
        SmallToggleRow(t.view.wireframe, viewport.wireframe) { value ->
            controller.updateViewport { s -> s.copy(wireframe = value) }
        }
        SmallToggleRow(t.view.hideBackfaces, viewport.backfaceCull) { value ->
            controller.updateViewport { s -> s.copy(backfaceCull = value) }
        }
        SmallToggleRow(t.view.markUnpainted, viewport.unpaintedTint) { value ->
            controller.updateViewport { s -> s.copy(unpaintedTint = value) }
        }

        if (viewport.mode == ViewMode.UNLIT) {
            LabeledSlider(
                t.view.unlitShading, viewport.unlitShading,
                { controller.updateViewport { s -> s.copy(unlitShading = it) } },
            )
            Text(
                t.view.unlitShadingHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        if (viewport.mode == ViewMode.PBR) {
            LabeledSlider(t.view.roughness, viewport.roughness, {
                controller.updateViewport { s -> s.copy(roughness = it) }
            })
            LabeledSlider(t.view.metallic, viewport.metallic, {
                controller.updateViewport { s -> s.copy(metallic = it) }
            })
        }
        if (viewport.mode == ViewMode.UV_CHECKER) {
            LabeledSlider(
                t.view.checkerDensity, viewport.uvCheckerDensity,
                { controller.updateViewport { s -> s.copy(uvCheckerDensity = it) } },
                valueRange = 4f..128f,
                format = { "${it.roundToInt()}" },
            )
        }

        ChipRow(modifier = Modifier.padding(top = 8.dp)) {
            ToggleChip(t.view.frameModel, false) { controller.resetView() }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        SmallToggleRow(t.view.showFps, controller.showFps) { value ->
            controller.setFpsVisible(value)
        }
        Text(
            t.view.showFpsHelp,
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
) {
    val info = controller.meshInfo
    val t = LocalStrings.current
    Panel(modifier = modifier, width = 320) {
        // Con el selector de idioma dentro, el panel ya no cabe entero en una
        // tablet en horizontal: se desplaza como el de pincel.
        Column(modifier = Modifier.heightIn(max = 560.dp).verticalScroll(rememberScrollState())) {
            PanelTitle(t.doc.title)

            // El idioma, lo primero: quien abre la app en una lengua que no es la
            // suya no puede leer el resto del panel para encontrarlo.
            Text(t.doc.languageTitle, style = MaterialTheme.typography.labelMedium)
            ChipRow(modifier = Modifier.padding(top = 4.dp)) {
                AppLanguage.entries.forEach { language ->
                    ToggleChip(language.nativeName, controller.language == language) {
                        controller.changeLanguage(language)
                    }
                }
            }
            Text(
                t.doc.languageHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 2.dp),
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            Text(
                if (info.loaded) {
                    t.doc.meshSummary.format(
                        info.name.ifBlank { t.doc.unnamedModel },
                        info.triangles,
                        info.vertices,
                        info.islands,
                    ) + if (info.submeshes > 1) t.doc.meshMaterials.format(info.submeshes) else ""
                } else {
                    t.doc.noModel
                },
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            if (info.uvsOutside01) {
                Text(
                    t.doc.uvWarning,
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.error,
                    modifier = Modifier.padding(top = 4.dp),
                )
            }

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            Text(t.doc.orientationTitle, style = MaterialTheme.typography.labelMedium)
            ChipRow(modifier = Modifier.padding(top = 4.dp)) {
                ToggleChip(t.doc.yUp, controller.upAxis == 0) {
                    controller.setOrientation(newUpAxis = 0)
                }
                ToggleChip(t.doc.zUp, controller.upAxis == 1) {
                    controller.setOrientation(newUpAxis = 1)
                }
                ToggleChip(t.doc.flip, controller.flipUp) {
                    controller.setOrientation(newFlipUp = !controller.flipUp)
                }
                ToggleChip(t.doc.rotate90, false) {
                    controller.setOrientation(newQuarterTurns = controller.quarterTurns + 1)
                }
            }

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            Text(t.doc.atlasTitle, style = MaterialTheme.typography.labelMedium)
            ChipRow(modifier = Modifier.padding(top = 4.dp)) {
                listOf(1024, 2048, 4096).forEach { resolution ->
                    ToggleChip(
                        "$resolution",
                        controller.documentResolution == resolution,
                    ) { controller.changeDocumentResolution(resolution) }
                }
            }
            Text(
                t.doc.atlasHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 2.dp),
            )

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                // Las referencias ya no salen de aquí: tienen botón propio en la
                // barra, que es donde se buscan cuando hacen falta.
                ToggleChip(t.doc.importModel, false) { onImportModel() }
                ToggleChip(t.doc.importImage, false) { onImportImage() }
                ToggleChip(t.doc.exportPng, false) { controller.exportTexture(true) }
            }

            HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

            Text(t.doc.toolbarTitle, style = MaterialTheme.typography.labelMedium)
            Text(
                t.doc.toolbarHelp,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 2.dp, bottom = 6.dp),
            )
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                ToggleChip(t.doc.toolbarReset, false) { controller.resetToolbarPlacement() }
                Text(
                    "${(controller.toolbarScale * 100).roundToInt()}%",
                    style = MaterialTheme.typography.labelSmall,
                    fontFamily = FontFamily.Monospace,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(top = 6.dp),
                )
            }

            Text(
                t.doc.history.format(controller.historyMemoryMb),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 10.dp),
            )
        }
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
    val t = LocalStrings.current
    var name by remember(openName) { mutableStateOf(openName.orEmpty()) }

    Panel(modifier = modifier, width = 340) {
        PanelTitle(t.projects.title)
        Text(
            t.projects.help,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(bottom = 8.dp),
        )

        OutlinedTextField(
            value = name,
            onValueChange = { name = it },
            label = { Text(t.projects.nameLabel) },
            singleLine = true,
            textStyle = MaterialTheme.typography.bodySmall,
            modifier = Modifier.fillMaxWidth(),
        )

        ChipRow(modifier = Modifier.padding(top = 8.dp)) {
            ToggleChip(t.projects.save, false) {
                controller.saveProjectAs(name)
            }
            ToggleChip(t.projects.newProject, false) { controller.newProject() }
            ToggleChip(t.projects.refresh, false) { controller.refreshProjects() }
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        SmallToggleRow(t.projects.autoSave, controller.autoSaveEnabled) { value ->
            controller.setAutoSave(enabled = value)
        }
        Text(
            if (openName != null) {
                t.projects.autoSaveOpen.format(openName)
            } else {
                t.projects.autoSaveNone
            },
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        if (controller.autoSaveEnabled) {
            ChipRow(modifier = Modifier.padding(top = 6.dp)) {
                listOf(1, 3, 5, 10, 15).forEach { minutes ->
                    ToggleChip(t.projects.minutes.format(minutes), controller.autoSaveMinutes == minutes) {
                        controller.setAutoSave(minutes = minutes)
                    }
                }
            }
            Text(
                if (controller.lastAutoSaveMs > 0L) {
                    t.projects.lastCheckpoint.format(
                        DateFormat.getTimeInstance(DateFormat.MEDIUM, controller.language.locale)
                            .format(Date(controller.lastAutoSaveMs)),
                    )
                } else {
                    t.projects.noCheckpoints
                },
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(top = 4.dp),
            )
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 10.dp), color = Color(0x22FFFFFF))

        if (controller.projects.isEmpty()) {
            Text(
                t.projects.empty,
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
            t.projects.savedIn.format(controller.projectsDir.absolutePath),
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
    val t = LocalStrings.current
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
                if (isOpen) t.projects.openSuffix.format(project.name) else project.name,
                style = MaterialTheme.typography.bodySmall,
            )
            Text(
                formatSize(project.sizeBytes) + " · " +
                    DateFormat.getDateTimeInstance(
                        DateFormat.SHORT, DateFormat.SHORT, LocalAppLanguage.current.locale,
                    ).format(Date(project.modifiedMs)),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        ToggleChip(t.projects.open, false) { onOpen() }
        IconButton(onClick = onDelete, modifier = Modifier.size(30.dp)) {
            Icon(Icons.Filled.Delete, t.projects.deleteProject, modifier = Modifier.size(16.dp))
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
    val t = LocalStrings.current
    Panel(modifier = modifier, width = 270) {
        PanelTitle(t.brush.title)
        LabeledSlider(
            t.brush.size, brush.radiusPx,
            { controller.updateBrush { b -> b.copy(radiusPx = it) } },
            valueRange = 1f..300f,
            format = { "${it.roundToInt()} px" },
            step = 1f,
        )
        LabeledSlider(
            t.brush.opacity, brush.opacity,
            { controller.updateBrush { b -> b.copy(opacity = it) } },
        )
        LabeledSlider(
            t.brush.hardness, brush.hardness,
            { controller.updateBrush { b -> b.copy(hardness = it) } },
        )

        Text(
            t.brush.tipTitle,
            style = MaterialTheme.typography.labelMedium,
            modifier = Modifier.padding(top = 6.dp, bottom = 4.dp),
        )
        // En fila desplazable: las cuatro puntas no caben de una vez en un panel
        // estrecho, y en un Row normal la última se aplasta hasta ser ilegible.
        LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            items(TipShape.entries) { shape ->
                ToggleChip(shape.label(t), brush.tipShape == shape) {
                    controller.updateBrush { b -> b.copy(tipShape = shape) }
                }
            }
        }

        ChipRow(modifier = Modifier.padding(top = 8.dp)) {
            ToggleChip(t.tool.brush, controller.tool == Tool.BRUSH) {
                controller.changeTool(Tool.BRUSH)
            }
            ToggleChip(t.tool.eraser, controller.tool == Tool.ERASER) {
                controller.changeTool(Tool.ERASER)
            }
        }

        LazyRow(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.padding(top = 8.dp),
        ) {
            items(controller.brushPresets) { preset ->
                ToggleChip(preset.name(t), false) { controller.applyPreset(preset) }
            }
        }
    }
}
