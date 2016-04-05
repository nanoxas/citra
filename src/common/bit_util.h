// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <type_traits>

namespace BitUtil {

/// Extract bits [begin_bit, end_bit] inclusive from s of type T.
template<size_t begin_bit, size_t end_bit, typename T>
constexpr T Bits(T s) {
    static_assert(begin_bit <= end_bit, "bit range must begin before it ends");
    static_assert(begin_bit < sizeof(s) * 8, "begin_bit must be smaller than size of T");
    static_assert(end_bit < sizeof(s) * 8, "begin_bit must be smaller than size of T");

    return (s >> begin_bit) & ((1 << (end_bit - begin_bit + 1)) - 1);
}

/// Sign-extends a val of type T that has NBits bits.
template<size_t NBits, typename T>
inline T SignExtend(const T val) {
    static_assert(std::is_signed<T>(), "T must be signed");
    static_assert(NBits <= 8 * sizeof(T), "NBits larger that bitsize of T");

    using unsignedT = typename std::make_unsigned<T>::type;

    constexpr size_t shift = 8 * sizeof(T) - NBits;
    return static_cast<T>(static_cast<unsignedT>(val) << shift) >> shift;
}

} // namespace BitUtil
