#include "engine/Engine.h"

#include <android/native_window.h>
#include <sys/resource.h>
#include <zlib.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

#include "core/Log.h"
#include "io/ModelLoader.h"

namespace uvp {

// ---------------------------------------------------------------------------
// Archivo de proyecto (.uvp)
//
// Todo binario y en un solo archivo: el modelo tal y como se importo, las capas
// con sus pixeles y la pared dibujada a mano. Los pixeles van por zlib porque
// una capa de dibujo es casi toda transparente o color plano y baja de 16 MB a
// unos pocos cientos de KB.
// ---------------------------------------------------------------------------
namespace {

constexpr char kProjectMagic[8] = {'U', 'V', 'P', 'A', 'I', 'N', 'T', '\0'};
constexpr uint32_t kProjectVersion = 1;
/// Tope de cordura al leer: evita que un archivo corrupto pida 4 GB.
constexpr uint64_t kMaxBlob = 512ull * 1024ull * 1024ull;

bool writeRaw(FILE* f, const void* data, size_t size) {
    return size == 0 || std::fwrite(data, 1, size, f) == size;
}
bool writeU32(FILE* f, uint32_t v) { return writeRaw(f, &v, sizeof(v)); }
bool writeU64(FILE* f, uint64_t v) { return writeRaw(f, &v, sizeof(v)); }
bool writeF32(FILE* f, float v) { return writeRaw(f, &v, sizeof(v)); }
bool writeString(FILE* f, const std::string& s) {
    return writeU32(f, static_cast<uint32_t>(s.size())) && writeRaw(f, s.data(), s.size());
}

/// Comprime un bloque. Separado de la escritura porque comprimir es lo unico
/// caro de guardar, y separandolo se puede repartir entre varios hilos.
bool packBlob(const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    out.clear();
    if (size == 0) return true;
    uLongf packedSize = compressBound(static_cast<uLong>(size));
    out.resize(packedSize);
    if (compress2(out.data(), &packedSize, data, static_cast<uLong>(size), 6) != Z_OK) {
        out.clear();
        return false;
    }
    out.resize(packedSize);
    return true;
}

/// Escribe un bloque ya comprimido. `rawSize` es lo que ocupaba sin comprimir,
/// que es lo que necesita la lectura para reservar sitio.
bool writePacked(FILE* f, size_t rawSize, const std::vector<uint8_t>& packed) {
    if (rawSize == 0) return writeU64(f, 0) && writeU64(f, 0);
    return writeU64(f, rawSize) && writeU64(f, packed.size()) &&
           writeRaw(f, packed.data(), packed.size());
}

/// Un trozo por comprimir y donde dejar el resultado.
struct PackJob {
    const uint8_t* data = nullptr;
    size_t size = 0;
    std::vector<uint8_t>* out = nullptr;
};

/**
 * Comprime todos los trozos a la vez, uno por hilo.
 *
 * Los hilos se bajan a prioridad de fondo a proposito: en un movil eso los
 * manda a los nucleos pequeños y deja los grandes para quien esta dibujando.
 * Guardar tarda algo mas y no se nota nada, que es justo lo que se busca.
 */
bool packAll(std::vector<PackJob>& jobs) {
    if (jobs.empty()) return true;

    std::atomic<size_t> next{0};
    std::atomic<bool> ok{true};
    auto run = [&jobs, &next, &ok]() {
        setpriority(PRIO_PROCESS, 0, 10);  // THREAD_PRIORITY_BACKGROUND
        for (;;) {
            const size_t index = next.fetch_add(1);
            if (index >= jobs.size()) return;
            const PackJob& job = jobs[index];
            if (!packBlob(job.data, job.size, *job.out)) ok.store(false);
        }
    };

    unsigned int cores = std::thread::hardware_concurrency();
    if (cores == 0) cores = 2;
    // Uno menos que nucleos hay: el que falta es el que sigue dibujando.
    const size_t threads =
        std::min<size_t>(jobs.size(), std::max<unsigned int>(cores - 1u, 1u));

    std::vector<std::thread> pool;
    pool.reserve(threads - 1);
    for (size_t i = 1; i < threads; ++i) pool.emplace_back(run);
    run();
    for (std::thread& t : pool) t.join();
    return ok.load();
}

bool readRaw(FILE* f, void* data, size_t size) {
    return size == 0 || std::fread(data, 1, size, f) == size;
}
bool readU32(FILE* f, uint32_t& v) { return readRaw(f, &v, sizeof(v)); }
bool readU64(FILE* f, uint64_t& v) { return readRaw(f, &v, sizeof(v)); }
bool readF32(FILE* f, float& v) { return readRaw(f, &v, sizeof(v)); }

bool readString(FILE* f, std::string& s) {
    uint32_t size = 0;
    if (!readU32(f, size) || size > 4096) return false;
    s.assign(size, '\0');
    return readRaw(f, s.data(), size);
}

bool readBlob(FILE* f, std::vector<uint8_t>& out) {
    uint64_t rawSize = 0;
    uint64_t packedSize = 0;
    if (!readU64(f, rawSize) || !readU64(f, packedSize)) return false;
    if (rawSize > kMaxBlob || packedSize > kMaxBlob) return false;
    out.clear();
    if (rawSize == 0) return true;

    std::vector<uint8_t> packed(static_cast<size_t>(packedSize));
    if (!readRaw(f, packed.data(), packed.size())) return false;

    out.resize(static_cast<size_t>(rawSize));
    uLongf produced = static_cast<uLongf>(rawSize);
    if (uncompress(out.data(), &produced, packed.data(),
                   static_cast<uLong>(packedSize)) != Z_OK ||
        produced != rawSize) {
        out.clear();
        return false;
    }
    return true;
}

}  // namespace

Engine::~Engine() {
    // El hilo que escribe el punto de control sigue leyendo del motor: hay que
    // esperarlo antes de que estos miembros dejen de existir.
    if (checkpoint_.worker.joinable()) checkpoint_.worker.join();
    onSurfaceDestroyed();
}

// ---------------------------------------------------------------------------
// Superficie
// ---------------------------------------------------------------------------
bool Engine::onSurfaceCreated(ANativeWindow* window) {
    if (window_ != nullptr && window_ != window) {
        ANativeWindow_release(window_);
    }
    window_ = window;
    if (!context_.initialize(window)) return false;

    if (!renderer_.initialize()) {
        LOGE("No se pudo inicializar el visor");
        return false;
    }
    if (!paintEngine_.initialize()) {
        LOGE("No se pudo inicializar el motor de pintado");
        return false;
    }
    resourcesReady_ = true;

    viewportW_ = std::max(context_.width(), 1);
    viewportH_ = std::max(context_.height(), 1);
    camera_.setViewport(viewportW_, viewportH_);
    paintEngine_.resizeViewport(viewportW_, viewportH_);

    // Si el contexto se perdio (la app en segundo plano, o simplemente abrir el
    // selector de archivos) hay que rehacer todo lo que vivia en GPU: la malla
    // se vuelve a subir desde la copia en CPU y las capas desde la instantanea
    // que se tomo al destruir la superficie. Sin esto se vuelve con la interfaz
    // encima de un fondo vacio, que es exactamente lo que parece un cuelgue.
    if (!sourceMesh_.empty()) rebuildOrientedMesh(/*frameCamera=*/false);
    if (!pendingState_.empty()) {
        adoptDocumentState(pendingState_);
        pendingState_.clear();
    } else if (document_.valid()) {
        paintEngine_.setDocument(&document_);
        paintEngine_.setMesh(&mesh_);
    }
    return true;
}

/// Sube un documento guardado a GPU y lo enchufa al motor de pintado.
void Engine::adoptDocumentState(const DocumentState& state) {
    if (!document_.restoreState(state)) {
        LOGE("No se pudo restaurar el documento; se crea uno vacio");
        createDocument(state.resolution > 0 ? state.resolution : 2048);
        return;
    }
    paintEngine_.setDocument(&document_);
    paintEngine_.setMesh(&mesh_);
    if (mesh_.valid()) document_.buildUvMask(mesh_);
}

void Engine::onSurfaceChanged(int width, int height) {
    if (!context_.valid()) return;
    context_.refreshSurfaceSize();
    viewportW_ = std::max(width > 0 ? width : context_.width(), 1);
    viewportH_ = std::max(height > 0 ? height : context_.height(), 1);
    camera_.setViewport(viewportW_, viewportH_);
    paintEngine_.resizeViewport(viewportW_, viewportH_);
    LOGI("Viewport: %dx%d", viewportW_, viewportH_);
}

void Engine::onSurfaceDestroyed() {
    if (!resourcesReady_ && !context_.valid()) return;

    // Un punto de control a medio leer se queda sin texturas de las que leer, y
    // sin frames que lo empujen: se corta aqui o bloquearia todos los
    // siguientes. Lo que ya estuviera escribiendose no toca GL y puede acabar.
    if (checkpoint_.stage == Checkpoint::Stage::Reading) failCheckpoint("err.engine_not_ready");

    // Lo pintado solo existe en texturas, asi que hay que bajarlo a CPU AHORA,
    // con el contexto todavia vivo. Un segundo despues ya no habria nada que
    // leer.
    if (context_.valid() && document_.valid()) {
        if (!document_.captureState(pendingState_)) pendingState_.clear();
    }

    // El contexto se va y con el todos los objetos GL: hay que soltarlos
    // mientras el contexto sigue vivo.
    paintEngine_.shutdown();
    document_.destroy();
    renderer_.shutdown();
    mesh_.destroy();
    destroyFullscreenVao();
    context_.shutdown();
    if (window_ != nullptr) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
    resourcesReady_ = false;
}

uint32_t Engine::pickScreenColor(int x, int y) {
    if (!context_.valid()) return 0u;
    // El origen de glReadPixels esta abajo; el de la UI, arriba.
    const int flippedY = viewportH_ - 1 - std::clamp(y, 0, viewportH_ - 1);
    const int clampedX = std::clamp(x, 0, viewportW_ - 1);

    Framebuffer::unbind();
    uint8_t px[4] = {0, 0, 0, 0};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(clampedX, flippedY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    return (0xFFu << 24) | (static_cast<uint32_t>(px[0]) << 16) |
           (static_cast<uint32_t>(px[1]) << 8) | static_cast<uint32_t>(px[2]);
}

// ---------------------------------------------------------------------------
// Contenido
// ---------------------------------------------------------------------------
bool Engine::loadModel(const uint8_t* data, size_t size, const std::string& ext,
                       const std::string& name, std::string& error) {
    MeshData meshData;
    if (!loadModelFromMemory(data, size, ext, meshData, error)) {
        LOGE("Fallo cargando el modelo: %s", error.c_str());
        return false;
    }

    if (!meshData.hasUVs) {
        error =
            "Este modelo no trae coordenadas UV. Sin UVs no hay donde pintar: "
            "desplegalo antes de importarlo.";
        LOGE("%s", error.c_str());
        return false;
    }

    sourceMesh_ = std::move(meshData);
    sourceMesh_.computeUvIslands();

    // Copia literal del archivo: el proyecto la reutiliza para reabrir el
    // modelo sin depender de que el original siga en su sitio.
    modelBytes_.assign(data, data + size);
    modelExt_ = ext;
    modelName_ = name;

    meshStats_.vertexCount = static_cast<int>(sourceMesh_.vertices.size());
    meshStats_.triangleCount = static_cast<int>(sourceMesh_.indices.size() / 3);
    meshStats_.submeshCount = static_cast<int>(sourceMesh_.submeshes.size());
    meshStats_.islandCount = sourceMesh_.islandCount;
    meshStats_.hasUVs = sourceMesh_.hasUVs;
    meshStats_.uvsOutside01 = sourceMesh_.uvsOutside01;

    // Heuristica de orientacion. Los ejes horizontales de un modelo suelen
    // quedar centrados en el origen (la malla se reparte a ambos lados),
    // mientras que el vertical queda claramente a un solo lado: apoyado en el
    // suelo o colgando del origen. Se mide esa asimetria. Es una suposicion
    // razonable, no una certeza: por eso el ajuste esta tambien en la interfaz.
    const Vec3 extent = sourceMesh_.bounds.extent();
    const Vec3 mn = sourceMesh_.bounds.min;
    const Vec3 mx = sourceMesh_.bounds.max;
    const float asymY = extent.y > 1e-6f ? std::fabs(mn.y + mx.y) / extent.y : 0.0f;
    const float asymZ = extent.z > 1e-6f ? std::fabs(mn.z + mx.z) / extent.z : 0.0f;
    upAxis_ = (asymZ > 0.5f && asymZ > asymY * 1.5f) ? 1 : 0;

    // El sentido se elige para que el modelo quede por encima del plano del
    // origen. Un personaje exportado "de pie" siempre cumple eso; si la malla
    // cuelga hacia el lado negativo, es que ese eje apunta al reves.
    flipUp_ = (upAxis_ == 1) ? (mx.z <= -mn.z) : (mx.y <= -mn.y);
    quarterTurns_ = 0;
    rebuildOrientedMesh();

    LOGI("Modelo cargado: %d vertices, %d triangulos, %d submallas, %d islas UV | "
         "extent=%.2f/%.2f/%.2f asimetria Y=%.2f Z=%.2f -> %s%s%s",
         meshStats_.vertexCount, meshStats_.triangleCount, meshStats_.submeshCount,
         meshStats_.islandCount, extent.x, extent.y, extent.z, asymY, asymZ,
         upAxis_ == 1 ? "Z arriba" : "Y arriba", flipUp_ ? " (invertido)" : "",
         meshStats_.uvsOutside01 ? " (UVs fuera de 0-1: puede ser UDIM)" : "");
    return true;
}

void Engine::setOrientation(int upAxis, bool flipUp, int quarterTurns) {
    upAxis_ = (upAxis == 1) ? 1 : 0;
    flipUp_ = flipUp;
    quarterTurns_ = ((quarterTurns % 4) + 4) % 4;
    rebuildOrientedMesh();
}

void Engine::rebuildOrientedMesh(bool frameCamera) {
    if (sourceMesh_.empty()) return;

    // Z arriba -> Y arriba: giro de -90 grados en X.
    Mat4 transform = Mat4::identity();
    if (upAxis_ == 1) {
        transform.at(1, 1) = 0.0f;
        transform.at(1, 2) = -1.0f;
        transform.at(2, 1) = 1.0f;
        transform.at(2, 2) = 0.0f;
    }
    if (flipUp_) {
        // Giro de 180 grados en X: pone el eje vertical del derecho.
        Mat4 flip = Mat4::identity();
        flip.at(1, 1) = -1.0f;
        flip.at(2, 2) = -1.0f;
        transform = flip * transform;
    }
    if (quarterTurns_ != 0) {
        const float angle = radians(90.0f * static_cast<float>(quarterTurns_));
        Mat4 spin = Mat4::identity();
        spin.at(0, 0) = std::cos(angle);
        spin.at(0, 2) = -std::sin(angle);
        spin.at(2, 0) = std::sin(angle);
        spin.at(2, 2) = std::cos(angle);
        transform = spin * transform;
    }

    std::vector<Vertex> vertices = sourceMesh_.vertices;
    modelBounds_ = Aabb{};
    for (Vertex& v : vertices) {
        v.position = (transform * Vec4(v.position, 0.0f)).xyz();
        v.normal = normalize((transform * Vec4(v.normal, 0.0f)).xyz());
        modelBounds_.expand(v.position);
    }

    mesh_.upload(vertices, sourceMesh_.indices);
    mesh_.buildWireframe();

    camera_.setViewport(viewportW_, viewportH_);
    if (frameCamera) camera_.frameBounds(modelBounds_);

    if (document_.valid()) {
        document_.buildUvMask(mesh_);
        paintEngine_.setMesh(&mesh_);
        document_.markDirty();
    }
}

bool Engine::createDocument(int resolution) {
    if (!resourcesReady_) return false;
    // El documento que se estaba leyendo a plazos deja de existir aqui.
    if (checkpoint_.stage == Checkpoint::Stage::Reading) failCheckpoint("");
    if (!document_.create(resolution)) return false;
    paintEngine_.setDocument(&document_);
    paintEngine_.setMesh(&mesh_);
    if (mesh_.valid()) document_.buildUvMask(mesh_);
    return true;
}

// ---------------------------------------------------------------------------
// Proyecto
// ---------------------------------------------------------------------------
namespace {

/**
 * Comprime y escribe el .uvp. No toca GL ni el motor: todo lo que necesita
 * viaja dentro de [ProjectPayload], asi que puede correr en el hilo que sea.
 */
bool writeProject(const std::string& path, ProjectPayload& payload, std::string& error) {
    // La compresion es lo unico caro que queda, y va repartida entre nucleos.
    std::vector<PackJob> jobs;
    jobs.reserve(payload.state.layers.size() + 2u);
    jobs.push_back({payload.modelBytes.data(), payload.modelBytes.size(), &payload.packedModel});
    payload.packedLayers.resize(payload.state.layers.size());
    for (size_t i = 0; i < payload.state.layers.size(); ++i) {
        jobs.push_back({payload.state.layers[i].pixels.data(),
                        payload.state.layers[i].pixels.size(), &payload.packedLayers[i]});
    }
    jobs.push_back({payload.state.boundary.data(), payload.state.boundary.size(),
                    &payload.packedBoundary});
    if (!packAll(jobs)) {
        error = "err.project_write_failed";
        return false;
    }

    // Se escribe a un temporal y se renombra al final: si el proceso muere a
    // mitad, el proyecto anterior sigue intacto en vez de quedar truncado.
    const std::string temp = path + ".tmp";
    FILE* f = std::fopen(temp.c_str(), "wb");
    if (f == nullptr) {
        error = "err.write_failed|" + path;
        return false;
    }

    const DocumentState& state = payload.state;
    bool ok = writeRaw(f, kProjectMagic, sizeof(kProjectMagic));
    ok = ok && writeU32(f, kProjectVersion);
    ok = ok && writeU32(f, static_cast<uint32_t>(state.resolution));
    ok = ok && writeU32(f, static_cast<uint32_t>(state.activeIndex));
    ok = ok && writeU32(f, static_cast<uint32_t>(state.nextLayerId));
    ok = ok && writeU32(f, static_cast<uint32_t>(state.layers.size()));
    ok = ok && writeU32(f, static_cast<uint32_t>(payload.upAxis));
    ok = ok && writeU32(f, payload.flipUp ? 1u : 0u);
    ok = ok && writeU32(f, static_cast<uint32_t>(payload.quarterTurns));
    ok = ok && writeString(f, payload.modelExt);
    ok = ok && writeString(f, payload.modelName);
    ok = ok && writePacked(f, payload.modelBytes.size(), payload.packedModel);

    for (size_t i = 0; i < state.layers.size(); ++i) {
        if (!ok) break;
        const LayerState& layer = state.layers[i];
        ok = ok && writeString(f, layer.info.name);
        ok = ok && writeU32(f, static_cast<uint32_t>(layer.id));
        ok = ok && writeF32(f, layer.info.opacity);
        ok = ok && writeU32(f, static_cast<uint32_t>(layer.info.blend));
        const uint32_t flags = (layer.info.visible ? 1u : 0u) | (layer.info.locked ? 2u : 0u) |
                               (layer.info.alphaLock ? 4u : 0u) |
                               (layer.info.clipToBelow ? 8u : 0u);
        ok = ok && writeU32(f, flags);
        ok = ok && writePacked(f, layer.pixels.size(), payload.packedLayers[i]);
    }

    ok = ok && writePacked(f, state.boundary.size(), payload.packedBoundary);
    ok = ok && std::fflush(f) == 0;
    std::fclose(f);

    if (!ok) {
        std::remove(temp.c_str());
        error = "err.project_write_failed";
        return false;
    }
    std::remove(path.c_str());
    if (std::rename(temp.c_str(), path.c_str()) != 0) {
        std::remove(temp.c_str());
        error = "err.project_close_failed";
        return false;
    }

    LOGI("Proyecto guardado en %s (%d capas)", path.c_str(),
         static_cast<int>(state.layers.size()));
    return true;
}

}  // namespace

void Engine::fillPayloadMeta(ProjectPayload& payload) const {
    payload.upAxis = upAxis_;
    payload.flipUp = flipUp_;
    payload.quarterTurns = quarterTurns_;
    payload.modelExt = modelExt_;
    payload.modelName = modelName_;
    // Copia propia del modelo: el hilo que escribe vive por su cuenta y aqui
    // se puede estar cargando otro mientras tanto.
    payload.modelBytes = modelBytes_;
}

bool Engine::saveProject(const std::string& path, std::string& error) {
    if (!document_.valid()) {
        error = "err.nothing_to_save";
        return false;
    }

    // Guardar de una pieza y un punto de control a la vez escribirian en el
    // mismo temporal, asi que aqui se espera a que el de fondo termine. Es la
    // unica espera que se paga, y solo al guardar a mano o al salir.
    if (checkpoint_.stage == Checkpoint::Stage::Reading) failCheckpoint("");
    if (checkpoint_.worker.joinable()) checkpoint_.worker.join();

    ProjectPayload payload;
    if (!document_.captureState(payload.state)) {
        error = "err.layers_unreadable";
        return false;
    }
    fillPayloadMeta(payload);
    return writeProject(path, payload, error);
}

// ---------------------------------------------------------------------------
// Punto de control en segundo plano
//
// Guardar de una pieza planta el hilo de render: leer el atlas de cada capa y
// comprimirlo son un par de segundos con el lapiz en la mano, y el
// autoguardado los cobraba a mitad de trazo. Esto lo hace a plazos: una capa
// por frame y con lectura sin espera (la GPU sirve la copia por su cuenta), y
// en cuanto estan todos los bytes, un hilo aparte comprime y escribe.
//
// Las capas se leen en frames distintos, asi que un punto de control cogido
// mientras se pinta puede llevar unos milisegundos de desfase entre una capa y
// la siguiente. Para un guardado automatico es lo de menos: lo que importa es
// no perder el trabajo, y quien guarda a mano usa la ruta de una pieza.
// ---------------------------------------------------------------------------
bool Engine::beginCheckpoint(const std::string& path) {
    if (!document_.valid() || path.empty()) return false;
    // Uno cada vez. Si el anterior sigue escribiendo, el que llega se descarta:
    // el siguiente intento llegara solo, y encolarlos solo acumularia trabajo.
    if (checkpoint_.stage == Checkpoint::Stage::Reading ||
        checkpoint_.stage == Checkpoint::Stage::Writing) {
        return false;
    }
    if (checkpoint_.worker.joinable()) checkpoint_.worker.join();
    if (document_.readbackPending()) return false;

    checkpoint_.path = path;
    checkpoint_.error.clear();
    checkpoint_.finished.store(false);
    checkpoint_.ok.store(false);
    checkpoint_.payload = ProjectPayload{};
    fillPayloadMeta(checkpoint_.payload);

    // Los metadatos de las capas se copian ahora, de una vez; los pixeles van
    // llegando frame a frame.
    DocumentState& state = checkpoint_.payload.state;
    state.resolution = document_.resolution();
    state.activeIndex = document_.activeIndex();
    state.nextLayerId = document_.nextLayerId();
    state.layers.resize(static_cast<size_t>(document_.layerCount()));
    for (int i = 0; i < document_.layerCount(); ++i) {
        const Layer* layer = document_.layerAt(i);
        if (layer == nullptr) return false;
        state.layers[static_cast<size_t>(i)].info = layer->info;
        state.layers[static_cast<size_t>(i)].id = layer->id;
    }

    checkpoint_.layerIndex = 0;
    checkpoint_.readingBoundary = false;
    checkpoint_.boundaryRgba.clear();
    checkpoint_.stage = Checkpoint::Stage::Reading;
    return true;
}

void Engine::pollCheckpoint() {
    switch (checkpoint_.stage) {
        case Checkpoint::Stage::Reading: {
            // Un solo paso por frame: o se pide una lectura, o se recoge la que
            // ya estaba servida. Nunca las dos, para que ningun frame pague de
            // golpe la copia de 16 MB y la peticion siguiente.
            if (document_.readbackPending()) {
                if (!document_.readbackReady()) return;
                if (checkpoint_.readingBoundary) {
                    if (!document_.finishReadback(checkpoint_.boundaryRgba)) {
                        failCheckpoint("err.layers_unreadable");
                        return;
                    }
                    startCheckpointWorker();
                } else {
                    const size_t index = static_cast<size_t>(checkpoint_.layerIndex);
                    if (index >= checkpoint_.payload.state.layers.size() ||
                        !document_.finishReadback(
                            checkpoint_.payload.state.layers[index].pixels)) {
                        failCheckpoint("err.layers_unreadable");
                        return;
                    }
                    ++checkpoint_.layerIndex;
                }
                return;
            }

            // La pila de capas puede cambiar mientras se lee: añadir, borrar,
            // reordenar o cambiar la resolucion del atlas. Si pasa, este punto
            // de control ya no cuadra y se tira sin ruido; el siguiente lo coge
            // todo bien y no se pierde nada, porque el documento sigue marcado
            // como pendiente de guardar.
            const DocumentState& state = checkpoint_.payload.state;
            if (document_.resolution() != state.resolution ||
                document_.layerCount() != static_cast<int>(state.layers.size())) {
                failCheckpoint("");
                return;
            }
            if (checkpoint_.layerIndex < document_.layerCount()) {
                const Layer* layer = document_.layerAt(checkpoint_.layerIndex);
                if (layer == nullptr ||
                    layer->id != state.layers[static_cast<size_t>(checkpoint_.layerIndex)].id) {
                    failCheckpoint("");
                    return;
                }
                if (!document_.beginReadback(layer->texture)) {
                    failCheckpoint("err.layers_unreadable");
                }
                return;
            }
            if (document_.hasBoundaryContent() && !checkpoint_.readingBoundary) {
                checkpoint_.readingBoundary = true;
                if (!document_.beginReadback(document_.boundaryMask())) {
                    failCheckpoint("err.layers_unreadable");
                }
                return;
            }
            startCheckpointWorker();
            return;
        }
        case Checkpoint::Stage::Writing:
            if (!checkpoint_.finished.load()) return;
            if (checkpoint_.worker.joinable()) checkpoint_.worker.join();
            checkpoint_.stage = checkpoint_.ok.load() ? Checkpoint::Stage::Done
                                                      : Checkpoint::Stage::Failed;
            // Los pixeles ya no hacen falta y son decenas de megas.
            checkpoint_.payload = ProjectPayload{};
            checkpoint_.boundaryRgba.clear();
            return;
        default:
            return;
    }
}

void Engine::startCheckpointWorker() {
    checkpoint_.stage = Checkpoint::Stage::Writing;
    checkpoint_.finished.store(false);
    checkpoint_.worker = std::thread([this]() {
        setpriority(PRIO_PROCESS, 0, 10);  // THREAD_PRIORITY_BACKGROUND
        // La pared se guarda a un byte por texel; se leyo en cuatro canales
        // porque es lo unico que garantiza la lectura de vuelta.
        if (!checkpoint_.boundaryRgba.empty()) {
            const size_t count = checkpoint_.boundaryRgba.size() / 4u;
            checkpoint_.payload.state.boundary.resize(count);
            for (size_t i = 0; i < count; ++i) {
                checkpoint_.payload.state.boundary[i] = checkpoint_.boundaryRgba[i * 4u];
            }
            checkpoint_.boundaryRgba.clear();
            checkpoint_.boundaryRgba.shrink_to_fit();
        }
        std::string error;
        const bool ok = writeProject(checkpoint_.path, checkpoint_.payload, error);
        checkpoint_.error = error;
        checkpoint_.ok.store(ok);
        checkpoint_.finished.store(true);
    });
}

void Engine::failCheckpoint(const char* reason) {
    checkpoint_.error = reason;
    checkpoint_.stage = Checkpoint::Stage::Failed;
    checkpoint_.payload = ProjectPayload{};
    checkpoint_.boundaryRgba.clear();
}

int Engine::checkpointStatus() {
    switch (checkpoint_.stage) {
        case Checkpoint::Stage::Reading:
        case Checkpoint::Stage::Writing:
            return 1;
        case Checkpoint::Stage::Done:
            checkpoint_.stage = Checkpoint::Stage::Idle;
            return 2;
        case Checkpoint::Stage::Failed:
            checkpoint_.stage = Checkpoint::Stage::Idle;
            return 3;
        case Checkpoint::Stage::Idle:
            return 0;
    }
    return 0;
}

bool Engine::loadProject(const std::string& path, std::string& error) {
    if (!resourcesReady_) {
        error = "err.engine_not_ready";
        return false;
    }
    // Abrir otro proyecto se lleva por delante las capas que se estuvieran
    // leyendo para el punto de control.
    if (checkpoint_.stage == Checkpoint::Stage::Reading) failCheckpoint("");

    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        error = "err.project_not_found";
        return false;
    }

    char magic[sizeof(kProjectMagic)] = {};
    uint32_t version = 0;
    uint32_t resolution = 0;
    uint32_t activeIndex = 0;
    uint32_t nextLayerId = 1;
    uint32_t layerCount = 0;
    uint32_t upAxis = 0;
    uint32_t flipUp = 0;
    uint32_t quarterTurns = 0;
    std::string ext;
    std::string name;
    std::vector<uint8_t> modelData;

    bool ok = readRaw(f, magic, sizeof(magic)) &&
              std::memcmp(magic, kProjectMagic, sizeof(magic)) == 0;
    ok = ok && readU32(f, version) && version == kProjectVersion;
    ok = ok && readU32(f, resolution) && readU32(f, activeIndex) && readU32(f, nextLayerId) &&
         readU32(f, layerCount);
    ok = ok && readU32(f, upAxis) && readU32(f, flipUp) && readU32(f, quarterTurns);
    ok = ok && readString(f, ext) && readString(f, name) && readBlob(f, modelData);
    ok = ok && layerCount > 0 && layerCount <= 512 && resolution >= 512 && resolution <= 4096;

    DocumentState state;
    state.resolution = static_cast<int>(resolution);
    state.activeIndex = static_cast<int>(activeIndex);
    state.nextLayerId = static_cast<int>(nextLayerId);

    for (uint32_t i = 0; ok && i < layerCount; ++i) {
        LayerState layer;
        uint32_t id = 0;
        uint32_t blend = 0;
        uint32_t flags = 0;
        ok = ok && readString(f, layer.info.name) && readU32(f, id) &&
             readF32(f, layer.info.opacity) && readU32(f, blend) && readU32(f, flags) &&
             readBlob(f, layer.pixels);
        if (!ok) break;
        layer.id = static_cast<int>(id);
        layer.info.blend = static_cast<BlendMode>(blend);
        layer.info.visible = (flags & 1u) != 0;
        layer.info.locked = (flags & 2u) != 0;
        layer.info.alphaLock = (flags & 4u) != 0;
        layer.info.clipToBelow = (flags & 8u) != 0;
        state.layers.push_back(std::move(layer));
    }
    ok = ok && readBlob(f, state.boundary);
    std::fclose(f);

    if (!ok) {
        error = "err.project_invalid";
        return false;
    }

    if (!modelData.empty()) {
        std::string modelError;
        if (!loadModel(modelData.data(), modelData.size(), ext, name, modelError)) {
            error = modelError;
            return false;
        }
        setOrientation(static_cast<int>(upAxis), flipUp != 0, static_cast<int>(quarterTurns));
    }

    adoptDocumentState(state);
    LOGI("Proyecto abierto: %s (%u capas)", name.c_str(), layerCount);
    return true;
}

void Engine::resetView() {
    camera_.setViewport(viewportW_, viewportH_);
    if (modelBounds_.valid()) camera_.frameBounds(modelBounds_);
    camera_.resetRoll();
}

bool Engine::exportComposite(std::vector<uint8_t>& rgba, int& outSize, bool dilateForExport) {
    if (!document_.valid()) return false;
    outSize = document_.resolution();
    // 8 pasadas de dilatado dan margen de sobra para mipmaps agresivos.
    return document_.readComposite(rgba, dilateForExport, 8);
}

bool Engine::exportLayer(int index, std::vector<uint8_t>& rgba, int& outSize) {
    if (!document_.valid()) return false;
    outSize = document_.resolution();
    return document_.readLayer(index, rgba);
}

bool Engine::importLayerPixels(int index, const uint8_t* rgba, int size) {
    if (!document_.valid() || rgba == nullptr) return false;
    if (size != document_.resolution()) {
        LOGE("La imagen importada es de %d px y el documento de %d px", size,
             document_.resolution());
        return false;
    }
    Layer* layer = document_.layerAt(index);
    if (layer == nullptr) return false;
    layer->texture.upload(rgba);
    document_.markDirty();
    return true;
}

// ---------------------------------------------------------------------------
// Entrada
// ---------------------------------------------------------------------------
void Engine::pushCommand(const InputCommand& cmd) {
    std::lock_guard<std::mutex> lock(commandMutex_);
    commandQueue_.push_back(cmd);
}

void Engine::drainCommands() {
    {
        std::lock_guard<std::mutex> lock(commandMutex_);
        if (commandQueue_.empty()) return;
        commandScratch_.swap(commandQueue_);
        commandQueue_.clear();
    }
    for (const InputCommand& cmd : commandScratch_) handleCommand(cmd);
    commandScratch_.clear();
}

void Engine::handleCommand(const InputCommand& cmd) {
    switch (cmd.kind) {
        case InputCommand::Kind::StrokeBegin: {
            StrokePoint p;
            p.screen = {cmd.x, cmd.y};
            p.pressure = cmd.pressure;
            p.tilt = cmd.tilt;
            p.orientation = cmd.orientation;
            p.timeMs = cmd.timeMs;
            paintEngine_.beginStroke(p, brush_, paintMode_, camera_);
            hoverPos_ = p.screen;
            hoverVisible_ = true;
            hoverPressure_ = cmd.pressure;
            break;
        }
        case InputCommand::Kind::StrokeMove: {
            StrokePoint p;
            p.screen = {cmd.x, cmd.y};
            p.pressure = cmd.pressure;
            p.tilt = cmd.tilt;
            p.orientation = cmd.orientation;
            p.timeMs = cmd.timeMs;
            paintEngine_.addPoint(p);
            hoverPos_ = p.screen;
            hoverVisible_ = true;
            hoverPressure_ = cmd.pressure;
            break;
        }
        case InputCommand::Kind::StrokePredict: {
            StrokePoint p;
            p.screen = {cmd.x, cmd.y};
            p.pressure = cmd.pressure;
            p.tilt = cmd.tilt;
            p.orientation = cmd.orientation;
            p.timeMs = cmd.timeMs;
            p.predicted = true;
            paintEngine_.addPredictedPoint(p);
            break;
        }
        case InputCommand::Kind::Fill:
            paintEngine_.requestFill({cmd.x, cmd.y}, brush_);
            break;
        case InputCommand::Kind::StrokeEnd:
            paintEngine_.endStroke();
            hoverPressure_ = 0.0f;
            break;
        case InputCommand::Kind::StrokeCancel:
            paintEngine_.cancelStroke();
            hoverPressure_ = 0.0f;
            break;
        case InputCommand::Kind::Orbit:
            camera_.orbit(cmd.x, cmd.y);
            break;
        case InputCommand::Kind::Pan:
            camera_.pan(cmd.x, cmd.y);
            break;
        case InputCommand::Kind::Zoom:
            camera_.dolly(cmd.x);
            break;
        case InputCommand::Kind::ZoomAt:
            camera_.dollyAt(cmd.x, cmd.y, cmd.tilt);
            break;
        case InputCommand::Kind::Roll:
            camera_.roll(cmd.x);
            break;
        case InputCommand::Kind::Hover:
            hoverPos_ = {cmd.x, cmd.y};
            hoverVisible_ = true;
            hoverPressure_ = cmd.pressure;
            break;
        case InputCommand::Kind::HoverEnd:
            hoverVisible_ = false;
            hoverPressure_ = 0.0f;
            break;
    }
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
bool Engine::drawFrame() {
    if (!context_.valid() || !resourcesReady_) return false;

    drainCommands();
    paintEngine_.flush(camera_);
    // Un paso del punto de control por frame, si es que hay alguno en marcha.
    pollCheckpoint();

    const Texture2D* baseColor = nullptr;
    if (document_.valid()) baseColor = &document_.composite();

    if (mesh_.valid() && baseColor != nullptr) {
        const Texture2D* boundary =
            document_.valid() ? &document_.boundaryMask() : nullptr;
        renderer_.renderScene(viewportW_, viewportH_, camera_, mesh_, *baseColor, boundary,
                              viewport_);
    } else {
        Framebuffer::unbind();
        glViewport(0, 0, viewportW_, viewportH_);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glClearColor(viewport_.backgroundBottom.x, viewport_.backgroundBottom.y,
                     viewport_.backgroundBottom.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (hoverVisible_) {
        // El cursor crece con la presion para que se vea la respuesta del lapiz.
        float radius = brush_.radiusPx;
        if (brush_.pressureAffectsSize && hoverPressure_ > 0.0f) {
            radius *= lerpf(brush_.pressureSizeFloor, 1.0f, clampf(hoverPressure_, 0.0f, 1.0f));
        }
        // Mismo ajuste que al pintar: si no, el cursor prometeria un tamano
        // que el trazo real no cumpliria.
        if (brush_.lockSizeToSurface) {
            radius *= camera_.referenceDistance() / std::max(camera_.distance(), 1e-4f);
        }
        const Vec4 color = paintMode_ == PaintMode::Erase ? Vec4(1.0f, 0.45f, 0.4f, 0.85f)
                                                          : Vec4(1.0f, 1.0f, 1.0f, 0.75f);

        // Con el regulador activo el pincel va colgando por detras de la punta,
        // asi que el anillo tiene que ir donde cae la pintura y no donde esta el
        // lapiz: es el unico circulo que se dibuja, y en la punta no va nada. La
        // cuerda entre los dos explica ese retraso, y arranca en el borde del
        // anillo para no cruzarlo por debajo.
        Vec2 cursorAt = hoverPos_;
        const float rope = paintEngine_.ropeLength();
        if (rope > 0.0f) {
            cursorAt = paintEngine_.paintPosition();
            renderer_.drawRope(viewportW_, viewportH_, cursorAt, hoverPos_, radius,
                               Vec4(color.x, color.y, color.z, 0.55f));
        }
        renderer_.drawBrushCursor(viewportW_, viewportH_, cursorAt, radius, brush_.hardness,
                                  color);
    }

    context_.swapBuffers();
    return true;
}

}  // namespace uvp
