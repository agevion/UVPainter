# UVPainter — diseño y hoja de ruta

Este documento recoge **qué necesita de verdad una app de pintura de texturas
3D** y en qué orden conviene construirlo. Sale de mirar cómo resuelven esto
Substance 3D Painter, 3D-Coat, Blender (texture paint), ArmorPaint, Procreate,
Clip Studio Paint y Nomad Sculpt, y de lo que la documentación de Android indica
para stylus.

Marcado con ✅ lo que ya está, 🔨 lo que está a medias y ⬜ lo pendiente.

---

## Fase 0 — Núcleo (hecho)

- ✅ Motor C++ sobre GLES 3.0+, contexto EGL propio en hilo de render dedicado
- ✅ Carga de GLB / glTF / OBJ sin dependencias externas
- ✅ Cámara de órbita con encuadre automático
- ✅ Pintado por proyección con oclusión y desvanecido por orientación de cara
- ✅ Capas con opacidad, visibilidad, bloqueo, bloqueo de alfa, máscara de recorte
- ✅ 8 modos de fusión (normal, multiplicar, trama, superponer, añadir, sub/sobre-exponer, luz suave)
- ✅ Deshacer/rehacer por rectángulo sucio, con presupuesto de memoria
- ✅ Dilatación de costuras
- ✅ Modos de visor: plano, PBR, matcap procedural, cuadrícula UV, malla superpuesta
- ✅ Exportación PNG a Descargas
- ✅ Importar imagen a la capa activa
- ✅ Corrección de orientación del modelo
- ✅ Cursor del pincel con vista previa en hover

### Islas UV (hecho)

- ✅ Agrupación de vértices en islas UV por union-find
- ✅ Identificador de isla escrito en el prepaso y leído bajo el cursor
- ✅ Pincel confinado a la isla donde arranca el trazo
- ✅ Bote de pintura por isla, con dilatado de borde
- ✅ Costuras UV resaltadas en naranja sobre el modelo

### Entrada (hecho)

- ✅ Enrutado por tipo de herramienta: lápiz pinta, dedo navega
- ✅ Rechazo de palma en cuatro capas: dos dedos para navegar, filtro por tamaño
      de contacto, proximidad del lápiz y ventana de armado retroactiva
- ✅ Modo solo lápiz
- ✅ Aviso en pantalla cuando se descarta un apoyo
- ✅ Regularizador de trazo de cuerda, estilo Sketchbook
- ✅ Respuesta a presión con ganancia y curva ajustables
- ✅ Rechazo de palma real, incluido `FLAG_CANCELED` con reversión del trazo
- ✅ Presión, inclinación y orientación del S-Pen
- ✅ Puntos históricos (imprescindible: el S-Pen muestrea por encima del refresco)
- ✅ `requestUnbufferedDispatch()` al empezar el trazo
- ✅ Predicción de movimiento sin contaminar la textura
- ✅ Estabilizador de trazo
- ✅ Gestos: órbita, desplazamiento, zoom, giro, deshacer/rehacer por toque
- ✅ Botón del S-Pen como borrador temporal

---

## Fase 1 — Lo que falta para usarlo en serio

Prioridad alta. Sin esto la app se queda en demo.

- ⬜ **Guardado de proyecto** (`.uvp`): malla, capas comprimidas, pinceles,
      historial y cámara, con autoguardado y recuperación tras cierre inesperado.
      Es lo más urgente: ahora mismo cerrar la app pierde el trabajo.
- ✅ **Miniaturas de capa** (lectura reducida a 96×96, no la capa entera)
- ✅ **Formas geométricas**: línea, rectángulo, elipse y polígono de N lados
- ✅ **Límites manuales de pintura** con etiquetado por regiones, visibles en
      verde sobre el modelo
- ✅ **Imagen de referencia** flotante, arrastrable, con cuentagotas
- ✅ **Puntas procedurales**: redonda, plana orientable, cuadrada y spray, con
      grano anclado al atlas
- ⬜ **Sellos de alfa importables** desde PNG, para puntas arbitrarias
- ⬜ **Simetría** en X/Y/Z sobre el espacio del modelo.
- ⬜ **Vista 2D del atlas UV** como lienzo plano, con la malla superpuesta y
      sincronizada con la vista 3D. El shader `kCanvas2DFS` ya está escrito.
- ⬜ **Recorte por rectángulo sucio** en la composición, para que 4096² con
      muchas capas siga yendo suave.
- ⬜ **Miniaturas de capa** en el panel.
- ⬜ **Renombrar capas** y reordenarlas arrastrando.

---

## Fase 2 — Pincel y color

- ⬜ **Sellos de pincel** desde PNG (alfa importable) y pinceles procedurales
- ⬜ **Grano y textura** de trazo
- ⬜ **Dispersión, jitter** de tamaño, ángulo, color y posición
- ⬜ **Curvas de respuesta editables** (presión → tamaño/opacidad/flujo) con
      editor de curva, en vez de un solo exponente
- ⬜ **Velocidad → tamaño** (afilado de línea) y **entrada/salida progresiva**,
      que es lo que da el remate limpio a la línea de entintado
- ⬜ **Aerógrafo** con acumulación por tiempo
- ⬜ **Difuminar, emborronar y clonar**
- ⬜ **Reglas y guías**: línea recta, formas, perspectiva, simetría radial
- ⬜ **Armonías de color**, paletas guardables, mapas de degradado
- ⬜ **Gestión de pinceles**: guardar, importar y exportar `.uvbrush`

---

## Fase 3 — Texturizado de verdad

- ⬜ **Múltiples canales**: color base, normal, rugosidad, metalicidad, oclusión,
      emisión. El sistema de capas ya está preparado; falta la pila por canal y
      el visor consumiéndolos.
- ⬜ **Múltiples materiales / submallas** con un atlas por material
- ⬜ **UDIM**: varios tiles UV (`uUvOffset` ya está en el shader de pintado)
- ⬜ **Horneado de mapas**: oclusión ambiental, curvatura, grosor, posición,
      normal de mundo. Es lo que desbloquea el flujo tipo Substance.
- ⬜ **Máscaras generadoras** a partir de esos horneados (bordes desgastados,
      suciedad en cavidades)
- ⬜ **Máscaras de capa** propiamente dichas, con su propio lienzo
- ⬜ **Capas de relleno** con máscara, no destructivas
- ⬜ **Filtros y ajustes**: niveles, curvas, tono/saturación, desenfoque, ruido
- ⬜ **Exportar PSD con capas**, que es lo que hace que se pueda seguir el trabajo
      en el escritorio
- ⬜ **Presets de exportación** por motor (Unity URP/HDRP, Unreal, glTF)
- ⬜ **FBX** a la entrada. Implica traer Assimp por el NDK; conviene decidirlo
      aparte porque son unos 10 MB de APK y una dependencia grande.

---

## Fase 4 — Extensibilidad

Este es el punto que pediste explícitamente. El plan es de tres capas:

1. **Formatos de datos abiertos.** Pinceles (`.uvbrush`), materiales y paletas
   como ZIP con un manifiesto JSON y sus recursos. Importables desde el
   selector de archivos. No requiere ejecutar código de terceros, así que es
   seguro por construcción.

2. **Filtros y generadores en GLSL.** El usuario aporta un fragment shader con
   una firma fija y unos uniformes declarados en el manifiesto; la app le genera
   los deslizadores automáticamente. Es lo más potente por esfuerzo y sigue
   siendo seguro: un shader no puede salirse de su caja de arena.

3. **Scripting.** Un intérprete embebido (QuickJS encaja bien: pequeño, rápido y
   sin dependencias) con una API estable para crear capas, aplicar filtros,
   registrar herramientas y declarar paneles de interfaz en JSON.

El objetivo es que **todo lo interno se construya sobre esa misma API**, de modo
que un plugin de terceros no sea un ciudadano de segunda. Descartado el modelo
de plugins como APK aparte: mucha complejidad y un agujero de seguridad.

---

## Fase 5 — Comodidad y remate

- ⬜ **Menú radial** bajo el pulgar, configurable
- ⬜ **Reasignación de gestos** y de los botones del S-Pen
- ⬜ **S Pen Remote SDK**: el S-Pen de la Tab S7 tiene Bluetooth y Air Actions;
      se pueden mapear a deshacer, cambiar de pincel o girar la vista sin tocar
      la pantalla
- ⬜ **Atajos de teclado** para la Book Cover Keyboard
- ⬜ **Modo zurdo** (interfaz reflejada)
- ⬜ **Paneles acoplables** opcionales para quien quiera más densidad
- ⬜ **Panel de referencia** flotante, con cuentagotas sobre la imagen
- ⬜ **HDRI** importable y luces de estudio orientables
- ⬜ **Biblioteca de matcaps**
- ⬜ **Plano de corte** y aislar/ocultar partes de la malla
- ⬜ **Time-lapse** del proceso, exportable a vídeo
- ⬜ **Localización** español e inglés (ahora está todo en español)
- ⬜ **Onboarding** y HUD de rendimiento opcional

---

## Notas de rendimiento

La Tab S7 lleva Snapdragon 865+ con Adreno 650: GLES 3.2, Vulkan 1.1 y compute
shaders. Margen de sobra para 2048², y 4096² es viable con el recorte por
rectángulo sucio de la fase 1.

El presupuesto real es la **memoria**, no el relleno. Una capa a 4096² RGBA8 son
64 MB; diez capas son 640 MB. Cuando se llegue ahí hay que pasar a
almacenamiento por tiles disperso con compresión de los tiles inactivos, que es
lo que hacen las apps profesionales. La reducción por máximos que ya usa el
historial sirve igual para saber qué tiles están tocados.

## Decisiones tomadas

| Tema | Decisión | Motivo |
| --- | --- | --- |
| Stack | Kotlin + C++ nativo | Acceso crudo al S-Pen y a las librerías de baja latencia |
| minSdk | 29 | Suelo de `SurfaceControl` y del render en front-buffer |
| ABI | solo arm64-v8a | Es lo que lleva la tablet y lo que exige Play |
| GLSL | `#version 300 es` | Funciona aunque el driver devuelva un contexto ES 3.0 |
| Dependencias C++ | ninguna | Parsers propios de JSON, glTF y OBJ; imágenes vía APIs de Android |
| Canales | PBR genérico | Decisión del usuario; el visor plano cubre el trabajo de color liso |
| Interfaz | tipo Procreate | Lienzo casi a pantalla completa, gestos como ley |
