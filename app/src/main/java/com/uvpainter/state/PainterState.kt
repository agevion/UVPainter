package com.uvpainter.state

import androidx.compose.ui.graphics.Color
import kotlinx.serialization.KSerializer
import kotlinx.serialization.Serializable
import kotlinx.serialization.descriptors.PrimitiveKind
import kotlinx.serialization.descriptors.PrimitiveSerialDescriptor
import kotlinx.serialization.descriptors.SerialDescriptor
import kotlinx.serialization.encoding.Decoder
import kotlinx.serialization.encoding.Encoder

/**
 * Color como un entero sin signo. Es lo unico de estos ajustes que no sabe
 * serializarse solo, y sin el no se puede guardar el pincel entero de una pieza.
 */
object ColorAsLong : KSerializer<Color> {
    override val descriptor: SerialDescriptor =
        PrimitiveSerialDescriptor("Color", PrimitiveKind.LONG)

    override fun serialize(encoder: Encoder, value: Color) =
        encoder.encodeLong(value.value.toLong())

    override fun deserialize(decoder: Decoder): Color = Color(decoder.decodeLong().toULong())
}

enum class ViewMode(val nativeValue: Int, val label: String) {
    UNLIT(0, "Plano"),
    PBR(1, "PBR"),
    MATCAP(2, "Matcap"),
    UV_CHECKER(3, "Cuadrícula UV"),
}

enum class BlendMode(val nativeValue: Int, val label: String) {
    NORMAL(0, "Normal"),
    MULTIPLY(1, "Multiplicar"),
    SCREEN(2, "Trama"),
    OVERLAY(3, "Superponer"),
    ADD(4, "Añadir"),
    COLOR_DODGE(5, "Subexponer"),
    COLOR_BURN(6, "Sobreexponer"),
    SOFT_LIGHT(7, "Luz suave"),
    ;

    companion object {
        fun fromNative(value: Int): BlendMode = entries.firstOrNull { it.nativeValue == value } ?: NORMAL
    }
}

enum class PaintMode(val nativeValue: Int) { PAINT(0), ERASE(1), BOUNDARY(2) }

/** Forma de la punta: es lo que diferencia un pincel de otro de verdad. */
enum class TipShape(val nativeValue: Int, val label: String) {
    ROUND(0, "Redonda"),
    FLAT(1, "Plana"),
    SQUARE(2, "Cuadrada"),
    SPRAY(3, "Spray"),
}

/** Figuras que puede trazar la herramienta de formas. */
enum class ShapeKind(val nativeValue: Int, val label: String) {
    NONE(0, "Libre"),
    LINE(1, "Línea"),
    RECTANGLE(2, "Rectángulo"),
    ELLIPSE(3, "Elipse"),
    POLYGON(4, "Polígono"),
}

@Serializable
data class BrushState(
    val radiusPx: Float = 36f,
    /**
     * El tamaño de arriba se mide en píxeles de pantalla al encuadre inicial.
     * Con esto activo, acercar la cámara para ganar precisión ya no encoge el
     * trazo real: el pincel mantiene el mismo tamaño sobre la superficie y
     * solo crece en pantalla.
     */
    val lockSizeToSurface: Boolean = false,
    val hardness: Float = 0.55f,
    val opacity: Float = 1f,
    val flow: Float = 1f,
    val spacingPx: Float = 1.5f,
    @Serializable(with = ColorAsLong::class)
    val color: Color = Color(0xFFD9564A),
    val smoothing: Float = 0.35f,
    /** Regularizador de trazo tipo Sketchbook. 0 = desactivado. */
    val stabilizerRadiusPx: Float = 0f,
    // El S-Pen rara vez llega a 1.0 de presion en un trazo normal, y una curva
    // lineal obliga a apretar. Con ganancia por encima de 1 y exponente por
    // debajo, un apoyo suave ya da trazo pleno.
    val pressureGain: Float = 1.6f,
    val pressureCurve: Float = 0.5f,
    val pressureSizeFloor: Float = 0.35f,
    val pressureOpacityFloor: Float = 0.45f,
    val tiltSize: Float = 0f,
    val facingCutoff: Float = 0.02f,
    val facingFull: Float = 0.45f,
    val pressureAffectsSize: Boolean = true,
    // Apagado por defecto: para color plano de anime interesa opacidad
    // constante, y es la causa numero uno de "tengo que apretar mucho".
    val pressureAffectsOpacity: Boolean = false,
    val depthTest: Boolean = true,
    val backfaceCull: Boolean = true,
    val alphaLock: Boolean = false,
    /** Solo pinta la isla UV donde arrancó el trazo. */
    val restrictToIsland: Boolean = true,
    /** Solo pinta la región delimitada a mano donde arrancó el trazo. */
    val restrictToRegion: Boolean = true,

    // Punta: lo que hace que un preajuste sea otro pincel y no el mismo con
    // otros números.
    val tipShape: TipShape = TipShape.ROUND,
    val tipAspect: Float = 1f,
    val tipAngle: Float = 0f,
    val tipFollowsStroke: Boolean = false,
    val grainAmount: Float = 0f,
    val grainScale: Float = 700f,

    // Bote de pintura
    /** Se para donde cambia el color en vez de llenar la isla UV entera. */
    val fillClosedArea: Boolean = false,
    /** Cuánto puede variar el color y seguir contando como la misma zona. */
    val fillTolerance: Float = 0.12f,

    // Formas geométricas
    val shape: ShapeKind = ShapeKind.NONE,
    val polygonSides: Int = 6,
    val shapeFromCenter: Boolean = false,
)

/**
 * Qué hace el botón lateral del S-Pen.
 *
 * Las de «mientras se mantiene» son momentáneas: actúan al pulsar y se deshacen
 * al soltar. El resto se disparan una vez, al pulsar.
 */
@Serializable
enum class PenButtonAction(val label: String, val momentary: Boolean = false) {
    NONE("Nada"),
    ERASE_WHILE_HELD("Borrar mientras lo mantengo", momentary = true),
    PICK_WHILE_HELD("Cuentagotas mientras lo mantengo", momentary = true),
    TOGGLE_ERASER("Cambiar entre pincel y borrador"),
    UNDO("Deshacer"),
    REDO("Rehacer"),
    TOGGLE_STABILIZER("Activar o quitar el regulador de trazo"),
    RESET_VIEW("Encuadrar el modelo"),
    TOGGLE_UI("Ocultar o mostrar la interfaz"),
}

/** Ajustes de entrada: rechazo de palma y gestos. */
@Serializable
data class InputSettings(
    val navigationMinFingers: Int = 2,
    val stylusOnlyMode: Boolean = false,
    val palmSizeRejection: Boolean = true,
    val palmTouchMajorMm: Float = 17f,
    val fingerCanPaint: Boolean = false,
    val penButtonAction: PenButtonAction = PenButtonAction.ERASE_WHILE_HELD,
    val twistToRotateView: Boolean = true,
    val orbitSensitivity: Float = 1f,
)

/** Ajustes de pincel guardados como preajuste. */
data class BrushPreset(val name: String, val brush: BrushState)

@Serializable
data class ViewportState(
    // Plano por defecto: es el unico modo en el que el color que pintas es
    // exactamente el que elegiste. El PBR pasa por tonemap y gamma, asi que
    // desplaza el tono y hace imposible juzgar un color liso.
    val mode: ViewMode = ViewMode.UNLIT,
    val wireframe: Boolean = false,
    /** Bordes de isla UV en naranja: sin esto no se ve por dónde está cortado. */
    val seams: Boolean = true,
    val backfaceCull: Boolean = false,
    val unpaintedTint: Boolean = true,
    val roughness: Float = 0.65f,
    val metallic: Float = 0f,
    val uvCheckerDensity: Float = 32f,
    /**
     * Sombreado de la vista plana. Multiplica los tres canales por igual, así
     * que da volumen sin desplazar el tono. A 0 el color es exacto al píxel.
     */
    val unlitShading: Float = 0.4f,
)

data class LayerUi(
    val index: Int,
    val name: String,
    val opacity: Float,
    val blend: BlendMode,
    val visible: Boolean,
    val locked: Boolean,
    val alphaLock: Boolean,
    val clipToBelow: Boolean,
    /** Miniatura del contenido. Sin esto el panel de capas no dice nada. */
    val thumbnail: androidx.compose.ui.graphics.ImageBitmap? = null,
)

/** Un proyecto guardado en disco, tal y como lo lista el panel. */
data class ProjectEntry(
    val name: String,
    val path: String,
    val sizeBytes: Long,
    val modifiedMs: Long,
)

data class MeshInfo(
    val vertices: Int = 0,
    val triangles: Int = 0,
    val submeshes: Int = 0,
    val islands: Int = 0,
    val hasUVs: Boolean = false,
    val uvsOutside01: Boolean = false,
    val loaded: Boolean = false,
    val name: String = "",
)

/** Herramienta activa del lápiz. */
@Serializable
enum class Tool { BRUSH, ERASER, FILL, PICKER, SHAPE, BOUNDARY }

/**
 * Todo lo que la app recuerda entre sesiones y que no es pintura.
 *
 * Va en un solo JSON en preferencias: añadir un ajuste no obliga a tocar el
 * guardado, y los que falten se rellenan con su valor por defecto, así que una
 * versión vieja del archivo sigue abriendo.
 */
@Serializable
data class UiSettings(
    val brush: BrushState = BrushState(),
    val input: InputSettings = InputSettings(),
    val viewport: ViewportState = ViewportState(),
    val tool: Tool = Tool.BRUSH,
    val documentResolution: Int = 2048,
    /** Posición y tamaño de la barra principal, que el usuario puede mover. */
    val toolbarOffsetX: Float = 0f,
    val toolbarOffsetY: Float = 0f,
    val toolbarScale: Float = 1f,
    /** Longitud de cuerda que se recupera al reactivar el regulador de trazo. */
    val stabilizerMemory: Float = 24f,
    /** Punto de control periódico, por si la app se va abajo con trabajo hecho. */
    val autoSaveEnabled: Boolean = false,
    val autoSaveMinutes: Int = 5,
    val showFps: Boolean = false,
    /** Proyecto con nombre que estaba abierto, para seguir actualizándolo. */
    val openProjectPath: String? = null,
)

/** Preajustes iniciales pensados para linea y color plano de estilo anime. */
val defaultBrushPresets: List<BrushPreset> = listOf(
    BrushPreset(
        "Entintado",
        BrushState(
            radiusPx = 14f, hardness = 0.98f, opacity = 1f, flow = 1f, smoothing = 0.55f,
            pressureGain = 1.8f, pressureCurve = 0.45f, pressureSizeFloor = 0.15f,
            pressureAffectsSize = true, pressureAffectsOpacity = false,
        ),
    ),
    BrushPreset(
        "Color plano",
        BrushState(
            radiusPx = 55f, hardness = 1f, opacity = 1f, flow = 1f, smoothing = 0.25f,
            pressureAffectsSize = false, pressureAffectsOpacity = false,
        ),
    ),
    BrushPreset(
        "Sombra suave",
        BrushState(
            radiusPx = 90f, hardness = 0.15f, opacity = 0.5f, flow = 0.35f, smoothing = 0.3f,
            pressureGain = 1.4f, pressureCurve = 0.8f, pressureOpacityFloor = 0.15f,
            pressureAffectsSize = true, pressureAffectsOpacity = true,
        ),
    ),
    BrushPreset(
        "Aerógrafo",
        BrushState(
            radiusPx = 130f, hardness = 0.05f, opacity = 0.3f, flow = 0.35f, smoothing = 0.2f,
            tipShape = TipShape.SPRAY, grainScale = 1400f,
            pressureGain = 1.3f, pressureCurve = 1.1f, pressureOpacityFloor = 0.05f,
            pressureAffectsSize = false, pressureAffectsOpacity = true,
        ),
    ),
    BrushPreset(
        "Detalle fino",
        BrushState(
            radiusPx = 6f, hardness = 0.95f, opacity = 1f, flow = 1f, smoothing = 0.6f,
            pressureGain = 1.8f, pressureCurve = 0.45f, pressureSizeFloor = 0.25f,
            pressureAffectsOpacity = false,
        ),
    ),
    BrushPreset(
        "Tiralíneas",
        BrushState(
            radiusPx = 26f, hardness = 0.95f, opacity = 1f, flow = 1f,
            stabilizerRadiusPx = 22f, smoothing = 0.4f,
            tipShape = TipShape.FLAT, tipAspect = 0.22f, tipFollowsStroke = false,
            tipAngle = 0.6f,
            pressureAffectsSize = true, pressureSizeFloor = 0.5f,
        ),
    ),
    BrushPreset(
        "Carboncillo",
        BrushState(
            radiusPx = 60f, hardness = 0.35f, opacity = 0.85f, flow = 0.6f, smoothing = 0.2f,
            tipShape = TipShape.FLAT, tipAspect = 0.55f, tipFollowsStroke = true,
            grainAmount = 0.75f, grainScale = 1100f,
            pressureAffectsOpacity = true, pressureOpacityFloor = 0.2f,
        ),
    ),
    BrushPreset(
        "Textura seca",
        BrushState(
            radiusPx = 45f, hardness = 0.7f, opacity = 1f, flow = 0.8f, smoothing = 0.25f,
            tipShape = TipShape.SQUARE, grainAmount = 0.5f, grainScale = 500f,
        ),
    ),
)
