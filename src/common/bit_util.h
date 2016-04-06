// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <climits>
#include <cstddef>

namespace BitUtil {

/// The size of a type in terms of bits
template <typename T>
constexpr size_t BitSize() {
    return sizeof(T) * CHAR_BIT;
}

/// Extract bits [begin_bit, end_bit] inclusive from val of type T.
template<size_t begin_bit, size_t end_bit, typename T>
constexpr T Bits(const T value) {
    static_assert(begin_bit <= end_bit, "bit range must begin before it ends");
    static_assert(begin_bit < BitSize<T>(), "begin_bit must be smaller than size of T");
    static_assert(end_bit < BitSize<T>(), "begin_bit must be smaller than size of T");

    return (value >> begin_bit) & ((1 << (end_bit - begin_bit + 1)) - 1);
}

/// Extracts a single bit at bit_position from val of type T.
template<size_t bit_position, typename T>
constexpr T Bit(const T value) {
    return Bits<bit_position, bit_position, T>(value);
}

/// Sign-extends a val that has NBits bits to the full bitwidth of type T.
template<size_t NBits, typename T>
inline T SignExtend(const T value) {
    static_assert(NBits <= BitSize<T>(), "NBits larger that bitsize of T");

    constexpr T mask = static_cast<T>(1ULL << NBits) - 1;
    const T signbit = (value >> NBits) & 1;
    if (signbit != 0) {
        return value | ~mask;
    }
    return value;
}

} // namespace BitUtil
