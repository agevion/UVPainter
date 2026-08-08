// Todo el GLSL de la app. Se usa `#version 300 es` a proposito: es lo que
// garantiza que funcione aunque el driver nos devuelva un contexto ES 3.0.
#pragma once

namespace uvp::shaders {

// ---------------------------------------------------------------------------
// Comun
// ---------------------------------------------------------------------------

// Triangulo de pantalla completa generado desde gl_VertexID: sin VBO.
inline constexpr const char* kFullscreenVS = R"(#version 300 es
out vec2 vUv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Reduccion a miniatura con filtro de caja. Bajar de 2048 a 96 con una sola
// muestra por texel se comeria cualquier trazo fino; con 6x6 muestras la
// miniatura representa de verdad lo que hay en la capa.
inline constexpr const char* kThumbnailFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;
uniform sampler2D uTex;
uniform vec2 uSourceTexel;   // 1 / tamano de la fuente
uniform float uFootprint;    // texels de fuente por texel de destino

void main() {
    vec4 sum = vec4(0.0);
    const int kTaps = 6;
    for (int y = 0; y < kTaps; ++y) {
        for (int x = 0; x < kTaps; ++x) {
            vec2 offset = (vec2(float(x), float(y)) / float(kTaps - 1) - 0.5) * uFootprint;
            sum += texture(uTex, vUv + offset * uSourceTexel);
        }
    }
    fragColor = sum / float(kTaps * kTaps);
}
)";

// Copia directa de textura a textura.
inline constexpr const char* kCopyFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;
uniform sampler2D uTex;
void main() { fragColor = texture(uTex, vUv); }
)";

// Reduccion por maximos: encadenada, halla el rectangulo tocado por el trazo
// sin tener que leer de vuelta el atlas entero.
inline constexpr const char* kMaxReduceFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;
uniform sampler2D uTex;
uniform vec2 uSrcTexelSize;
void main() {
    vec2 base = vUv - uSrcTexelSize * 0.5;
    float a = texture(uTex, base).r;
    float b = texture(uTex, base + vec2(uSrcTexelSize.x, 0.0)).r;
    float c = texture(uTex, base + vec2(0.0, uSrcTexelSize.y)).r;
    float d = texture(uTex, base + uSrcTexelSize).r;
    fragColor = vec4(max(max(a, b), max(c, d)), 0.0, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Prepaso de profundidad.
//
// No se samplea el z-buffer: se escribe la distancia LINEAL a camara empaquetada
// en RGBA8. Con un near/far muy dispar, deslinealizar el z-buffer en el shader
// pierde tanta precision que el test de oclusion descarta de todo, y ademas
// asi el buffer se puede leer de vuelta para depurar.
// ---------------------------------------------------------------------------
inline constexpr const char* kDepthVS = R"(#version 300 es
layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aUv;
layout(location = 3) in float aIsland;
uniform mat4 uViewProj;
uniform mat4 uView;
out float vViewZ;
out vec2 vAtlasUv;
flat out float vIsland;
void main() {
    vViewZ = -(uView * vec4(aPosition, 1.0)).z;
    vAtlasUv = aUv;
    vIsland = aIsland;
    gl_Position = uViewProj * vec4(aPosition, 1.0);
}
)";

// Tres adjuntos: profundidad lineal empaquetada, el identificador de isla UV y
// la coordenada del atlas. Con una lectura de un solo pixel el motor sabe sobre
// que isla se apoyo el lapiz y, ademas, en que texel exacto del atlas cae: de
// ahi arranca el relleno por area cerrada.
inline constexpr const char* kDepthFS = R"(#version 300 es
precision highp float;
in float vViewZ;
in vec2 vAtlasUv;
flat in float vIsland;
uniform float uFar;
uniform sampler2D uRegionMap;
uniform bool uHasRegions;
layout(location = 0) out vec4 fragDepth;
layout(location = 1) out vec4 fragIds;
layout(location = 2) out vec4 fragAtlasUv;

vec4 packDepth(float v) {
    vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
    enc = fract(enc);
    enc -= enc.yzww * vec4(1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0, 0.0);
    return enc;
}

// 16 bits por coordenada. Con 8 el error seria de varios texels del atlas y el
// relleno arrancaria en el texel de al lado, que puede estar al otro lado del
// contorno que se pretende rellenar.
vec2 packUnit(float v) {
    float scaled = clamp(v, 0.0, 1.0) * 255.0;
    float hi = floor(scaled);
    return vec2(hi / 255.0, scaled - hi);
}

void main() {
    fragDepth = packDepth(clamp(vViewZ / uFar, 0.0, 1.0));
    fragAtlasUv = vec4(packUnit(vAtlasUv.x), packUnit(vAtlasUv.y));

    // Se empaquetan isla (RG) y region (BA) en un mismo adjunto: con una sola
    // lectura de un pixel el motor sabe, al empezar el trazo, en que isla y en
    // que region delimitada a mano se apoyo el lapiz.
    float island = vIsland + 1.0;   // 0 queda reservado para "sin geometria"
    float region = 0.0;
    if (uHasRegions) {
        vec4 c = texture(uRegionMap, vAtlasUv);
        region = floor(c.r * 255.0 + 0.5) + floor(c.g * 255.0 + 0.5) * 256.0;
    }
    fragIds = vec4(
        mod(island, 256.0) / 255.0,
        floor(island / 256.0) / 255.0,
        mod(region, 256.0) / 255.0,
        floor(region / 256.0) / 255.0);
}
)";

// Combina cobertura UV y limite dibujado en un solo mapa reducido, que es lo
// que se lee de vuelta para etiquetar regiones en CPU.
inline constexpr const char* kRegionSourceFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;
uniform sampler2D uCoverage;
uniform sampler2D uBoundary;
uniform vec2 uTexel;   // 1 / resolucion de la mascara de limites

void main() {
    // Se toma el maximo del limite en un vecindario para que una pared fina no
    // se pierda al reducir la resolucion: un hueco de un texel filtraria la
    // pintura al otro lado.
    //
    // Con texture() y un desplazamiento calculado, no con textureOffset(): esa
    // funcion exige un offset constante en tiempo de compilacion y el indice de
    // un bucle no lo es.
    float boundary = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * uTexel;
            boundary = max(boundary, texture(uBoundary, vUv + offset).r);
        }
    }
    fragColor = vec4(texture(uCoverage, vUv).r, boundary, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Pintado por proyeccion.
//
// El truco central: el vertex shader coloca cada vertice en su coordenada UV,
// de modo que rasterizamos el modelo DENTRO del atlas de textura. El fragment
// shader recibe la posicion 3D real de ese texel, la proyecta a pantalla y
// comprueba si el pincel lo toca, si esta tapado por otra parte de la malla y
// si la cara mira hacia la camara.
// ---------------------------------------------------------------------------
inline constexpr const char* kPaintVS = R"(#version 300 es
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in float aIsland;

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vAtlasUv;
flat out float vIsland;

uniform vec2 uUvOffset;   // para trabajar sobre un tile UDIM concreto

void main() {
    vWorldPos = aPosition;
    vWorldNormal = aNormal;
    vIsland = aIsland;
    vec2 uv = aUv - uUvOffset;
    vAtlasUv = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)";

inline constexpr const char* kPaintFS = R"(#version 300 es
precision highp float;

#define MAX_SEGMENTS 48

in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vAtlasUv;
flat in float vIsland;

layout(location = 0) out vec4 fragColor;   // R8: mascara acumulada del trazo

uniform mat4 uViewProj;
uniform mat4 uView;
uniform vec3 uCameraPos;
uniform vec2 uViewportSize;
uniform float uFar;
uniform bool uOrthographic;

uniform sampler2D uSceneDepth;
uniform bool uUseDepthTest;
uniform float uDepthBias;   // en unidades de mundo

uniform bool uBackfaceCull;
uniform float uFacingCutoff;   // por debajo de esto no se pinta nada
uniform float uFacingFull;     // por encima de esto se pinta a full

uniform int uSegmentCount;
uniform vec4 uSegA[MAX_SEGMENTS];   // xy = punto inicial px, z = radio px, w = alfa
uniform vec4 uSegB[MAX_SEGMENTS];   // xy = punto final px,   z = radio px, w = alfa
uniform float uHardness;

// Confinar el trazo a la isla UV donde arranco evita el problema clasico:
// dos zonas que en pantalla se tocan (la crin y el lomo) pero que en el atlas
// son islas distintas, y que al pintar cerca del borde se manchan entre si.
uniform bool uRestrictIsland;
uniform float uIslandId;
uniform bool uFillMode;   // bote de pintura: rellena la isla entera

// Forma de la punta. Es lo que hace que un preajuste sea un pincel distinto y
// no el mismo pincel con otros numeros.
uniform int uTipShape;          // 0 redondo, 1 plano, 2 cuadrado, 3 spray
uniform float uTipAspect;       // 1 = circular; por debajo, punta plana
uniform float uTipAngle;        // radianes
uniform bool uTipFollowsStroke; // la punta plana gira con la direccion del trazo
uniform float uGrainAmount;     // 0 = liso, 1 = todo grano
uniform float uGrainScale;      // celdas de grano a lo ancho del atlas

// Limite manual: mascara pintada a mano que el pincel no cruza.
uniform sampler2D uRegionMap;
uniform bool uRestrictRegion;
uniform float uRegionId;

float unpackDepth(vec4 c) {
    return dot(c, vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 16581375.0));
}

// Ruido estable en el espacio del atlas: el grano queda anclado a la textura,
// asi que repasar la misma zona no lo cambia de sitio.
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

/// Distancia al eje del trazo medida en el espacio de la punta.
float tipDistance(vec2 rel, vec2 dir) {
    float angle = uTipAngle;
    if (uTipFollowsStroke && dot(dir, dir) > 1e-6) angle += atan(dir.y, dir.x);
    float c = cos(-angle);
    float s = sin(-angle);
    vec2 local = vec2(c * rel.x - s * rel.y, s * rel.x + c * rel.y);
    local.y /= max(uTipAspect, 0.05);
    return uTipShape == 2 ? max(abs(local.x), abs(local.y)) : length(local);
}

float regionAt(vec2 uv) {
    // Se guarda en dos canales de 8 bits: hasta 65535 regiones.
    vec4 c = texture(uRegionMap, uv);
    return floor(c.r * 255.0 + 0.5) + floor(c.g * 255.0 + 0.5) * 256.0;
}

void main() {
    if (uRestrictIsland && abs(vIsland - uIslandId) > 0.25) discard;
    // El limite dibujado a mano parte la isla en regiones. Si el trazo empezo
    // en una, no puede saltar a la de al lado aunque el pincel la solape.
    if (uRestrictRegion && abs(regionAt(vAtlasUv) - uRegionId) > 0.5) discard;

    if (uFillMode) {
        // El bote no mira ni a camara ni a oclusion: rellena la isla completa,
        // tambien la parte que en ese momento queda de espaldas.
        fragColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 clip = uViewProj * vec4(vWorldPos, 1.0);
    if (clip.w <= 0.0) discard;

    vec3 ndc = clip.xyz / clip.w;
    vec2 screenUv = ndc.xy * 0.5 + 0.5;
    if (screenUv.x < -0.2 || screenUv.x > 1.2 || screenUv.y < -0.2 || screenUv.y > 1.2) discard;

    vec3 normal = normalize(vWorldNormal);
    vec3 toEye = uOrthographic ? normalize(uCameraPos) : normalize(uCameraPos - vWorldPos);
    float facing = dot(normal, toEye);
    if (uBackfaceCull && facing < uFacingCutoff) discard;

    float facingWeight = uBackfaceCull
        ? smoothstep(uFacingCutoff, uFacingFull, facing)
        : smoothstep(uFacingCutoff, uFacingFull, abs(facing));
    if (facingWeight <= 0.0) discard;

    if (uUseDepthTest) {
        float sceneZ = unpackDepth(texture(uSceneDepth, screenUv)) * uFar;
        float ownZ = -(uView * vec4(vWorldPos, 1.0)).z;
        if (ownZ > sceneZ + uDepthBias) discard;
    }

    // Ojo con el eje Y: MotionEvent mide desde arriba y OpenGL desde abajo.
    // Los segmentos llegan en coordenadas de Android, asi que hay que voltear.
    vec2 pixel = vec2(screenUv.x, 1.0 - screenUv.y) * uViewportSize;

    float mask = 0.0;
    for (int i = 0; i < MAX_SEGMENTS; ++i) {
        if (i >= uSegmentCount) break;
        vec2 a = uSegA[i].xy;
        vec2 b = uSegB[i].xy;
        vec2 pa = pixel - a;
        vec2 ba = b - a;
        float denom = max(dot(ba, ba), 1e-6);
        float t = clamp(dot(pa, ba) / denom, 0.0, 1.0);
        float dist = tipDistance(pa - ba * t, ba);

        float radius = mix(uSegA[i].z, uSegB[i].z, t);
        float alpha = mix(uSegA[i].w, uSegB[i].w, t);

        // Borde interior duro + al menos ~0.75 px de antialias siempre.
        float inner = radius * uHardness;
        float outer = max(radius, inner + 0.75);
        float falloff = 1.0 - smoothstep(inner, outer, dist);
        mask = max(mask, falloff * alpha);
    }

    if (uTipShape == 3) {
        // Spray: la cobertura se rompe en motas, mas densas en el centro.
        float n = hash21(floor(vAtlasUv * uGrainScale));
        mask *= step(1.0 - mask * 0.95, n + 0.05);
    } else if (uGrainAmount > 0.001) {
        float n = hash21(floor(vAtlasUv * uGrainScale));
        mask *= mix(1.0, n, uGrainAmount);
    }

    if (mask <= 0.0015) discard;
    fragColor = vec4(mask * facingWeight, 0.0, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Composicion del trazo sobre la capa.
//
// Se compone SIEMPRE contra la instantanea previa al trazo, no de forma
// incremental. Eso da opacidad constante (no se oscurece donde el trazo se
// cruza consigo mismo) y hace que deshacer sea trivial.
// ---------------------------------------------------------------------------
inline constexpr const char* kStrokeCompositeFS = R"(#version 300 es
precision highp float;

in vec2 vUv;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uBaseTex;      // capa antes de empezar el trazo
uniform sampler2D uStrokeMask;   // mascara acumulada (R8)
uniform sampler2D uPredictedMask;// tramo predicho, se descarta cada frame
uniform bool uUsePredicted;
uniform vec4 uBrushColor;        // rgb + alfa maximo del trazo
uniform int uMode;               // 0 = pintar, 1 = borrar, 2 = difuminar alfa
uniform bool uAlphaLock;         // no crear alfa nueva, solo repintar lo pintado

void main() {
    vec4 base = texture(uBaseTex, vUv);
    float raw = texture(uStrokeMask, vUv).r;
    if (uUsePredicted) raw = max(raw, texture(uPredictedMask, vUv).r);
    float m = clamp(raw * uBrushColor.a, 0.0, 1.0);

    if (uMode == 1) {
        // Borrador: solo come alfa.
        fragColor = vec4(base.rgb, base.a * (1.0 - m));
        return;
    }

    if (uAlphaLock) m *= base.a;

    float outA = m + base.a * (1.0 - m);
    vec3 outRgb = outA > 0.0
        ? (uBrushColor.rgb * m + base.rgb * base.a * (1.0 - m)) / outA
        : base.rgb;
    fragColor = vec4(outRgb, outA);
}
)";

// ---------------------------------------------------------------------------
// Composicion de la pila de capas.
// ---------------------------------------------------------------------------
inline constexpr const char* kLayerCompositeFS = R"(#version 300 es
precision highp float;

in vec2 vUv;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uDst;        // acumulado por debajo
uniform sampler2D uSrc;        // capa a mezclar
uniform float uOpacity;
uniform int uBlendMode;        // 0 normal 1 multiplicar 2 trama 3 superponer
                               // 4 aniadir 5 subexponer 6 sobreexponer 7 luz suave
uniform bool uClipToBelow;     // mascara de recorte

vec3 blend(vec3 d, vec3 s, int mode) {
    if (mode == 1) return d * s;
    if (mode == 2) return 1.0 - (1.0 - d) * (1.0 - s);
    if (mode == 3) return mix(2.0 * d * s, 1.0 - 2.0 * (1.0 - d) * (1.0 - s), step(0.5, d));
    if (mode == 4) return min(d + s, vec3(1.0));
    if (mode == 5) return clamp(d / max(1.0 - s, 1e-4), 0.0, 1.0);
    if (mode == 6) return 1.0 - clamp((1.0 - d) / max(s, 1e-4), 0.0, 1.0);
    if (mode == 7) {
        vec3 t = sqrt(max(d, 0.0));
        return mix(2.0 * d * s + d * d * (1.0 - 2.0 * s),
                   2.0 * d * (1.0 - s) + t * (2.0 * s - 1.0),
                   step(0.5, s));
    }
    return s;
}

void main() {
    vec4 dst = texture(uDst, vUv);
    vec4 src = texture(uSrc, vUv);

    float a = src.a * uOpacity;
    if (uClipToBelow) a *= dst.a;

    vec3 blended = blend(dst.rgb, src.rgb, uBlendMode);
    float outA = a + dst.a * (1.0 - a);
    vec3 outRgb = outA > 0.0
        ? (blended * a + dst.rgb * dst.a * (1.0 - a)) / outA
        : dst.rgb;
    fragColor = vec4(outRgb, outA);
}
)";

// ---------------------------------------------------------------------------
// Dilatacion de costuras. Rellena hacia fuera de las islas UV para que el
// filtrado bilineal no chupe texels vacios en los bordes.
// ---------------------------------------------------------------------------
inline constexpr const char* kDilateFS = R"(#version 300 es
precision highp float;

in vec2 vUv;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uTex;
uniform vec2 uTexelSize;

void main() {
    vec4 c = texture(uTex, vUv);
    if (c.a > 0.001) { fragColor = c; return; }

    vec3 sum = vec3(0.0);
    float weight = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            if (x == 0 && y == 0) continue;
            vec4 s = texture(uTex, vUv + vec2(float(x), float(y)) * uTexelSize);
            if (s.a > 0.001) {
                // Las diagonales pesan menos: evita esquinas cuadradas.
                float w = (x == 0 || y == 0) ? 1.0 : 0.7071;
                sum += s.rgb * w;
                weight += w;
            }
        }
    }

    fragColor = weight > 0.0 ? vec4(sum / weight, 1.0) : c;
}
)";

// ---------------------------------------------------------------------------
// Mascara de cobertura UV: 1 donde hay geometria en el atlas.
// ---------------------------------------------------------------------------
inline constexpr const char* kUvMaskVS = R"(#version 300 es
layout(location = 2) in vec2 aUv;
void main() { gl_Position = vec4(aUv * 2.0 - 1.0, 0.0, 1.0); }
)";

inline constexpr const char* kUvMaskFS = R"(#version 300 es
precision highp float;
layout(location = 0) out vec4 fragColor;
void main() { fragColor = vec4(1.0); }
)";

// ---------------------------------------------------------------------------
// Visor del modelo.
// ---------------------------------------------------------------------------
inline constexpr const char* kModelVS = R"(#version 300 es
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv;

uniform mat4 uViewProj;

void main() {
    vWorldPos = aPosition;
    vNormal = aNormal;
    vUv = aUv;
    gl_Position = uViewProj * vec4(aPosition, 1.0);
}
)";

inline constexpr const char* kModelFS = R"(#version 300 es
precision highp float;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUv;

layout(location = 0) out vec4 fragColor;

uniform sampler2D uBaseColor;
uniform vec3 uCameraPos;
uniform int uViewMode;        // 0 plano 1 PBR 2 matcap 3 cuadricula UV
uniform float uRoughness;
uniform float uMetallic;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientSky;
uniform vec3 uAmbientGround;
uniform float uUvCheckerDensity;
uniform bool uShowUnpaintedTint;
// Sombreado de la vista plana. Es una multiplicacion uniforme sobre los tres
// canales, asi que oscurece pero NO desplaza el tono ni la saturacion: el color
// sigue siendo el que elegiste. A 0 la vista es exacta al pixel.
uniform float uUnlitShading;
// Los limites dibujados a mano se pintan encima del modelo: si no se ven, no
// hay forma de saber por donde pasan ni si quedaron cerrados.
uniform sampler2D uBoundaryMask;
uniform bool uShowBoundary;
uniform vec3 uBoundaryColor;

const float PI = 3.14159265359;

float distributionGGX(float nDotH, float rough) {
    float a = rough * rough;
    float a2 = a * a;
    float d = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-6);
}

float geometrySmith(float nDotV, float nDotL, float rough) {
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float gv = nDotV / (nDotV * (1.0 - k) + k);
    float gl = nDotL / (nDotL * (1.0 - k) + k);
    return gv * gl;
}

vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Matcap procedural: no necesitamos cargar ninguna textura para tener una
// vista de "arcilla" util para juzgar la forma.
vec3 proceduralMatcap(vec3 viewNormal) {
    vec3 key = normalize(vec3(-0.4, 0.75, 0.6));
    vec3 fill = normalize(vec3(0.6, 0.1, 0.5));
    float kd = max(dot(viewNormal, key), 0.0);
    float fd = max(dot(viewNormal, fill), 0.0);
    float rim = pow(1.0 - max(viewNormal.z, 0.0), 2.5);
    float spec = pow(kd, 48.0);
    vec3 base = vec3(0.34, 0.35, 0.38);
    return base * (0.25 + 0.85 * kd) + vec3(0.16, 0.18, 0.22) * fd
         + vec3(1.0) * spec * 0.5 + vec3(0.28, 0.36, 0.5) * rim;
}

// Aplica el limite dibujado sobre el color final, sea cual sea el modo de vista.
vec4 withBoundary(vec4 color) {
    if (!uShowBoundary) return color;
    float wall = texture(uBoundaryMask, vUv).r;
    return vec4(mix(color.rgb, uBoundaryColor, smoothstep(0.15, 0.6, wall)), color.a);
}

void main() {
    vec4 albedo = texture(uBaseColor, vUv);
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uCameraPos - vWorldPos);
    if (!gl_FrontFacing) n = -n;

    if (uViewMode == 3) {
        vec2 g = floor(vUv * uUvCheckerDensity);
        float checker = mod(g.x + g.y, 2.0);
        vec3 c = mix(vec3(0.22, 0.24, 0.28), vec3(0.72, 0.74, 0.80), checker);
        // Tinte de color por cuadrante para leer estiramientos de un vistazo.
        c *= vec3(0.85 + 0.3 * fract(vUv.x * 2.0), 0.9, 0.85 + 0.3 * fract(vUv.y * 2.0));
        float shade = 0.55 + 0.45 * max(dot(n, v), 0.0);
        fragColor = withBoundary(vec4(c * shade, 1.0));
        return;
    }

    if (uViewMode == 2) {
        // Normal en espacio de vista, aproximada con la base de la camara.
        vec3 fwd = -v;
        vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), fwd));
        vec3 up = cross(fwd, right);
        vec3 vn = normalize(vec3(dot(n, right), dot(n, up), dot(n, -fwd)));
        fragColor = withBoundary(vec4(proceduralMatcap(vn), 1.0));
        return;
    }

    vec3 base = albedo.rgb;
    if (uShowUnpaintedTint && albedo.a < 0.004) {
        // Zonas sin pintar: cuadros tenues para que se vea que estan vacias.
        vec2 g = floor(vUv * 64.0);
        base = mix(vec3(0.16), vec3(0.24), mod(g.x + g.y, 2.0));
    }

    if (uViewMode == 0) {
        float ndotv = max(dot(n, v), 0.0);
        float shade = mix(1.0, 0.5 + 0.5 * ndotv, clamp(uUnlitShading, 0.0, 1.0));
        fragColor = withBoundary(vec4(base * shade, 1.0));
        return;
    }

    // PBR: una direccional mas ambiente hemisferico. Suficiente para juzgar
    // volumen sin montar todavia un IBL completo.
    vec3 l = normalize(-uLightDir);
    vec3 h = normalize(v + l);
    float nDotV = max(dot(n, v), 1e-4);
    float nDotL = max(dot(n, l), 0.0);
    float nDotH = max(dot(n, h), 0.0);
    float vDotH = max(dot(v, h), 0.0);

    float rough = clamp(uRoughness, 0.045, 1.0);
    vec3 f0 = mix(vec3(0.04), base, uMetallic);
    vec3 f = fresnelSchlick(vDotH, f0);
    float ndf = distributionGGX(nDotH, rough);
    float g = geometrySmith(nDotV, nDotL, rough);

    vec3 specular = (ndf * g * f) / max(4.0 * nDotV * nDotL, 1e-4);
    vec3 kd = (vec3(1.0) - f) * (1.0 - uMetallic);
    vec3 direct = (kd * base / PI + specular) * uLightColor * nDotL;

    float hemi = n.y * 0.5 + 0.5;
    vec3 ambient = mix(uAmbientGround, uAmbientSky, hemi) * base * (1.0 - uMetallic * 0.6);

    vec3 color = direct + ambient;
    color = color / (color + vec3(1.0));                  // tonemap Reinhard
    color = pow(color, vec3(1.0 / 2.2));                  // a sRGB
    fragColor = withBoundary(vec4(color, 1.0));
}
)";

// ---------------------------------------------------------------------------
// Malla de alambre superpuesta.
// ---------------------------------------------------------------------------
inline constexpr const char* kWireframeFS = R"(#version 300 es
precision highp float;
layout(location = 0) out vec4 fragColor;
uniform vec4 uColor;
void main() { fragColor = uColor; }
)";

// ---------------------------------------------------------------------------
// Fondo del visor: degradado vertical suave.
// ---------------------------------------------------------------------------
inline constexpr const char* kBackgroundFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;
uniform vec3 uTopColor;
uniform vec3 uBottomColor;
void main() {
    vec3 c = mix(uBottomColor, uTopColor, smoothstep(0.0, 1.0, vUv.y));
    // Vinieta suave para que el modelo destaque.
    vec2 d = vUv - 0.5;
    c *= 1.0 - 0.35 * dot(d, d);
    fragColor = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Cursor del pincel (anillo en espacio de pantalla).
// ---------------------------------------------------------------------------
inline constexpr const char* kBrushCursorFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;

uniform vec2 uViewportSize;
uniform vec2 uCenter;      // px, en coordenadas de Android (origen arriba)
uniform float uRadius;     // px
uniform float uInnerScale; // dureza -> radio del anillo interior
uniform vec4 uColor;

void main() {
    vec2 p = vec2(vUv.x, 1.0 - vUv.y) * uViewportSize;
    float d = length(p - uCenter);

    float outer = 1.0 - smoothstep(uRadius - 1.2, uRadius + 0.6, d);
    float outerHole = 1.0 - smoothstep(uRadius - 2.6, uRadius - 1.4, d);
    float ring = clamp(outer - outerHole, 0.0, 1.0);

    float ri = uRadius * uInnerScale;
    float innerRing = 0.0;
    if (uInnerScale < 0.97 && ri > 2.0) {
        float a = 1.0 - smoothstep(ri - 1.0, ri + 0.5, d);
        float b = 1.0 - smoothstep(ri - 2.0, ri - 1.1, d);
        innerRing = clamp(a - b, 0.0, 1.0) * 0.5;
    }

    float alpha = max(ring, innerRing) * uColor.a;
    if (alpha <= 0.002) discard;
    fragColor = vec4(uColor.rgb, alpha);
}
)";

// ---------------------------------------------------------------------------
// Cuerda del regulador de trazo.
//
// El regulador funciona como si del lapiz colgara una cuerda de longitud fija y
// el pincel fuera el peso del otro extremo: el pincel solo se mueve cuando la
// cuerda se tensa, y por eso el temblor por debajo de esa longitud se descarta
// entero. Dibujarla no es adorno: sin verla no se entiende por que el trazo
// sale con retraso respecto a la punta.
// ---------------------------------------------------------------------------
inline constexpr const char* kRopeFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;

uniform vec2 uViewportSize;
uniform vec2 uAnchor;   // px, donde cae la pintura
uniform vec2 uTip;      // px, la punta del lapiz
uniform vec4 uColor;

void main() {
    vec2 p = vec2(vUv.x, 1.0 - vUv.y) * uViewportSize;

    // Distancia al segmento anclaje-punta.
    vec2 ba = uTip - uAnchor;
    vec2 pa = p - uAnchor;
    float len2 = max(dot(ba, ba), 1e-4);
    float t = clamp(dot(pa, ba) / len2, 0.0, 1.0);
    float line = length(pa - ba * t);

    // La cuerda se dibuja fina y con la punta un poco mas marcada, para que se
    // lea sobre cualquier fondo sin tapar lo que hay debajo.
    float rope = 1.0 - smoothstep(0.6, 1.8, line);

    float dTip = length(p - uTip);
    float ring = clamp((1.0 - smoothstep(3.2, 4.4, dTip)) -
                       (1.0 - smoothstep(2.0, 3.0, dTip)), 0.0, 1.0);

    float alpha = max(rope * 0.65, ring) * uColor.a;
    if (alpha <= 0.002) discard;
    fragColor = vec4(uColor.rgb, alpha);
}
)";

// ---------------------------------------------------------------------------
// Vista 2D del atlas UV (lienzo plano) con la malla superpuesta.
// ---------------------------------------------------------------------------
inline constexpr const char* kCanvas2DFS = R"(#version 300 es
precision highp float;
in vec2 vUv;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uTexture;
uniform vec2 uPan;
uniform float uZoom;
uniform float uAspect;
uniform float uCheckerSize;

void main() {
    vec2 uv = (vUv - 0.5) * vec2(uAspect, 1.0) / uZoom + 0.5 + uPan;

    vec2 g = floor(vUv * uCheckerSize);
    vec3 checker = mix(vec3(0.20), vec3(0.26), mod(g.x + g.y, 2.0));

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        fragColor = vec4(checker * 0.5, 1.0);
        return;
    }

    vec4 c = texture(uTexture, uv);
    fragColor = vec4(mix(checker, c.rgb, c.a), 1.0);
}
)";

}  // namespace uvp::shaders
