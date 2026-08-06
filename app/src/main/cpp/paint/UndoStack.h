// Historial basado en parches de pixeles acotados al rectangulo que el trazo
// toco de verdad. Guardar la capa entera por paso reventaria la memoria de la
// tablet en cuanto el atlas pasa de 2K.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace uvp {

struct PixelPatch {
    int layerId = 0;
    int x = 0, y = 0, w = 0, h = 0;
    std::vector<uint8_t> before;
    std::vector<uint8_t> after;
    std::string label;

    size_t bytes() const { return before.size() + after.size(); }
};

class UndoStack {
public:
    void setMemoryBudget(size_t bytes) { budgetBytes_ = bytes; }

    void push(PixelPatch&& patch);
    void clear();

    bool canUndo() const { return cursor_ > 0; }
    bool canRedo() const { return cursor_ < static_cast<int>(records_.size()); }

    // Devuelven el parche a aplicar, o nullptr si no hay nada que hacer.
    const PixelPatch* undo();
    const PixelPatch* redo();

    int undoCount() const { return cursor_; }
    int redoCount() const { return static_cast<int>(records_.size()) - cursor_; }
    size_t memoryUsed() const { return usedBytes_; }

private:
    void trimToBudget();

    std::vector<PixelPatch> records_;
    int cursor_ = 0;  // numero de registros ya aplicados
    size_t usedBytes_ = 0;
    size_t budgetBytes_ = 320u * 1024u * 1024u;
};

}  // namespace uvp
