// Native invoker and the value types natives exchange with the game.
#pragma once
#include <cstdint>
#include <cstring>
#include <type_traits>
#include "scripthook.h"

using Hash = uint32_t;
using Any = uint32_t;

// Vectors cross the native boundary as three 8-byte slots (float + padding each).
struct alignas(8) Vector3 {
    float x = 0; uint32_t px = 0;
    float y = 0; uint32_t py = 0;
    float z = 0; uint32_t pz = 0;
    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};
static_assert(sizeof(Vector3) == 24, "native Vector3 layout");

namespace tg {

template <typename T>
inline UINT64 toNativeArg(T v) {
    static_assert(sizeof(T) <= sizeof(UINT64), "native argument wider than 64 bits");
    UINT64 out = 0;
    if constexpr (std::is_same_v<T, bool>) {
        out = v ? 1 : 0;
    } else {
        std::memcpy(&out, &v, sizeof(T));  // same as the SDK: value in the low bytes, rest zero
    }
    return out;
}

template <typename R, typename... A>
inline R invoke(UINT64 hash, A... args) {
    nativeInit(hash);
    (nativePush64(toNativeArg(args)), ...);
    if constexpr (std::is_void_v<R>) {
        nativeCall();
    } else {
        return *reinterpret_cast<R*>(nativeCall());
    }
}

}  // namespace tg
