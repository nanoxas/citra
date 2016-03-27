// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <type_traits>

namespace MathUtil
{

inline bool IntervalsIntersect(unsigned start0, unsigned length0, unsigned start1, unsigned length1) {
    return (std::max(start0, start1) < std::min(start0 + length0, start1 + length1));
}

template<typename T>
inline T Clamp(const T val, const T& min, const T& max) {
    return std::max(min, std::min(max, val));
}

/// Sign-extends a val of type T that has NBits bits.
template<size_t NBits, typename T>
inline T SignExtend(const T val) {
    static_assert(std::numeric_limits<T>::is_signed, "T must be signed");
    static_assert(NBits <= 8 * sizeof(T), "NBits larger that bitsize of T");

    using unsignedT = typename std::make_unsigned<T>::type;

    constexpr size_t shift = 8 * sizeof(T) - NBits;
    return static_cast<T>(static_cast<unsignedT>(val) << shift) >> shift;
}

template<class T>
struct Rectangle {
    T left;
    T top;
    T right;
    T bottom;

    Rectangle() {}

    Rectangle(T left, T top, T right, T bottom) : left(left), top(top), right(right), bottom(bottom) {}

    T GetWidth() const { return std::abs(static_cast<typename std::make_signed<T>::type>(right - left)); }
    T GetHeight() const { return std::abs(static_cast<typename std::make_signed<T>::type>(bottom - top)); }
};

}  // namespace MathUtil
