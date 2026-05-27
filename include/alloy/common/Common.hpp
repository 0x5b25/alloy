#pragma once

#include "Macros.h"
#include "RefCnt.hpp"
#include <cassert>

namespace alloy::common
{
    template<typename T, typename U>
    std::enable_if_t<std::is_convertible<T*, U*>::value, T*>
    PtrCast(U* ptr){
    #ifdef VLD_DEBUG
        T* res = dynamic_cast<T*>(ptr);
        //Actually is the target type
        assert(res != nullptr);
        //And can be done using a reinterpret_cast
        //assert((void*)res == (void*)ptr);
        return res;
    #else
        return static_cast<T*>(ptr);
    #endif
    }

    template<typename T, typename U>
    sp<T> SPCast(const sp<U>& ptr) {
        return RefRawPtr<T>(PtrCast<T>(ptr.get()));
    }

    template<typename T>
    sp<T> RefRawPtr(T* ptr){
        ptr->ref();
        return sp{ptr};        
    }


    /*
    Returns true if given number is a power of two.
    T must be unsigned integer number or signed integer but always nonnegative.
    For 0 returns true.
    */
    template <typename T>
    inline constexpr bool IsPow2(T x) {
        return (x & (x - 1)) == 0;
    }

    // Aligns given value up to nearest multiply of align value. For example: VmaAlignUp(11, 8) = 16.
    // Use types like uint32_t, uint64_t as T.
    template <typename T>
    inline constexpr T AlignUp(T val, T alignment) {
        assert(IsPow2(alignment));
        return (val + alignment - 1) & ~(alignment - 1);
    }

    // Aligns given value down to nearest multiply of align value. For example: VmaAlignDown(11, 8) = 8.
    // Use types like uint32_t, uint64_t as T.
    template <typename T>
    inline constexpr T AlignDown(T val, T alignment) {
        assert(IsPow2(alignment));
        return val & ~(alignment - 1);
    }

    // Division with mathematical rounding to nearest number.
    template <typename T>
    inline constexpr T RoundDiv(T x, T y) {
        return (x + (y / (T)2)) / y;
    }

    // Divide by 'y' and round up to nearest integer.
    template <typename T>
    inline constexpr T DivideRoundingUp(T x, T y) {
        return (x + y - (T)1) / y;
    }

    // Returns smallest power of 2 greater or equal to v.
    inline constexpr uint32_t NextPow2(uint32_t v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        return v;
    }

    inline constexpr uint64_t NextPow2(uint64_t v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        v++;
        return v;
    }

    // Returns largest power of 2 less or equal to v.
    inline constexpr uint32_t PrevPow2(uint32_t v) {
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v = v ^ (v >> 1);
        return v;
    }

    inline constexpr uint64_t PrevPow2(uint64_t v) {
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        v = v ^ (v >> 1);
        return v;
    }

} // namespace alloy

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete;   \
    void operator=(const TypeName&) = delete


