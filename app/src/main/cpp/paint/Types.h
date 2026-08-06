#pragma once

#include <string>

#include "core/Math.h"

namespace uvp {

enum class BlendMode : int {
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    Overlay = 3,
    Add = 4,
    ColorDodge = 5,
    ColorBurn = 6,
    SoftLight = 7,
};

enum class PaintMode : int {
    Paint = 0,
    Erase = 1,
    /// El trazo no pinta color: dibuja un limite que el pincel no podra cruzar.
    Boundary = 2,
};

/// Forma de la punta. Es lo que diferencia un pincel de otro de verdad.
enum class TipShape : int {
    Round = 0,
    Flat = 1,    // punta plana / tiralineas
    Square = 2,
    Spray = 3,
};

/// Figuras geometricas que puede trazar la herramienta de formas.
enum class ShapeKind : int {
    None = 0,
    Line = 1,
    Rectangle = 2,
    Ellipse = 3,
    Polygon = 4,
};

enum class ViewMode : int {
    Unlit = 0,
    Pbr = 1,
    Matcap = 2,
    UvChecker = 3,
};

// Un punto del trazo tal y como llega del MotionEvent, ya en pixeles de la
// superficie de dibujo.
struct StrokePoint {
    Vec2 screen;
    float pressure = 1.0f;
    float tilt = 0.0f;         // radianes desde la perpendicular
    float orientation = 0.0f;  // radianes, direccion de inclinacion
    double timeMs = 0.0;
    bool predicted = false;
};

struct BrushSettings {
    float radiusPx = 36.0f;
    /// El radio de arriba se mide en pixeles de pantalla al encuadre inicial
    /// del modelo. Con esto activo, acercar la camara para ganar precision ya
    /// no encoge el trazo real hasta volverlo un punto sin fuerza: el pincel
    /// mantiene el mismo tamano sobre la superficie y solo crece en pantalla,
    /// que es justo lo que hace falta para pintar detalle fino de cerca.
    bool lockSizeToSurface = false;
    float hardness = 0.55f;
    float opacity = 1.0f;   // alfa maximo del trazo entero
    float flow = 1.0f;      // cuanto aporta cada pasada
    float spacingPx = 1.5f; // separacion minima entre muestras

    // Punta
    TipShape tipShape = TipShape::Round;
    float tipAspect = 1.0f;          // <1 aplana la punta
    float tipAngle = 0.0f;           // radianes
    bool tipFollowsStroke = false;   // la punta plana gira con el trazo
    float grainAmount = 0.0f;        // textura del trazo
    float grainScale = 700.0f;       // celdas de grano a lo ancho del atlas

    Vec3 color{0.85f, 0.25f, 0.30f};

    // Respuesta al lapiz
    bool pressureAffectsSize = true;
    bool pressureAffectsOpacity = false;
    float pressureSizeFloor = 0.35f;     // radio relativo a presion cero
    float pressureOpacityFloor = 0.45f;
    float pressureGain = 1.6f;           // el S-Pen rara vez alcanza 1.0 real
    float pressureCurve = 0.5f;          // <1 mas sensible al toque suave
    float tiltAffectsSize = 0.0f;        // 0..1

    // Estabilizador del trazo (0 = crudo, 1 = muy suavizado)
    float smoothing = 0.35f;

    /// Regularizador de cuerda: el pincel va atado al lapiz por una cuerda de
    /// esta longitud en pixeles y solo avanza cuando se tensa. Es lo que en
    /// Sketchbook llaman "regular trazo": convierte un pulso tembloroso en
    /// lineas largas y limpias. 0 lo desactiva.
    float stabilizerRadiusPx = 0.0f;

    // Proyeccion
    bool depthTest = true;       // no pintar lo que esta tapado
    bool backfaceCull = true;    // no pintar caras que no miran a camara
    float facingCutoff = 0.02f;  // por debajo, nada
    float facingFull = 0.45f;    // por encima, alfa completo

    bool alphaLock = false;

    /// Confina el trazo a la isla UV donde arranco.
    bool restrictToIsland = true;
    /// Confina el trazo a la region delimitada a mano donde arranco.
    bool restrictToRegion = true;

    // Herramienta de formas. Si no es None, el trazo describe la figura entre
    // el punto inicial y el actual en vez de seguir al lapiz.
    ShapeKind shape = ShapeKind::None;
    int polygonSides = 6;
    bool shapeFromCenter = false;
};

struct LayerInfo {
    std::string name;
    float opacity = 1.0f;
    BlendMode blend = BlendMode::Normal;
    bool visible = true;
    bool locked = false;
    bool alphaLock = false;
    bool clipToBelow = false;
};

}  // namespace uvp
