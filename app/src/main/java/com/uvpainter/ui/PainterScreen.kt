package com.uvpainter.ui

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Brush
import androidx.compose.material.icons.filled.Category
import androidx.compose.material.icons.filled.CenterFocusStrong
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.Colorize
import androidx.compose.material.icons.filled.FormatColorFill
import androidx.compose.material.icons.filled.Gesture
import androidx.compose.material.icons.filled.Fullscreen
import androidx.compose.material.icons.filled.FullscreenExit
import androidx.compose.material.icons.filled.Layers
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.filled.PhotoLibrary
import androidx.compose.material.icons.filled.Redo
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.TouchApp
import androidx.compose.material.icons.filled.Undo
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.uvpainter.i18n.AppLanguage
import com.uvpainter.i18n.LocalAppLanguage
import com.uvpainter.i18n.LocalStrings
import androidx.compose.runtime.CompositionLocalProvider
import com.uvpainter.engine.PainterSurfaceView
import com.uvpainter.engine.PenAction
import com.uvpainter.engine.QuickGesture
import com.uvpainter.state.PainterController
import com.uvpainter.state.PenButtonAction
import com.uvpainter.state.Tool
import com.uvpainter.ui.components.ColorSwatch
import com.uvpainter.ui.components.UvpIcons
import com.uvpainter.ui.components.VerticalSlider
import kotlinx.coroutines.delay
import kotlin.math.roundToInt

private enum class OpenPanel { NONE, BRUSH, FILL, COLOR, LAYERS, VIEW, INPUT, SHAPE, DOCUMENT, PROJECTS }

/**
 * Una referencia abierta. Lleva identificador propio porque puede haber varias
 * a la vez y dos fotos distintas pueden decodificarse al mismo bitmap; sin él,
 * cerrar una cerraría la equivocada.
 */
private data class ReferenceItem(val id: Long, val image: ImageBitmap)

@Composable
fun PainterScreen(controller: PainterController, defaultModelAsset: String?) {
    CompositionLocalProvider(
        LocalStrings provides controller.language.strings,
        LocalAppLanguage provides controller.language,
    ) {
        PainterScreenContent(controller, defaultModelAsset)
    }
}

@Composable
private fun PainterScreenContent(controller: PainterController, defaultModelAsset: String?) {
    var openPanel by remember { mutableStateOf(OpenPanel.NONE) }
    val context = LocalContext.current
    val t = LocalStrings.current
    // Varias referencias a la vez: casi nunca se dibuja con una sola imagen
    // delante, y tener que cerrar la anterior para mirar la siguiente obliga a
    // volver a buscarla en el disco cada vez.
    val referenceImages = remember { mutableStateListOf<ReferenceItem>() }
    var nextReferenceId by remember { mutableStateOf(0L) }

    // El panel de capas es el único que cuesta dinero tenerlo abierto: sus
    // miniaturas se leen de la GPU. El controlador necesita saberlo.
    LaunchedEffect(openPanel) { controller.setLayersPanelVisible(openPanel == OpenPanel.LAYERS) }

    val modelPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri -> uri?.let { controller.importModel(it) } }

    val imagePicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri -> uri?.let { controller.importTextureToActiveLayer(it) } }

    val referencePicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenMultipleDocuments(),
    ) { uris ->
        var failed = false
        uris.forEach { uri ->
            val image = decodeReference(context, uri)
            if (image == null) {
                failed = true
            } else {
                referenceImages.add(ReferenceItem(nextReferenceId++, image))
            }
        }
        if (failed) controller.statusMessage = t.status.referenceOpenFailed
    }

    Box(modifier = Modifier.fillMaxSize().background(Color.Black)) {
        AndroidView(
            modifier = Modifier.fillMaxSize(),
            factory = { context ->
                PainterSurfaceView(context).apply {
                    engineHandle = controller.handle
                    renderThread = controller.renderThread
                    onSurfaceReady = { _, _ -> controller.onSurfaceReady(defaultModelAsset) }
                    onQuickGesture = { gesture ->
                        when (gesture) {
                            QuickGesture.UNDO -> controller.undo()
                            QuickGesture.REDO -> controller.redo()
                            QuickGesture.TOGGLE_UI -> controller.uiVisible = !controller.uiVisible
                        }
                    }
                    onStylusButtonChanged = { pressed -> controller.setTemporaryErase(pressed) }
                    onStrokeFinished = { controller.refreshHistory() }
                    onFillRequested = { x, y -> controller.fillAt(x, y) }
                    onPickRequested = { x, y -> controller.pickColorAt(x.toInt(), y.toInt()) }
                    onPalmRejected = { controller.notePalmRejected() }
                    // Tocar el lienzo cierra lo que hubiera abierto. Es lo que
                    // se espera de un menú flotante, y evita tener que volver al
                    // botón exacto que lo abrió para quitarlo de en medio.
                    onViewportTouch = { openPanel = OpenPanel.NONE }
                }
            },
            update = { view ->
                val input = controller.input
                view.navigationMinFingers = input.navigationMinFingers
                view.zoomToPinchCenter = input.zoomToPinchCenter
                view.twoFingerPan = input.twoFingerPan
                view.stylusOnlyMode = input.stylusOnlyMode
                view.palmSizeRejection = input.palmSizeRejection
                view.palmTouchMajorMm = input.palmTouchMajorMm
                view.fingerCanPaint = input.fingerCanPaint
                view.penButtonErases = input.penButtonAction == PenButtonAction.ERASE_WHILE_HELD
                view.twistToRotateView = input.twistToRotateView
                view.orbitSensitivity = input.orbitSensitivity
                view.penAction = when (controller.tool) {
                    Tool.FILL -> PenAction.FILL
                    Tool.PICKER -> PenAction.PICK
                    else -> PenAction.PAINT
                }
            },
        )

        if (controller.uiVisible) {
            LeftRail(controller)
            TopToolbar(
                controller = controller,
                openPanel = openPanel,
                onPanelToggle = { panel ->
                    openPanel = if (openPanel == panel) OpenPanel.NONE else panel
                },
                onPickReference = { referencePicker.launch(arrayOf("image/*")) },
            )
            PanelHost(
                controller = controller,
                openPanel = openPanel,
                onImportModel = {
                    modelPicker.launch(arrayOf("application/octet-stream", "model/*", "*/*"))
                },
                onImportImage = { imagePicker.launch(arrayOf("image/*")) },
            )
        } else {
            MinimalRail(controller)
        }

        // Cada referencia nace un poco más abajo y a la derecha que la anterior:
        // apiladas en el mismo sitio parecerían una sola.
        referenceImages.forEachIndexed { index, item ->
            key(item.id) {
                ReferencePanel(
                    image = item.image,
                    startOffset = index % 6,
                    onClose = { referenceImages.removeAll { it.id == item.id } },
                    onPickColor = { color ->
                        controller.setColor(color)
                        controller.commitColor(color)
                    },
                )
            }
        }

        if (controller.showFps) FpsOverlay(controller)

        StatusBar(controller)

        if (controller.busy) {
            Box(
                modifier = Modifier.fillMaxSize().background(Color(0x66000000)),
                contentAlignment = Alignment.Center,
            ) {
                CircularProgressIndicator()
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Carril izquierdo: tamaño y opacidad siempre a mano, como en Procreate.
// ---------------------------------------------------------------------------
@Composable
private fun BoxScope.LeftRail(controller: PainterController) {
    val t = LocalStrings.current
    Column(
        modifier = Modifier
            .align(Alignment.CenterStart)
            .padding(start = 10.dp)
            .clip(RoundedCornerShape(22.dp))
            .background(UvpTokens.ToolbarBackground)
            .padding(vertical = 12.dp, horizontal = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        RailSlider(
            caption = t.tool.railSize,
            readout = "${controller.brush.radiusPx.roundToInt()}",
            value = controller.brush.radiusPx,
            valueRange = 1f..300f,
            onValueChange = { value -> controller.updateBrush { it.copy(radiusPx = value) } },
        )
        Spacer(modifier = Modifier.height(14.dp))
        RailSlider(
            caption = t.tool.railOpacity,
            readout = "${(controller.brush.opacity * 100).roundToInt()}",
            value = controller.brush.opacity,
            valueRange = 0f..1f,
            onValueChange = { value -> controller.updateBrush { it.copy(opacity = value) } },
        )
        // La longitud de cuerda solo aparece con el regulador encendido y con el
        // pincel en la mano: ocupa sitio y no significa nada en el resto.
        if (controller.brush.stabilizerRadiusPx > 0.5f && controller.tool == Tool.BRUSH) {
            Spacer(modifier = Modifier.height(14.dp))
            RailSlider(
                caption = t.tool.railStabilizer,
                readout = "${controller.brush.stabilizerRadiusPx.roundToInt()}",
                value = controller.brush.stabilizerRadiusPx,
                valueRange = 4f..120f,
                onValueChange = { value -> controller.setStabilizerRadius(value) },
            )
        }
    }
}

/**
 * Cada barra lleva rotulo arriba y valor abajo. Sin ellos no hay forma de saber
 * cual es cual: dos deslizadores identicos no dicen nada.
 */
@Composable
private fun RailSlider(
    caption: String,
    readout: String,
    value: Float,
    valueRange: ClosedFloatingPointRange<Float>,
    onValueChange: (Float) -> Unit,
) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            caption,
            style = MaterialTheme.typography.labelSmall,
            fontSize = 9.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        VerticalSlider(
            value = value,
            onValueChange = onValueChange,
            valueRange = valueRange,
            modifier = Modifier.height(180.dp).padding(vertical = 5.dp),
        )
        Text(
            readout,
            style = MaterialTheme.typography.labelSmall,
            fontFamily = FontFamily.Monospace,
            color = MaterialTheme.colorScheme.onSurface,
        )
    }
}

/** Carril reducido del modo pantalla completa, al estilo Sketchbook. */
@Composable
private fun BoxScope.MinimalRail(controller: PainterController) {
    val t = LocalStrings.current
    Column(
        modifier = Modifier
            .align(Alignment.CenterStart)
            .padding(start = 6.dp)
            .clip(RoundedCornerShape(18.dp))
            .background(Color(0x99101218))
            .padding(vertical = 8.dp, horizontal = 5.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        VerticalSlider(
            value = controller.brush.radiusPx,
            onValueChange = { value -> controller.updateBrush { it.copy(radiusPx = value) } },
            valueRange = 1f..300f,
            trackWidth = 22,
            modifier = Modifier.height(120.dp),
        )
        Spacer(modifier = Modifier.height(8.dp))
        ColorSwatch(color = controller.brush.color, modifier = Modifier.size(24.dp))
        Spacer(modifier = Modifier.height(8.dp))
        ToolIcon(Icons.Filled.Undo, t.tool.undo, enabled = controller.canUndo, compact = true) {
            controller.undo()
        }
        ToolIcon(Icons.Filled.FullscreenExit, t.tool.showUi, compact = true) {
            controller.uiVisible = true
        }
    }
}

// ---------------------------------------------------------------------------
// Barra superior derecha
// ---------------------------------------------------------------------------
@Composable
private fun BoxScope.TopToolbar(
    controller: PainterController,
    openPanel: OpenPanel,
    onPanelToggle: (OpenPanel) -> Unit,
    onPickReference: () -> Unit,
) {
    val t = LocalStrings.current
    // Las herramientas que no traen panel propio cierran el que hubiera: coger
    // la goma con el panel del pincel abierto dejaba el panel ahí estorbando.
    val closePanels = { onPanelToggle(OpenPanel.NONE) }
    Row(
        modifier = Modifier
            .align(Alignment.TopEnd)
            .padding(top = 10.dp, end = 10.dp)
            .clip(RoundedCornerShape(20.dp))
            .background(UvpTokens.ToolbarBackground)
            .padding(horizontal = 6.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        ToolIcon(Icons.Filled.Undo, t.tool.undo, enabled = controller.canUndo) { controller.undo() }
        ToolIcon(Icons.Filled.Redo, t.tool.redo, enabled = controller.canRedo) { controller.redo() }

        Spacer(modifier = Modifier.width(6.dp))

        // Herramientas del lapiz.
        ToolIcon(Icons.Filled.Brush, t.tool.brush, selected = controller.tool == Tool.BRUSH) {
            controller.changeTool(Tool.BRUSH)
            onPanelToggle(OpenPanel.BRUSH)
        }
        ToolIcon(UvpIcons.Eraser, t.tool.eraser, selected = controller.tool == Tool.ERASER) {
            controller.changeTool(Tool.ERASER)
            closePanels()
        }
        ToolIcon(Icons.Filled.FormatColorFill, t.tool.fill, selected = controller.tool == Tool.FILL) {
            controller.changeTool(Tool.FILL)
            onPanelToggle(OpenPanel.FILL)
        }
        ToolIcon(Icons.Filled.Colorize, t.tool.picker, selected = controller.tool == Tool.PICKER) {
            controller.changeTool(Tool.PICKER)
            closePanels()
        }
        // El regulador se enciende y se apaga a media lamina, asi que vive en la
        // barra y no dentro de un panel. Solo con el pincel: el bote y el
        // cuentagotas no trazan nada, asi que ahi no significa nada. El
        // interruptor sigue a [stabilizerActive] y no a la longitud guardada,
        // que se conserva para cuando se vuelva al pincel.
        ToolIcon(
            Icons.Filled.Gesture,
            t.tool.stabilizer,
            selected = controller.stabilizerActive,
            enabled = controller.tool == Tool.BRUSH,
        ) {
            controller.setStabilizerEnabled(controller.brush.stabilizerRadiusPx <= 0.5f)
            closePanels()
        }
        ToolIcon(
            Icons.Filled.Category,
            t.tool.shapesAndBounds,
            selected = controller.tool == Tool.SHAPE || controller.tool == Tool.BOUNDARY,
        ) {
            if (controller.tool != Tool.SHAPE && controller.tool != Tool.BOUNDARY) {
                controller.changeTool(Tool.SHAPE)
            }
            onPanelToggle(OpenPanel.SHAPE)
        }

        Spacer(modifier = Modifier.width(6.dp))

        Box(modifier = Modifier.size(34.dp).padding(3.dp)) {
            ColorSwatch(
                color = controller.brush.color,
                selected = openPanel == OpenPanel.COLOR,
                modifier = Modifier.fillMaxSize(),
                onClick = { onPanelToggle(OpenPanel.COLOR) },
            )
        }

        ToolIcon(Icons.Filled.Layers, t.tool.layers, selected = openPanel == OpenPanel.LAYERS) {
            onPanelToggle(OpenPanel.LAYERS)
        }
        ToolIcon(Icons.Filled.Palette, t.tool.view, selected = openPanel == OpenPanel.VIEW) {
            onPanelToggle(OpenPanel.VIEW)
        }
        ToolIcon(Icons.Filled.TouchApp, t.tool.penAndPalm, selected = openPanel == OpenPanel.INPUT) {
            onPanelToggle(OpenPanel.INPUT)
        }
        // Las referencias se abren desde aquí y no desde el panel de ajustes:
        // se consultan a media lámina y se abren de varias en varias.
        ToolIcon(Icons.Filled.PhotoLibrary, t.tool.reference) {
            closePanels()
            onPickReference()
        }
        ToolIcon(Icons.Filled.CenterFocusStrong, t.tool.frame) {
            controller.resetView()
            closePanels()
        }
        ToolIcon(Icons.Filled.Folder, t.tool.projects, selected = openPanel == OpenPanel.PROJECTS) {
            onPanelToggle(OpenPanel.PROJECTS)
        }
        ToolIcon(Icons.Filled.Settings, t.tool.document, selected = openPanel == OpenPanel.DOCUMENT) {
            onPanelToggle(OpenPanel.DOCUMENT)
        }
        ToolIcon(Icons.Filled.Fullscreen, t.tool.fullscreen) {
            closePanels()
            controller.uiVisible = false
        }
    }
}

@Composable
private fun ToolIcon(
    icon: ImageVector,
    description: String,
    selected: Boolean = false,
    enabled: Boolean = true,
    compact: Boolean = false,
    onClick: () -> Unit,
) {
    val tint = when {
        !enabled -> MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.35f)
        selected -> MaterialTheme.colorScheme.primary
        else -> MaterialTheme.colorScheme.onSurface
    }
    Box(
        modifier = Modifier
            .size(if (compact) 30.dp else 38.dp)
            .clip(CircleShape)
            .background(if (selected) Color(0x332F80FF) else Color.Transparent)
            .pointerInput(enabled, selected) {
                detectTapGestures { if (enabled) onClick() }
            },
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            icon,
            description,
            tint = tint,
            modifier = Modifier.size(if (compact) 16.dp else 20.dp),
        )
    }
}

// ---------------------------------------------------------------------------
// Paneles flotantes
// ---------------------------------------------------------------------------
@Composable
private fun BoxScope.PanelHost(
    controller: PainterController,
    openPanel: OpenPanel,
    onImportModel: () -> Unit,
    onImportImage: () -> Unit,
) {
    AnimatedVisibility(
        visible = openPanel != OpenPanel.NONE,
        enter = fadeIn() + slideInHorizontally { it / 3 },
        exit = fadeOut() + slideOutHorizontally { it / 3 },
        modifier = Modifier.align(Alignment.TopEnd).padding(top = 62.dp, end = 10.dp),
    ) {
        when (openPanel) {
            OpenPanel.BRUSH -> BrushPanel(controller)
            OpenPanel.FILL -> FillPanel(controller)
            OpenPanel.COLOR -> ColorPanel(controller)
            OpenPanel.LAYERS -> LayersPanel(controller)
            OpenPanel.VIEW -> ViewPanel(controller)
            OpenPanel.INPUT -> InputPanel(controller)
            OpenPanel.SHAPE -> ShapePanel(controller)
            OpenPanel.DOCUMENT -> DocumentPanel(controller, onImportModel, onImportImage)
            OpenPanel.PROJECTS -> ProjectsPanel(controller)
            OpenPanel.NONE -> Unit
        }
    }
}

// ---------------------------------------------------------------------------
// Mensajes
// ---------------------------------------------------------------------------
/**
 * Contador de fotogramas, arriba a la izquierda. Se enciende desde el panel de
 * vista y solo entonces se mide: con el apagado, el hilo de render ni cuenta.
 */
@Composable
private fun BoxScope.FpsOverlay(controller: PainterController) {
    Box(
        modifier = Modifier
            .align(Alignment.TopStart)
            .padding(start = 10.dp, top = 10.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(Color(0x99101218))
            .padding(horizontal = 8.dp, vertical = 4.dp),
    ) {
        Text(
            "${controller.fps.roundToInt()} FPS",
            style = MaterialTheme.typography.labelSmall,
            fontFamily = FontFamily.Monospace,
            // Verde mientras va fino, ambar en cuanto cae por debajo de 30.
            color = if (controller.fps >= 30f) Color(0xFF8BE28B) else Color(0xFFE2A98B),
        )
    }
}

/**
 * Abre una imagen de referencia y la deja en memoria a un tamaño razonable.
 *
 * Se reduce al decodificar porque ahora puede haber varias abiertas y las fotos
 * de una cámara moderna pasan de los 50 MP: sin esto, tres referencias se comen
 * la memoria de la app y se la quitan al atlas, que es lo que de verdad importa.
 * Mil seiscientos píxeles de lado dan de sobra para ver la referencia ampliada.
 */
private fun decodeReference(context: android.content.Context, uri: android.net.Uri): ImageBitmap? =
    runCatching {
        val bounds = android.graphics.BitmapFactory.Options().apply { inJustDecodeBounds = true }
        context.contentResolver.openInputStream(uri)?.use {
            android.graphics.BitmapFactory.decodeStream(it, null, bounds)
        }
        val largestSide = maxOf(bounds.outWidth, bounds.outHeight)
        var sample = 1
        while (largestSide / sample > MAX_REFERENCE_SIDE) sample *= 2

        val options = android.graphics.BitmapFactory.Options().apply { inSampleSize = sample }
        context.contentResolver.openInputStream(uri)?.use { stream ->
            android.graphics.BitmapFactory.decodeStream(stream, null, options)?.asImageBitmap()
        }
    }.getOrNull()

private const val MAX_REFERENCE_SIDE = 1600

@Composable
private fun BoxScope.StatusBar(controller: PainterController) {
    val message = controller.statusMessage

    LaunchedEffect(message) {
        if (message != null) {
            delay(3500)
            controller.statusMessage = null
        }
    }

    AnimatedVisibility(
        visible = message != null,
        enter = fadeIn(),
        exit = fadeOut(),
        modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 16.dp),
    ) {
        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(10.dp))
                .background(UvpTokens.PanelBackground)
                .border(1.dp, UvpTokens.PanelBorder, RoundedCornerShape(10.dp))
                .padding(horizontal = 14.dp, vertical = 8.dp),
        ) {
            Text(
                message.orEmpty(),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurface,
            )
        }
    }
}
