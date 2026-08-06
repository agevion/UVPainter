#include "paint/UndoStack.h"

#include "core/Log.h"

namespace uvp {

void UndoStack::push(PixelPatch&& patch) {
    // Al pintar algo nuevo se descarta la rama de rehacer.
    while (static_cast<int>(records_.size()) > cursor_) {
        usedBytes_ -= records_.back().bytes();
        records_.pop_back();
    }

    usedBytes_ += patch.bytes();
    records_.push_back(std::move(patch));
    cursor_ = static_cast<int>(records_.size());
    trimToBudget();
}

void UndoStack::clear() {
    records_.clear();
    cursor_ = 0;
    usedBytes_ = 0;
}

const PixelPatch* UndoStack::undo() {
    if (!canUndo()) return nullptr;
    --cursor_;
    return &records_[static_cast<size_t>(cursor_)];
}

const PixelPatch* UndoStack::redo() {
    if (!canRedo()) return nullptr;
    const PixelPatch* patch = &records_[static_cast<size_t>(cursor_)];
    ++cursor_;
    return patch;
}

void UndoStack::trimToBudget() {
    // Se tiran los pasos mas antiguos, nunca los recientes.
    while (usedBytes_ > budgetBytes_ && records_.size() > 1) {
        usedBytes_ -= records_.front().bytes();
        records_.erase(records_.begin());
        if (cursor_ > 0) --cursor_;
    }
    if (usedBytes_ > budgetBytes_) {
        LOGW("Un solo paso de historial ocupa %zu MB", usedBytes_ / (1024u * 1024u));
    }
}

}  // namespace uvp
