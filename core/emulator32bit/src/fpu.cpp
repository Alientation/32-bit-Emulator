#include "emulator32bit/fpu.h"

#include <atomic>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <limits>

// The host is asked for the result and the flags of one operation at a time. On x86-64 the SSE
// control register is read and written directly (a few cycles); anywhere else, or with
// AEMU_FPU_FENV defined, the same is done with <cfenv>.
#if defined(__SSE2__) && defined(__x86_64__) && !defined(AEMU_FPU_FENV)
#include <xmmintrin.h>
#define AEMU_FPU_MXCSR 1
#endif

namespace fpu
{
namespace
{

/// A compiler may not move a floating point operation across the change of the rounding mode or
/// the read of the flags (it knows nothing of them), so the operands and the results of the
/// operations below are volatile and the changes are bracketed by this.
inline void host_barrier()
{
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

#ifdef AEMU_FPU_MXCSR

/// Always inlined (see the members): whether the compiler inlined it depended on how many
/// operations use it, and `fop3` made the instructions that were there before slower by being one
/// more.
///
/// While it lives the host rounds in the given mode and its exception flags start clear; the
/// state the host had is put back afterwards.
class HostFpu
{
  public:
    [[gnu::always_inline]] explicit HostFpu(word rounding) : m_saved(_mm_getcsr())
    {
        // All exceptions masked (0x1F80), the rounding control in bits 14-13: 00 nearest,
        // 01 down, 10 up, 11 zero.
        constexpr unsigned kMxcsrRounding[4] = {0, 2, 1, 3};
        _mm_setcsr(0x1F80u | (kMxcsrRounding[rounding & 3] << 13));
        host_barrier();
    }

    [[gnu::always_inline]] ~HostFpu()
    {
        host_barrier();
        _mm_setcsr(m_saved);
    }

    HostFpu(const HostFpu &) = delete;
    HostFpu &operator=(const HostFpu &) = delete;

    /// @return the flags the operations since the constructor raised, as an FPSR value
    [[gnu::always_inline]] word flags() const
    {
        host_barrier();
        const unsigned csr = _mm_getcsr();
        word flags = 0;
        flags |= (csr & 0x01) ? kInvalid : 0;   // IE
        flags |= (csr & 0x04) ? kDivByZero : 0; // ZE
        flags |= (csr & 0x08) ? kOverflow : 0;  // OE
        flags |= (csr & 0x10) ? kUnderflow : 0; // UE
        flags |= (csr & 0x20) ? kInexact : 0;   // PE
        return flags;
    }

  private:
    unsigned m_saved;
};

#else

class HostFpu
{
  public:
    [[gnu::always_inline]] explicit HostFpu(word rounding) : m_saved(std::fegetround())
    {
        constexpr int kModes[4] = {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};
        std::feclearexcept(FE_ALL_EXCEPT);
        if (kModes[rounding & 3] != m_saved)
        {
            std::fesetround(kModes[rounding & 3]);
        }
        host_barrier();
    }

    [[gnu::always_inline]] ~HostFpu()
    {
        host_barrier();
        std::fesetround(m_saved);
    }

    HostFpu(const HostFpu &) = delete;
    HostFpu &operator=(const HostFpu &) = delete;

    [[gnu::always_inline]] word flags() const
    {
        host_barrier();
        const int raised = std::fetestexcept(FE_ALL_EXCEPT);
        word flags = 0;
        flags |= (raised & FE_INVALID) ? kInvalid : 0;
        flags |= (raised & FE_DIVBYZERO) ? kDivByZero : 0;
        flags |= (raised & FE_OVERFLOW) ? kOverflow : 0;
        flags |= (raised & FE_UNDERFLOW) ? kUnderflow : 0;
        flags |= (raised & FE_INEXACT) ? kInexact : 0;
        return flags;
    }

  private:
    int m_saved;
};

#endif

/// The two precisions, with what the code needs to know about the bit patterns.
template <class T>
struct Format;

template <>
struct Format<float>
{
    using Bits = std::uint32_t;
    static constexpr Bits kSign = 0x80000000u;
    static constexpr Bits kQuiet = 0x00400000u;
    static constexpr Bits kExponent = 0x7F800000u;
    static constexpr Bits kDefaultNan = 0x7FC00000u;
};

template <>
struct Format<double>
{
    using Bits = std::uint64_t;
    static constexpr Bits kSign = 0x8000000000000000ull;
    static constexpr Bits kQuiet = 0x0008000000000000ull;
    static constexpr Bits kExponent = 0x7FF0000000000000ull;
    static constexpr Bits kDefaultNan = 0x7FF8000000000000ull;
};

template <class T>
T from_bits(U64 bits)
{
    return std::bit_cast<T>(typename Format<T>::Bits(bits));
}

template <class T>
U64 to_bits(T value)
{
    return U64(std::bit_cast<typename Format<T>::Bits>(value));
}

template <class T>
bool is_signaling_nan(T value)
{
    using F = Format<T>;
    const typename F::Bits bits = std::bit_cast<typename F::Bits>(value);
    return (bits & F::kExponent) == F::kExponent
           && (bits & ~(F::kSign | F::kExponent)) != 0 && (bits & F::kQuiet) == 0;
}

/// @return the bits of @p value, or the default NaN if it is one
template <class T>
U64 canonical(T value)
{
    return value != value ? U64(Format<T>::kDefaultNan) : to_bits(value);
}

template <class T>
Result binary_op(U8 fn, U64 abits, U64 bbits, word rounding)
{
    const T a = from_bits<T>(abits);
    const T b = from_bits<T>(bbits);

    if (fn == kBinaryFn_min || fn == kBinaryFn_max)
    {
        // Like fmin and fmax of C: a NaN operand is ignored. A signaling one is an error.
        if (is_signaling_nan(a) || is_signaling_nan(b))
        {
            return {Format<T>::kDefaultNan, kInvalid};
        }
        if (a != a || b != b)
        {
            const T number = a != a ? b : a;
            return {canonical(number), 0};
        }
        const bool pick_a = a == b ? (std::signbit(a) == (fn == kBinaryFn_min))
                                   : (fn == kBinaryFn_min ? a < b : a > b);
        return {to_bits(pick_a ? a : b), 0};
    }

    HostFpu host(rounding);
    volatile T va = a;
    volatile T vb = b;
    volatile T result = 0;
    switch (fn)
    {
    case kBinaryFn_add:
        result = va + vb;
        break;
    case kBinaryFn_sub:
        result = va - vb;
        break;
    case kBinaryFn_mul:
        result = va * vb;
        break;
    default: // kBinaryFn_div
        result = va / vb;
        break;
    }
    const T value = result;
    return {canonical(value), host.flags()};
}

/// a * b + c with one rounding. The signs of the operands are changed first for the other three
/// functions; a sign change is exact, and the result of a NaN is the default NaN anyway.
template <class T>
Result fused_op(U8 fn, U64 abits, U64 bbits, U64 cbits, word rounding)
{
    T a = from_bits<T>(abits);
    const T b = from_bits<T>(bbits);
    T c = from_bits<T>(cbits);
    if (fn == kFmaFn_msub || fn == kFmaFn_nmadd)
    {
        a = -a;
    }
    if (fn == kFmaFn_nmadd || fn == kFmaFn_nmsub)
    {
        c = -c;
    }

    HostFpu host(rounding);
    volatile T va = a;
    volatile T vb = b;
    volatile T vc = c;
    volatile T result = std::fma(T(va), T(vb), T(vc));
    const T value = result;
    return {canonical(value), host.flags()};
}

template <class T>
Result sqrt_op(U64 abits, word rounding)
{
    HostFpu host(rounding);
    volatile T va = from_bits<T>(abits);
    volatile T result = std::sqrt(T(va));
    const T value = result;
    return {canonical(value), host.flags()};
}

/// abs, neg: the sign bit and nothing else
template <class T>
Result sign_op(U8 fn, U64 abits)
{
    using F = Format<T>;
    const typename F::Bits bits = typename F::Bits(abits);
    return {U64(fn == kUnaryFn_abs ? (bits & ~F::kSign) : (bits ^ F::kSign)), 0};
}

/// The rint family: round to an integral value and stay a float. Inexact is not raised.
template <class T>
Result rint_op(U8 fn, U64 abits, word rounding)
{
    const T a = from_bits<T>(abits);
    if (a != a)
    {
        return {Format<T>::kDefaultNan, is_signaling_nan(a) ? kInvalid : 0};
    }

    T value;
    switch (fn)
    {
    case kUnaryFn_rintz:
        value = std::trunc(a);
        break;
    case kUnaryFn_rintm:
        value = std::floor(a);
        break;
    case kUnaryFn_rintp:
        value = std::ceil(a);
        break;
    case kUnaryFn_rinta:
        value = std::round(a);
        break;
    default: // kUnaryFn_rint
    {
        HostFpu host(rounding);
        volatile T va = a;
        volatile T result = std::nearbyint(T(va));
        value = result;
        break;
    }
    }
    return {to_bits(value), 0};
}

/// Float to a 32 bit integer, saturating. A NaN gives 0.
template <class T>
Result to_int_op(U8 fn, U64 abits, word rounding)
{
    const bool is_signed = fn == kUnaryFn_tos32 || fn == kUnaryFn_tos32r;
    const bool current_mode = fn == kUnaryFn_tos32r || fn == kUnaryFn_tou32r;
    const T a = from_bits<T>(abits);

    if (a != a)
    {
        return {0, kInvalid};
    }

    T rounded;
    if (current_mode)
    {
        HostFpu host(rounding);
        volatile T va = a;
        volatile T result = std::nearbyint(T(va));
        rounded = result;
    }
    else
    {
        rounded = std::trunc(a);
    }

    // The limits are exact in a float: 2^31, 2^32 and their negatives.
    constexpr T kTwo31 = T(2147483648.0);
    constexpr T kTwo32 = T(4294967296.0);
    if (is_signed)
    {
        if (rounded >= kTwo31)
        {
            return {std::uint32_t(INT32_MAX), kInvalid};
        }
        if (rounded < -kTwo31)
        {
            return {std::uint32_t(INT32_MIN), kInvalid};
        }
        return {std::uint32_t(std::int32_t(rounded)), rounded != a ? kInexact : 0};
    }
    if (rounded >= kTwo32)
    {
        return {UINT32_MAX, kInvalid};
    }
    if (rounded <= T(-1))
    {
        return {0, kInvalid};
    }
    return {std::uint32_t(rounded), rounded != a ? kInexact : 0};
}

/// A 32 bit integer to a float, rounded in the mode of the FPCR.
template <class T>
Result from_int_op(U8 fn, U64 abits, word rounding)
{
    HostFpu host(rounding);
    volatile std::int32_t vs = std::int32_t(std::uint32_t(abits));
    volatile std::uint32_t vu = std::uint32_t(abits);
    volatile T result = fn == kUnaryFn_froms32 ? T(vs) : T(vu);
    const T value = result;
    return {to_bits(value), host.flags()};
}

} // namespace

Result binary(const U8 fn, const bool dbl, const U64 a, const U64 b, const word rounding)
{
    return dbl ? binary_op<double>(fn, a, b, rounding) : binary_op<float>(fn, a, b, rounding);
}

Result fused(const U8 fn, const bool dbl, const U64 a, const U64 b, const U64 c,
              const word rounding)
{
    return dbl ? fused_op<double>(fn, a, b, c, rounding) : fused_op<float>(fn, a, b, c, rounding);
}

Result unary(const U8 fn, const bool dbl, const U64 a, const word rounding)
{
    switch (fn)
    {
    case kUnaryFn_abs:
    case kUnaryFn_neg:
        return dbl ? sign_op<double>(fn, a) : sign_op<float>(fn, a);
    case kUnaryFn_sqrt:
        return dbl ? sqrt_op<double>(a, rounding) : sqrt_op<float>(a, rounding);
    case kUnaryFn_rint:
    case kUnaryFn_rintz:
    case kUnaryFn_rintm:
    case kUnaryFn_rintp:
    case kUnaryFn_rinta:
        return dbl ? rint_op<double>(fn, a, rounding) : rint_op<float>(fn, a, rounding);
    case kUnaryFn_tos32:
    case kUnaryFn_tou32:
    case kUnaryFn_tos32r:
    case kUnaryFn_tou32r:
        return dbl ? to_int_op<double>(fn, a, rounding) : to_int_op<float>(fn, a, rounding);
    case kUnaryFn_froms32:
    case kUnaryFn_fromu32:
        return dbl ? from_int_op<double>(fn, a, rounding) : from_int_op<float>(fn, a, rounding);
    default: // kUnaryFn_fcvt
        break;
    }

    if (!dbl)
    {
        // float to double: always exact
        const float f = from_bits<float>(a);
        if (f != f)
        {
            return {Format<double>::kDefaultNan, is_signaling_nan(f) ? kInvalid : 0};
        }
        return {to_bits(double(f)), 0};
    }

    HostFpu host(rounding);
    volatile double vd = from_bits<double>(a);
    volatile float result = float(vd);
    const float f = result;
    return {canonical(f), host.flags()};
}

Result compare(const bool dbl, const bool signaling, const U64 a, const U64 b)
{
    constexpr U64 kN = 0b1000, kZ = 0b0100, kC = 0b0010, kV = 0b0001;

    auto compare_as = [&]<class T>(T) -> Result
    {
        const T x = from_bits<T>(a);
        const T y = from_bits<T>(b);
        if (x != x || y != y)
        {
            const bool invalid = signaling || is_signaling_nan(x) || is_signaling_nan(y);
            return {kC | kV, invalid ? kInvalid : 0};
        }
        if (x == y)
        {
            return {kZ | kC, 0};
        }
        return {x < y ? kN : kC, 0};
    };
    return dbl ? compare_as(double{}) : compare_as(float{});
}

} // namespace fpu
