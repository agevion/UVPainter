// Parser JSON minimo, suficiente para glTF 2.0. Sin excepciones ni dependencias.
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace uvp {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<JsonValue> items;                          // Array
    std::vector<std::pair<std::string, JsonValue>> fields;  // Object

    bool isNull() const { return type == Type::Null; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    size_t size() const { return type == Type::Array ? items.size() : 0; }

    const JsonValue& operator[](size_t i) const {
        static const JsonValue kNull;
        return (type == Type::Array && i < items.size()) ? items[i] : kNull;
    }

    // Devuelve nullptr si la clave no existe: asi se distingue "ausente" de "0".
    const JsonValue* find(const char* key) const {
        if (type != Type::Object) return nullptr;
        for (const auto& f : fields) {
            if (f.first == key) return &f.second;
        }
        return nullptr;
    }

    const JsonValue& get(const char* key) const {
        static const JsonValue kNull;
        const JsonValue* v = find(key);
        return v != nullptr ? *v : kNull;
    }

    double asNumber(double fallback = 0.0) const {
        return type == Type::Number ? number : fallback;
    }
    int asInt(int fallback = 0) const {
        return type == Type::Number ? static_cast<int>(number) : fallback;
    }
    bool asBool(bool fallback = false) const {
        return type == Type::Bool ? boolean : fallback;
    }
    const std::string& asString() const {
        static const std::string kEmpty;
        return type == Type::String ? text : kEmpty;
    }

    // Atajos para leer campos con valor por defecto.
    int intField(const char* key, int fallback = 0) const { return get(key).asInt(fallback); }
    double numField(const char* key, double fallback = 0.0) const {
        return get(key).asNumber(fallback);
    }
};

// Devuelve false si el JSON esta mal formado. `out` queda indefinido en ese caso.
bool parseJson(const char* data, size_t length, JsonValue& out);

}  // namespace uvp
