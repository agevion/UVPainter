# UVPainter

Pintura de texturas directamente sobre el modelo 3D, para Android, pensada para
tablet con lápiz. Desarrollada y probada en una **Galaxy Tab S7 (SM-T870)** con
S-Pen.

Estado actual: **vertical slice funcionando en dispositivo**. Carga un modelo,
lo orbitas con los dedos, pintas con el S-Pen sobre la superficie con oclusión
correcta, gestionas capas y exportas el PNG.

---

## Compilar y ejecutar

Requiere JDK 17+ (sirve el que trae Android Studio) y el SDK de Android con
NDK 28.2 y CMake 3.22.1. La ruta del SDK va en `local.properties`.

```bash
./gradlew :app:assembleDebug
```

Instalar en la tablet conectada por USB con depuración activada:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

En PowerShell hay que exportar `JAVA_HOME` antes, porque no está en el sistema:

```bash
$env:JAVA_HOME="C:\Program Files\Android\Android Studio\jbr"; ./gradlew.bat :app:assembleDebug
```

Para probar el trazo sin lápiz (pintar con el dedo), se puede arrancar así:

```bash
adb shell am start -n com.uvpainter/.MainActivity --ez finger_paint true
```

Ese mismo ajuste está en el panel de Pincel como «El dedo también pinta».

La app arranca con un modelo de ejemplo incluido (`WaterBottle.glb`) para poder
probar sin importar nada. Ver [Créditos](#créditos).

---

## Cómo se usa

| Acción | Gesto |
| --- | --- |
| Pintar | S-Pen sobre el modelo |
| Orbitar | Dos dedos, arrastrar |
| Zoom | Dos dedos, pellizco |
| Girar la vista | Dos dedos, giro |
| Desplazar | Tres dedos, arrastrar |
| Deshacer | Toque con dos dedos |
| Rehacer | Toque con tres dedos |
| Ocultar la interfaz | Toque con cuatro dedos |
| Borrador temporal | Botón lateral del S-Pen |

### Idiomas

La interfaz habla castellano, inglés, francés, italiano y alemán. Se cambia en
el panel de documento (el engranaje de la barra), lo primero de todo: quien abre
la app en una lengua que no es la suya no puede leer el resto del panel para
encontrar el ajuste. El cambio es inmediato y no recrea la actividad, así que se
puede probar a media lámina sin perder nada.

Mientras nadie elija, se sigue al idioma del sistema si es uno de esos cinco;
si no, castellano. En cuanto se toca la lista, manda la elección y se recuerda
entre sesiones.

Los rótulos no viven en `res/values/strings.xml` sino en `i18n/`, en tablas de
Kotlin: un recurso de Android obliga a recrear la actividad para cambiar de
idioma, y aquí eso significaría tirar el contexto GL y con él el historial. Cada
tabla es un `Strings` completo, así que dejarse un texto al traducir no compila.
Los mensajes de error suben del motor como clave (`err.glb_truncated`) y se
traducen en Kotlin: un motor que hablara castellano dejaría media app sin
traducir justo cuando algo falla.

### Rechazo de palma

Enrutar por tipo de herramienta no basta: al apoyar la mano, el canto de la
palma suele tocar la pantalla **antes** que la punta del lápiz, así que en ese
instante no hay ningún lápiz "cerca" con el que descartarla. Se combinan cuatro
defensas, todas ajustables en el panel del lápiz:

1. **Dos dedos para navegar.** Una palma apoyada es una mancha, no dos contactos
   deliberados. Es la defensa que más casos cubre.
2. **Filtro por tamaño de contacto.** El canto de la mano deja una huella mucho
   más ancha que un dedo; `touchMajor` la delata (umbral por defecto 17 mm).
3. **Proximidad del lápiz.** Mientras el S-Pen está apoyado o flotando en rango,
   ningún contacto táctil llega ni al pincel ni a la cámara.
4. **Ventana de armado.** Un contacto no mueve la cámara hasta que lleva 70 ms
   en pantalla, y si el lápiz aterriza dentro de esa ventana los contactos
   previos se descartan retroactivamente: era la mano acomodándose.

Además se atiende a `ACTION_CANCEL` y a `FLAG_CANCELED`, que es como Android
avisa *a posteriori* de que un contacto era la palma; en ese caso el trazo se
revierte entero. Cuando algo se descarta, la app lo dice en pantalla: sin esa
señal no se distingue "te estoy ignorando a propósito" de "no funciono".

Para quien apoye la mano entera hay además un **modo solo lápiz** que ignora
todo el táctil.

### Pintar sin invadir la zona de al lado

Al cargar el modelo se agrupan los vértices en **islas UV** (union-find sobre los
índices: tanto glTF como OBJ duplican los vértices en las costuras, así que dos
triángulos que comparten índice comparten continuidad en el atlas).

Con eso, el pincel puede confinarse a la isla donde arrancó el trazo. Es lo que
resuelve el caso clásico: la crin y el lomo se tocan en pantalla, pero en el
atlas son islas distintas, y sin esto pintar cerca del borde las mancha entre
sí. Con la restricción activa puedes empezar donde querías y seguir sin mirar.

El mismo mapa alimenta el **bote de pintura**, que rellena la isla completa —
también la parte que en ese momento queda de espaldas — y se detiene exacto en
la costura.

Las costuras se dibujan en naranja sobre el modelo (aristas usadas por un solo
triángulo, que es justo por donde se cortó al desplegar).

### Límites dibujados a mano

Cuando las UVs del modelo no están bien cortadas y dos zonas que deberían ser
independientes comparten isla, se puede **trazar una pared a mano** sobre la
superficie (herramienta Formas → Trazar límite). Se dibuja en verde encima del
modelo para que se vea por dónde pasa y si quedó cerrada.

Por debajo, esa pared se etiqueta en regiones: se reducen la cobertura UV y la
máscara de límites a un mapa de 1024², se etiquetan las componentes conexas en
CPU y el resultado vuelve a GPU como identificador de región. El prepaso escribe
isla y región en el mismo adjunto, así que al apoyar el lápiz basta leer un píxel
para saber en qué región empieza el trazo. El pincel descarta después todo
fragmento de otra región.

El efecto es el que se pide de forma natural: si empiezas dentro de una zona
cerrada, el pincel no sale de ella; si empiezas fuera, no entra.

### Formas

Línea, rectángulo, elipse y polígono de N lados. Se apoya el lápiz donde empieza
la figura y se arrastra: la vista previa se rehace entera en cada frame, y al
levantar se fija. Van a grosor y opacidad constantes a propósito — una recta que
adelgaza donde apretaste menos no es una recta.

---

## Arquitectura

```
app/src/main/
  cpp/                  motor: C++20 sobre OpenGL ES 3.0+
    core/Math.h          álgebra lineal (column-major, compatible con GLSL)
    gl/                  envoltorios RAII de shader, textura, FBO, malla; contexto EGL
    io/                  parser JSON propio + cargador GLB/glTF/OBJ, sin dependencias
    scene/Camera         cámara de órbita
    paint/               documento, capas, motor de pintado, historial
    render/              shaders y visor
    engine/Engine        fachada; cola de entrada con mutex
    JniBridge.cpp        puente con Kotlin
  java/com/uvpainter/
    engine/              NativeBridge, RenderThread, PainterSurfaceView (S-Pen)
    state/               PainterController y estado de Compose
    i18n/                rótulos de la interfaz en los cinco idiomas
    ui/                  interfaz en Compose
    io/TextureIo         PNG y SAF vía APIs de Android
```

### Por qué Kotlin + C++ y no Flutter

El motivo es el lápiz. Solo en Android nativo hay acceso a `MotionEvent` crudo
con presión, inclinación y orientación, a los **puntos históricos** (el S-Pen
muestrea muy por encima de la tasa de refresco y descartarlos convierte las
curvas rápidas en polígonos), a `requestUnbufferedDispatch()` y a las dos
librerías que existen justo para esto:

- `androidx.graphics:graphics-core` — renderizado en front-buffer
- `androidx.input:input-motionprediction` — extrapolación del trazo

### Cómo funciona el pintado

El truco central está en `kPaintVS` / `kPaintFS`: **el vertex shader coloca cada
vértice en su coordenada UV**, de modo que se rasteriza el modelo *dentro* del
atlas de textura. El fragment shader recibe entonces, para cada texel, su
posición 3D real; la proyecta a pantalla y comprueba tres cosas:

1. si el pincel lo toca (distancia a la cápsula del segmento del trazo),
2. si está tapado por otra parte de la malla (prepaso de profundidad),
3. si la cara mira a la cámara (desvanecido suave en los bordes).

Detalles que importan y que son fáciles de hacer mal:

- **La profundidad se compara en espacio lineal**, empaquetada en RGBA8, no
  sampleando el z-buffer. Con un `near/far` muy dispar, deslinealizar el z-buffer
  en el shader pierde tanta precisión que el test descarta todos los fragmentos.
- **El trazo se compone siempre contra una instantánea** tomada al empezar, no de
  forma incremental. Así la opacidad es constante aunque el trazo se cruce
  consigo mismo, y deshacer se reduce a restaurar un rectángulo.
- **La acumulación usa `GL_MAX`**, no suma, por el mismo motivo.
- **La predicción va a una máscara aparte** que se descarta en cada frame, así que
  jamás queda grabada: se gana latencia percibida sin arriesgar pintura donde no
  tocaba.
- **El historial guarda solo el rectángulo tocado**, hallado con una reducción por
  máximos en GPU hasta 64×64 y una única lectura de 16 KB por trazo. Guardar la
  capa entera por paso reventaría la memoria en cuanto el atlas pasa de 2K.
- **Dilatación de costuras** tras componer, para que el filtrado bilineal no chupe
  texels vacíos en los bordes de las islas UV.

### Orientación de los modelos

Cada pipeline usa ejes distintos (Blender y 3ds Max son Z arriba, Unity y glTF
son Y arriba) y además el sentido puede venir invertido. Al importar se aplica
una heurística: los ejes horizontales suelen quedar centrados en el origen y el
vertical claramente a un lado, así que se mide esa asimetría y se elige el
sentido que deja el modelo por encima del plano del origen. Es una suposición
razonable, no una certeza, así que el ajuste está también en el panel de
Documento (`Y arriba` / `Z arriba` / `Invertir` / `Girar 90°`).

---

## Formatos

| Entrada | Estado |
| --- | --- |
| `.glb` | Sí |
| `.gltf` con buffers embebidos o data URI | Sí |
| `.obj` (+ grupos `usemtl`) | Sí |
| `.gltf` con `.bin` externo | No (exporta como `.glb`) |
| `.fbx` | No todavía — ver hoja de ruta |

La salida es PNG a `Descargas/UVPainter`, con dilatación de costuras aplicada.

---

## Notas sobre el color

El color que ves pintado solo coincide **exactamente** con el del selector en la
vista **Plano**. Los modos PBR y matcap pasan por iluminación, tonemap y gamma,
así que desplazan el tono; sirven para juzgar volumen, no color.

La vista plana lleva un control de **volumen** que multiplica los tres canales
por igual: oscurece en los bordes para que se lea la forma sin tocar el tono ni
la saturación. A 0 la vista es exacta al píxel.

## Limitaciones conocidas

- Solo se pinta el canal de **color base**. El resto de canales PBR están
  diseñados pero no implementados.
- No hay **guardado de proyecto**: al cerrar se pierden capas y límites. Exporta
  el PNG antes de salir.
- Las puntas son procedurales (redonda, plana, cuadrada, spray, con grano). No
  se pueden **importar sellos de alfa** desde PNG todavía.
- Los límites dibujados a mano **no entran en el historial** de deshacer; se
  quitan con «Borrar límites».
- El etiquetado de regiones trabaja a 1024², así que la pared tiene una
  precisión de un par de texels. Para una frontera trazada a mano sobra, pero no
  sirve para separar detalles finísimos.
- Solo el **tile UDIM 0-1**. Si el modelo trae UVs fuera de ese rango, la app
  avisa en el panel de Documento.
- No hay **guardado de proyecto** todavía: al cerrar se pierden las capas. Exporta
  el PNG antes de salir.
- La composición de capas y la del trazo recorren el atlas entero en cada frame.
  A 2048² sobra de largo en un Adreno 650; a 4096² con muchas capas conviene el
  recorte por rectángulo sucio que está en la hoja de ruta.

Ver [DISENO.md](DISENO.md) para el plan completo.

---

## Créditos

El modelo de ejemplo que trae la app, `app/src/main/assets/models/WaterBottle.glb`,
procede de los [glTF Sample Assets](https://github.com/KhronosGroup/glTF-Sample-Assets)
de Khronos: *WaterBottle*, © 2017 Microsoft, publicado bajo
[CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/) (dominio
público). Se ha modificado para quitarle las texturas incrustadas, que la app no
usa porque pinta las suyas: de 8,6 MB a 147 KB, conservando geometría y UVs.
