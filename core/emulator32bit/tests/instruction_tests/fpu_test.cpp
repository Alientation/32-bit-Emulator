// Tests for the floating point instructions (vop1, vop2, vcmp, docs/isa.md, "Floating point").
//
// The expected results are of two kinds: values worked out by hand (the IEEE 754 results that
// anybody can check), and a differential check against a plain C++ computation on the host with
// its rounding mode and flags set and read through <cfenv>.

#include "emulator32bit/fpu.h"
#include "emulator32bit_test/emulator32bit_test.h"

#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <random>
#include <sstream>

namespace
{
using namespace fpu;

constexpr U8 kXa = 2; // the first operand: x2 (x2, x3 for a double)
constexpr U8 kXb = 4; // the second one: x4 (x4, x5)
constexpr U8 kXd = 6; // the destination: x6 (x6, x7)

constexpr word kModes[] = {kRoundNearest, kRoundUp, kRoundDown, kRoundZero};

constexpr word kQuietNan32 = 0x7FC00000;
constexpr word kSignalingNan32 = 0x7F800001;
constexpr U64 kQuietNan64 = 0x7FF8000000000000ull;
constexpr U64 kSignalingNan64 = 0x7FF0000000000001ull;

word bits(const float f)
{
    return std::bit_cast<word>(f);
}

U64 bits(const double d)
{
    return std::bit_cast<U64>(d);
}

/// The value of an operation: the bits of the result (or the integer, or NZCV) and the FPSR it
/// raised.
struct Out
{
    U64 value;
    word flags;

    bool operator==(const Out &other) const
    {
        return value == other.value && flags == other.flags;
    }
};

std::ostream &operator<<(std::ostream &os, const Out &out)
{
    return os << "{value=0x" << std::hex << out.value << " flags=0b" << std::dec
              << ((out.flags >> 4) & 1) << ((out.flags >> 3) & 1) << ((out.flags >> 2) & 1)
              << ((out.flags >> 1) & 1) << (out.flags & 1) << " (IXC UFC OFC DZC IOC)}";
}

class FpuTest : public EmulatorFixture
{
  protected:
    void set_value(const U8 reg, const bool pair, const U64 value)
    {
        cpu.write_reg(reg, word(value));
        if (pair) cpu.write_reg(U8(reg + 1), word(value >> 32));
    }

    U64 get_value(const U8 reg, const bool pair)
    {
        return pair ? (U64(cpu.read_reg(U8(reg + 1))) << 32) | cpu.read_reg(reg)
                    : U64(cpu.read_reg(reg));
    }

    word fpsr()
    {
        return cpu.read_sysreg(Emulator32bit::kSysregId_fpsr);
    }

    void set_rounding(const word mode)
    {
        cpu.write_sysreg(Emulator32bit::kSysregId_fpcr, mode);
    }

    void start(const word rounding)
    {
        set_rounding(rounding);
        cpu.write_sysreg(Emulator32bit::kSysregId_fpsr, 0);
    }

    /// vop2: xd = xa fn xb
    Out binary(const U8 fn, const bool dbl, const U64 a, const U64 b, const word rounding = 0)
    {
        start(rounding);
        set_value(kXa, dbl, a);
        set_value(kXb, dbl, b);
        execute(Emulator32bit::asm_vop2(fn, dbl, kXd, kXa, kXb));
        return {get_value(kXd, dbl), fpsr()};
    }

    /// vop1: xd = fn(xa)
    Out unary(const U8 fn, const bool dbl, const U64 a, const word rounding = 0)
    {
        start(rounding);
        set_value(kXa, unary_source_is_pair(fn, dbl), a);
        execute(Emulator32bit::asm_vop1(fn, dbl, kXd, kXa));
        return {get_value(kXd, unary_dest_is_pair(fn, dbl)), fpsr()};
    }

    /// vcmp: NZCV (bit 3 is N)
    Out compare(const bool dbl, const bool signaling, const U64 a, const U64 b)
    {
        start(kRoundNearest);
        set_value(kXa, dbl, a);
        set_value(kXb, dbl, b);
        execute(Emulator32bit::asm_vcmp(dbl, signaling, kXa, kXb));
        const NZCVFlags f = flags();
        return {word(f.n) << 3 | word(f.z) << 2 | word(f.c) << 1 | word(f.v), fpsr()};
    }
};

constexpr word kEq = 0b0110, kLess = 0b1000, kGreater = 0b0010, kUnordered = 0b0011;

// ----- the host, as the oracle -----

word host_flags()
{
    const int raised = std::fetestexcept(FE_ALL_EXCEPT);
    return ((raised & FE_INVALID) ? kInvalid : 0) | ((raised & FE_DIVBYZERO) ? kDivByZero : 0)
           | ((raised & FE_OVERFLOW) ? kOverflow : 0) | ((raised & FE_UNDERFLOW) ? kUnderflow : 0)
           | ((raised & FE_INEXACT) ? kInexact : 0);
}

int host_mode(const word mode)
{
    constexpr int kHost[] = {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};
    return kHost[mode];
}

template <class T>
U64 canonical_bits(const T value)
{
    if (value != value)
    {
        return sizeof(T) == 4 ? U64(kQuietNan32) : kQuietNan64;
    }
    return std::bit_cast<std::conditional_t<sizeof(T) == 4, word, U64>>(value);
}

template <class T>
T value_of(const U64 b)
{
    return std::bit_cast<T>(std::conditional_t<sizeof(T) == 4, word, U64>(b));
}

/// One operation on the host: the result and the flags, in the given mode.
template <class T>
[[gnu::noinline]] Out host_binary(const U8 fn, const T a, const T b, const word rounding)
{
    std::fesetround(host_mode(rounding));
    std::feclearexcept(FE_ALL_EXCEPT);
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
    default:
        result = va / vb;
        break;
    }
    const word raised = host_flags();
    const T value = result;
    std::fesetround(FE_TONEAREST);
    return {canonical_bits(value), raised};
}

template <class T>
[[gnu::noinline]] Out host_sqrt(const T a, const word rounding)
{
    std::fesetround(host_mode(rounding));
    std::feclearexcept(FE_ALL_EXCEPT);
    volatile T va = a;
    volatile T result = std::sqrt(T(va));
    const word raised = host_flags();
    const T value = result;
    std::fesetround(FE_TONEAREST);
    return {canonical_bits(value), raised};
}

// The names of the operations, for the messages.
const char *binary_name(const U8 fn)
{
    constexpr const char *kNames[] = {"vadd", "vsub", "vmul", "vdiv", "vmin", "vmax"};
    return kNames[fn];
}

std::string describe(const U8 fn, const bool dbl, const U64 a, const U64 b, const word rounding)
{
    std::ostringstream os;
    os << binary_name(fn) << (dbl ? ".f64" : ".f32") << " a=0x" << std::hex << a << " b=0x" << b
       << std::dec << " rounding=" << rounding;
    return os.str();
}

} // namespace

// ----- vop2 -----

TEST_F(FpuTest, binary_results_worked_out_by_hand)
{
    struct Case
    {
        U8 fn;
        float a, b;
        float expected;
        word flags;
    };
    const float inf = std::numeric_limits<float>::infinity();
    const float max = std::numeric_limits<float>::max();
    const Case cases[] = {
        {kBinaryFn_add, 1.0f, 2.0f, 3.0f, 0},
        {kBinaryFn_add, 0.1f, 0.2f, 0.3f, kInexact},
        {kBinaryFn_sub, 5.0f, 1.5f, 3.5f, 0},
        {kBinaryFn_sub, 1.0f, 1.0f, 0.0f, 0},
        {kBinaryFn_mul, 1.5f, -4.0f, -6.0f, 0},
        {kBinaryFn_mul, max, 2.0f, inf, kOverflow | kInexact},
        {kBinaryFn_div, 1.0f, 4.0f, 0.25f, 0},
        {kBinaryFn_div, 1.0f, 0.0f, inf, kDivByZero},
        {kBinaryFn_div, -1.0f, 0.0f, -inf, kDivByZero},
        {kBinaryFn_div, 1.0f, 3.0f, 1.0f / 3.0f, kInexact},
        {kBinaryFn_add, inf, inf, inf, 0},
        {kBinaryFn_mul, 1e30f, 1e30f, inf, kOverflow | kInexact},
    };
    for (const Case &c : cases)
    {
        const Out out = binary(c.fn, false, bits(c.a), bits(c.b));
        EXPECT_EQ(out, (Out{bits(c.expected), c.flags}))
            << binary_name(c.fn) << ".f32 " << c.a << ", " << c.b;
    }
}

TEST_F(FpuTest, a_denormal_result_that_loses_bits_is_an_underflow)
{
    // The smallest denormal times a half is half of it: a tie that rounds to even, to zero.
    EXPECT_EQ(binary(kBinaryFn_mul, false, 0x00000001, bits(0.5f)),
              (Out{0, kUnderflow | kInexact}));
    // Exactly representable, so nothing is raised.
    EXPECT_EQ(binary(kBinaryFn_mul, false, 0x00000002, bits(0.5f)), (Out{1, 0}));
}

TEST_F(FpuTest, a_nan_result_is_always_the_default_nan)
{
    const word inf = bits(std::numeric_limits<float>::infinity());

    // Invalid operations
    EXPECT_EQ(binary(kBinaryFn_div, false, 0, 0), (Out{kQuietNan32, kInvalid}));
    EXPECT_EQ(binary(kBinaryFn_sub, false, inf, inf), (Out{kQuietNan32, kInvalid}));
    EXPECT_EQ(binary(kBinaryFn_mul, false, inf, 0), (Out{kQuietNan32, kInvalid}));

    // A signaling NaN is invalid and becomes quiet, a quiet one passes silently and loses its
    // payload (and the sign).
    EXPECT_EQ(binary(kBinaryFn_add, false, kSignalingNan32, bits(1.0f)),
              (Out{kQuietNan32, kInvalid}));
    EXPECT_EQ(binary(kBinaryFn_add, false, 0xFFC00123, bits(1.0f)), (Out{kQuietNan32, 0}));
    EXPECT_EQ(binary(kBinaryFn_add, true, kSignalingNan64, bits(1.0)),
              (Out{kQuietNan64, kInvalid}));
    EXPECT_EQ(binary(kBinaryFn_div, true, 0, 0), (Out{kQuietNan64, kInvalid}));
}

TEST_F(FpuTest, binary_results_of_a_double)
{
    EXPECT_EQ(binary(kBinaryFn_add, true, bits(0.1), bits(0.2)), (Out{0x3FD3333333333334ull, kInexact}));
    EXPECT_EQ(binary(kBinaryFn_div, true, bits(1.0), bits(3.0)), (Out{0x3FD5555555555555ull, kInexact}));
    EXPECT_EQ(binary(kBinaryFn_mul, true, bits(1.5), bits(-2.0)), (Out{bits(-3.0), 0}));
    EXPECT_EQ(binary(kBinaryFn_div, true, bits(1.0), bits(0.0)),
              (Out{0x7FF0000000000000ull, kDivByZero}));
    EXPECT_EQ(binary(kBinaryFn_sub, true, bits(5.0), bits(0.5)), (Out{bits(4.5), 0}));
}

TEST_F(FpuTest, the_rounding_mode_of_the_fpcr_decides_an_inexact_result)
{
    // 1/3 is 0.0101...b: down it is ...AAA, up it is ...AAB, and to nearest the AAB is closer.
    struct Case
    {
        word mode;
        word expected_f32;
        U64 expected_f64;
    };
    const Case cases[] = {
        {kRoundNearest, 0x3EAAAAAB, 0x3FD5555555555555ull},
        {kRoundUp, 0x3EAAAAAB, 0x3FD5555555555556ull},
        {kRoundDown, 0x3EAAAAAA, 0x3FD5555555555555ull},
        {kRoundZero, 0x3EAAAAAA, 0x3FD5555555555555ull},
    };
    for (const Case &c : cases)
    {
        EXPECT_EQ(binary(kBinaryFn_div, false, bits(1.0f), bits(3.0f), c.mode),
                  (Out{c.expected_f32, kInexact}))
            << "f32, mode " << c.mode;
        EXPECT_EQ(binary(kBinaryFn_div, true, bits(1.0), bits(3.0), c.mode),
                  (Out{c.expected_f64, kInexact}))
            << "f64, mode " << c.mode;
    }

    // For a negative number up and down swap.
    EXPECT_EQ(binary(kBinaryFn_div, false, bits(-1.0f), bits(3.0f), kRoundUp),
              (Out{0xBEAAAAAA, kInexact}));
    EXPECT_EQ(binary(kBinaryFn_div, false, bits(-1.0f), bits(3.0f), kRoundDown),
              (Out{0xBEAAAAAB, kInexact}));

    // An overflow is infinity or the largest number, by the mode.
    const word max = bits(std::numeric_limits<float>::max());
    EXPECT_EQ(binary(kBinaryFn_mul, false, max, bits(2.0f), kRoundZero),
              (Out{max, kOverflow | kInexact}));
    EXPECT_EQ(binary(kBinaryFn_mul, false, max, bits(2.0f), kRoundUp),
              (Out{0x7F800000, kOverflow | kInexact}));
}

TEST_F(FpuTest, the_host_rounding_mode_is_not_changed)
{
    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    binary(kBinaryFn_div, false, bits(1.0f), bits(3.0f), kRoundZero);
    unary(kUnaryFn_rint, false, bits(2.5f), kRoundDown);
    unary(kUnaryFn_fromu32, false, 0xFFFFFFFF, kRoundUp);
    EXPECT_EQ(std::fegetround(), FE_TONEAREST);

    // And the host computes as it always did.
    volatile float one = 1.0f;
    volatile float three = 3.0f;
    volatile float quotient = one / three;
    EXPECT_EQ(bits(quotient), 0x3EAAAAABu);
}

TEST_F(FpuTest, binary_operations_agree_with_the_host)
{
    std::mt19937_64 rng(0xF10A7);
    const U8 functions[] = {kBinaryFn_add, kBinaryFn_sub, kBinaryFn_mul, kBinaryFn_div};

    // Random patterns, which are mostly huge or tiny or not numbers, and patterns of a modest
    // exponent, where the results are ordinary and cancel and round.
    const auto random_f32 = [&](bool modest)
    {
        word value = word(rng());
        if (modest)
        {
            value = (value & 0x807FFFFF) | ((100 + rng() % 56) << 23);
        }
        return value;
    };
    const auto random_f64 = [&](bool modest)
    {
        U64 value = rng();
        if (modest)
        {
            value = (value & 0x800FFFFFFFFFFFFFull) | ((U64(900) + rng() % 246) << 52);
        }
        return value;
    };

    for (int i = 0; i < 4000; i++)
    {
        const bool modest = i % 4 != 0;
        const word mode = kModes[i % 4];
        const U8 fn = functions[(i / 4) % 4];

        const word a32 = random_f32(modest), b32 = random_f32(modest);
        EXPECT_EQ(binary(fn, false, a32, b32, mode),
                  host_binary<float>(fn, value_of<float>(a32), value_of<float>(b32), mode))
            << describe(fn, false, a32, b32, mode);

        const U64 a64 = random_f64(modest), b64 = random_f64(modest);
        EXPECT_EQ(binary(fn, true, a64, b64, mode),
                  host_binary<double>(fn, value_of<double>(a64), value_of<double>(b64), mode))
            << describe(fn, true, a64, b64, mode);
    }
}

TEST_F(FpuTest, min_and_max_ignore_a_quiet_nan_like_fmin_and_fmax)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_EQ(binary(kBinaryFn_min, false, bits(1.0f), bits(2.0f)), (Out{bits(1.0f), 0}));
    EXPECT_EQ(binary(kBinaryFn_max, false, bits(1.0f), bits(2.0f)), (Out{bits(2.0f), 0}));
    EXPECT_EQ(binary(kBinaryFn_min, false, bits(-3.0f), bits(2.0f)), (Out{bits(-3.0f), 0}));
    EXPECT_EQ(binary(kBinaryFn_max, false, bits(-3.0f), bits(-2.0f)), (Out{bits(-2.0f), 0}));

    // A NaN operand is skipped, two of them are a NaN.
    EXPECT_EQ(binary(kBinaryFn_min, false, bits(nan), bits(2.0f)), (Out{bits(2.0f), 0}));
    EXPECT_EQ(binary(kBinaryFn_max, false, bits(5.0f), bits(nan)), (Out{bits(5.0f), 0}));
    EXPECT_EQ(binary(kBinaryFn_max, false, bits(nan), bits(nan)), (Out{kQuietNan32, 0}));

    // A signaling NaN is an error.
    EXPECT_EQ(binary(kBinaryFn_min, false, kSignalingNan32, bits(2.0f)),
              (Out{kQuietNan32, kInvalid}));

    // -0 is smaller than +0, whichever way round they come.
    EXPECT_EQ(binary(kBinaryFn_min, false, bits(0.0f), bits(-0.0f)), (Out{bits(-0.0f), 0}));
    EXPECT_EQ(binary(kBinaryFn_min, false, bits(-0.0f), bits(0.0f)), (Out{bits(-0.0f), 0}));
    EXPECT_EQ(binary(kBinaryFn_max, false, bits(0.0f), bits(-0.0f)), (Out{bits(0.0f), 0}));
    EXPECT_EQ(binary(kBinaryFn_max, false, bits(-0.0f), bits(0.0f)), (Out{bits(0.0f), 0}));

    EXPECT_EQ(binary(kBinaryFn_min, true, bits(1.5), bits(-1.5)), (Out{bits(-1.5), 0}));
    EXPECT_EQ(binary(kBinaryFn_max, true, bits(1.5), bits(-1.5)), (Out{bits(1.5), 0}));
}

// ----- vop1 -----

TEST_F(FpuTest, abs_and_neg_only_touch_the_sign_bit)
{
    EXPECT_EQ(unary(kUnaryFn_abs, false, bits(-2.5f)), (Out{bits(2.5f), 0}));
    EXPECT_EQ(unary(kUnaryFn_neg, false, bits(2.5f)), (Out{bits(-2.5f), 0}));
    EXPECT_EQ(unary(kUnaryFn_neg, false, bits(0.0f)), (Out{bits(-0.0f), 0}));
    EXPECT_EQ(unary(kUnaryFn_abs, true, bits(-2.5)), (Out{bits(2.5), 0}));
    EXPECT_EQ(unary(kUnaryFn_neg, true, bits(2.5)), (Out{bits(-2.5), 0}));

    // A NaN keeps its payload and a signaling one is not signaled.
    EXPECT_EQ(unary(kUnaryFn_neg, false, kSignalingNan32), (Out{kSignalingNan32 ^ 0x80000000, 0}));
    EXPECT_EQ(unary(kUnaryFn_abs, false, 0xFF800123), (Out{0x7F800123, 0}));
    EXPECT_EQ(unary(kUnaryFn_neg, true, kSignalingNan64), (Out{kSignalingNan64 ^ (1ull << 63), 0}));
}

TEST_F(FpuTest, sqrt)
{
    EXPECT_EQ(unary(kUnaryFn_sqrt, false, bits(2.0f)), (Out{0x3FB504F3, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_sqrt, true, bits(2.0)), (Out{0x3FF6A09E667F3BCDull, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_sqrt, false, bits(16.0f)), (Out{bits(4.0f), 0}));
    EXPECT_EQ(unary(kUnaryFn_sqrt, false, bits(-0.0f)), (Out{bits(-0.0f), 0}));
    EXPECT_EQ(unary(kUnaryFn_sqrt, false, bits(-1.0f)), (Out{kQuietNan32, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_sqrt, true, bits(-1.0)), (Out{kQuietNan64, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_sqrt, false, bits(std::numeric_limits<float>::infinity())),
              (Out{0x7F800000, 0}));

    // Rounding of the root of 2
    EXPECT_EQ(unary(kUnaryFn_sqrt, false, bits(2.0f), kRoundUp), (Out{0x3FB504F4, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_sqrt, false, bits(2.0f), kRoundZero), (Out{0x3FB504F3, kInexact}));
}

TEST_F(FpuTest, sqrt_agrees_with_the_host)
{
    std::mt19937_64 rng(0x5A27);
    for (int i = 0; i < 2000; i++)
    {
        const word mode = kModes[i % 4];
        const word a32 = (word(rng()) & 0x807FFFFF) | ((100 + rng() % 56) << 23);
        EXPECT_EQ(unary(kUnaryFn_sqrt, false, a32, mode),
                  host_sqrt<float>(value_of<float>(a32), mode))
            << "0x" << std::hex << a32 << " mode " << mode;
        const U64 a64 = (rng() & 0x800FFFFFFFFFFFFFull) | ((U64(900) + rng() % 246) << 52);
        EXPECT_EQ(unary(kUnaryFn_sqrt, true, a64, mode),
                  host_sqrt<double>(value_of<double>(a64), mode))
            << "0x" << std::hex << a64 << " mode " << mode;
    }
}

TEST_F(FpuTest, rounding_to_an_integral_value)
{
    struct Case
    {
        float in;
        float rint, rintz, rintm, rintp, rinta; // rint is to nearest even
    };
    const Case cases[] = {
        {2.5f, 2.0f, 2.0f, 2.0f, 3.0f, 3.0f},   {3.5f, 4.0f, 3.0f, 3.0f, 4.0f, 4.0f},
        {-2.5f, -2.0f, -2.0f, -3.0f, -2.0f, -3.0f}, {2.4f, 2.0f, 2.0f, 2.0f, 3.0f, 2.0f},
        {-0.4f, -0.0f, -0.0f, -1.0f, -0.0f, -0.0f}, {0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f},
        {7.0f, 7.0f, 7.0f, 7.0f, 7.0f, 7.0f},
    };
    for (const Case &c : cases)
    {
        EXPECT_EQ(unary(kUnaryFn_rint, false, bits(c.in)), (Out{bits(c.rint), 0})) << "rint " << c.in;
        EXPECT_EQ(unary(kUnaryFn_rintz, false, bits(c.in)), (Out{bits(c.rintz), 0})) << "rintz " << c.in;
        EXPECT_EQ(unary(kUnaryFn_rintm, false, bits(c.in)), (Out{bits(c.rintm), 0})) << "rintm " << c.in;
        EXPECT_EQ(unary(kUnaryFn_rintp, false, bits(c.in)), (Out{bits(c.rintp), 0})) << "rintp " << c.in;
        EXPECT_EQ(unary(kUnaryFn_rinta, false, bits(c.in)), (Out{bits(c.rinta), 0})) << "rinta " << c.in;
        EXPECT_EQ(unary(kUnaryFn_rintz, true, bits(double(c.in))), (Out{bits(double(c.rintz)), 0}));
    }

    // vrint follows the FPCR; the others do not.
    EXPECT_EQ(unary(kUnaryFn_rint, false, bits(2.5f), kRoundUp), (Out{bits(3.0f), 0}));
    EXPECT_EQ(unary(kUnaryFn_rint, false, bits(2.5f), kRoundDown), (Out{bits(2.0f), 0}));
    EXPECT_EQ(unary(kUnaryFn_rint, false, bits(-2.5f), kRoundZero), (Out{bits(-2.0f), 0}));
    EXPECT_EQ(unary(kUnaryFn_rintz, false, bits(2.5f), kRoundUp), (Out{bits(2.0f), 0}));

    // Infinity stays, a NaN is the default one.
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_EQ(unary(kUnaryFn_rintm, false, bits(-inf)), (Out{bits(-inf), 0}));
    EXPECT_EQ(unary(kUnaryFn_rint, false, kSignalingNan32), (Out{kQuietNan32, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_rinta, false, 0x7FC00007), (Out{kQuietNan32, 0}));
}

TEST_F(FpuTest, float_to_integer_truncates_like_a_c_cast)
{
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(1.9f)), (Out{1, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(-1.9f)), (Out{word(-1), kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(-0.5f)), (Out{0, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(42.0f)), (Out{42, 0}));
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(-2147483648.0f)), (Out{0x80000000, 0}));
    EXPECT_EQ(unary(kUnaryFn_tos32, true, bits(-1234567.75)), (Out{word(-1234567), kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tou32, false, bits(4000000000.0f)), (Out{4000000000u, 0}));
    EXPECT_EQ(unary(kUnaryFn_tou32, true, bits(4294967295.0)), (Out{0xFFFFFFFF, 0}));
    EXPECT_EQ(unary(kUnaryFn_tou32, false, bits(-0.5f)), (Out{0, kInexact}));

    // The FPCR does not matter.
    for (const word mode : kModes)
    {
        EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(-1.9f), mode), (Out{word(-1), kInexact}));
    }
}

TEST_F(FpuTest, float_to_integer_saturates_and_a_nan_is_zero)
{
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(3.0e9f)), (Out{0x7FFFFFFF, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(2147483648.0f)), (Out{0x7FFFFFFF, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(-3.0e9f)), (Out{0x80000000, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(inf)), (Out{0x7FFFFFFF, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tos32, false, bits(-inf)), (Out{0x80000000, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tos32, false, kQuietNan32), (Out{0, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tos32, true, bits(1e10)), (Out{0x7FFFFFFF, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tou32, false, bits(5.0e9f)), (Out{0xFFFFFFFF, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tou32, false, bits(-1.0f)), (Out{0, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tou32, true, bits(-1.5)), (Out{0, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tou32, true, kQuietNan64), (Out{0, kInvalid}));
}

TEST_F(FpuTest, float_to_integer_in_the_mode_of_the_fpcr)
{
    // vcvtr: 2.5 to nearest even is 2, 3.5 is 4.
    EXPECT_EQ(unary(kUnaryFn_tos32r, false, bits(2.5f), kRoundNearest), (Out{2, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tos32r, false, bits(3.5f), kRoundNearest), (Out{4, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tos32r, false, bits(2.5f), kRoundUp), (Out{3, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tos32r, false, bits(2.5f), kRoundDown), (Out{2, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tos32r, false, bits(-2.5f), kRoundDown), (Out{word(-3), kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tos32r, false, bits(-2.5f), kRoundZero), (Out{word(-2), kInexact}));
    EXPECT_EQ(unary(kUnaryFn_tos32r, false, bits(6.0f), kRoundUp), (Out{6, 0}));
    EXPECT_EQ(unary(kUnaryFn_tou32r, true, bits(2.5), kRoundUp), (Out{3, kInexact}));

    // Rounded first, then checked: -0.5 down is -1, which an unsigned number cannot hold.
    EXPECT_EQ(unary(kUnaryFn_tou32r, false, bits(-0.5f), kRoundDown), (Out{0, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_tou32r, false, bits(-0.5f), kRoundNearest), (Out{0, kInexact}));
}

TEST_F(FpuTest, integer_to_float_rounds_in_the_mode_of_the_fpcr)
{
    EXPECT_EQ(unary(kUnaryFn_froms32, false, word(-1)), (Out{bits(-1.0f), 0}));
    EXPECT_EQ(unary(kUnaryFn_froms32, false, 0), (Out{0, 0}));
    EXPECT_EQ(unary(kUnaryFn_fromu32, false, 0x80000000), (Out{bits(2147483648.0f), 0}));
    EXPECT_EQ(unary(kUnaryFn_froms32, true, word(-5)), (Out{bits(-5.0), 0}));
    EXPECT_EQ(unary(kUnaryFn_fromu32, true, 0xFFFFFFFF), (Out{bits(4294967295.0), 0}));

    // 16777217 = 2^24 + 1 does not fit in 24 bits.
    EXPECT_EQ(unary(kUnaryFn_froms32, false, 16777217, kRoundNearest), (Out{0x4B800000, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_froms32, false, 16777217, kRoundUp), (Out{0x4B800001, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_froms32, false, 16777217, kRoundZero), (Out{0x4B800000, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_fromu32, false, 0xFFFFFFFF, kRoundNearest),
              (Out{0x4F800000, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_fromu32, false, 0xFFFFFFFF, kRoundZero), (Out{0x4F7FFFFF, kInexact}));
}

TEST_F(FpuTest, converting_between_the_precisions)
{
    // float to double is exact
    EXPECT_EQ(unary(kUnaryFn_fcvt, false, bits(1.5f)), (Out{bits(1.5), 0}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, false, bits(-0.1f)), (Out{bits(double(-0.1f)), 0}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, false, 0x00000001), (Out{bits(double(std::bit_cast<float>(1u))), 0}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, false, 0x7F800000), (Out{0x7FF0000000000000ull, 0}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, false, kSignalingNan32), (Out{kQuietNan64, kInvalid}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, false, 0xFFC00001), (Out{kQuietNan64, 0}));

    // double to float rounds
    EXPECT_EQ(unary(kUnaryFn_fcvt, true, bits(0.1)), (Out{0x3DCCCCCD, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, true, bits(0.1), kRoundZero), (Out{0x3DCCCCCC, kInexact}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, true, bits(1.5)), (Out{bits(1.5f), 0}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, true, bits(1e300)), (Out{0x7F800000, kOverflow | kInexact}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, true, bits(1e300), kRoundZero),
              (Out{0x7F7FFFFF, kOverflow | kInexact}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, true, bits(1e-300)), (Out{0, kUnderflow | kInexact}));
    EXPECT_EQ(unary(kUnaryFn_fcvt, true, kSignalingNan64), (Out{kQuietNan32, kInvalid}));
}

// ----- vcmp -----

TEST_F(FpuTest, compare_sets_the_flags_like_a_comparison_of_integers)
{
    for (const bool dbl : {false, true})
    {
        const auto value = [&](const double d) { return dbl ? bits(d) : U64(bits(float(d))); };
        EXPECT_EQ(compare(dbl, false, value(1.0), value(1.0)), (Out{kEq, 0}));
        EXPECT_EQ(compare(dbl, false, value(1.0), value(2.0)), (Out{kLess, 0}));
        EXPECT_EQ(compare(dbl, false, value(2.0), value(1.0)), (Out{kGreater, 0}));
        EXPECT_EQ(compare(dbl, false, value(-2.0), value(1.0)), (Out{kLess, 0}));
        EXPECT_EQ(compare(dbl, false, value(0.0), value(-0.0)), (Out{kEq, 0})) << "+0 == -0";
        EXPECT_EQ(compare(dbl, false, value(-INFINITY), value(INFINITY)), (Out{kLess, 0}));
    }
}

TEST_F(FpuTest, compare_with_a_nan_is_unordered)
{
    // A quiet NaN is silent for vcmp and an error for vcmpe; a signaling one is an error for both.
    EXPECT_EQ(compare(false, false, kQuietNan32, bits(1.0f)), (Out{kUnordered, 0}));
    EXPECT_EQ(compare(false, false, bits(1.0f), kQuietNan32), (Out{kUnordered, 0}));
    EXPECT_EQ(compare(false, false, kQuietNan32, kQuietNan32), (Out{kUnordered, 0}));
    EXPECT_EQ(compare(false, true, kQuietNan32, bits(1.0f)), (Out{kUnordered, kInvalid}));
    EXPECT_EQ(compare(false, false, kSignalingNan32, bits(1.0f)), (Out{kUnordered, kInvalid}));
    EXPECT_EQ(compare(true, false, bits(1.0), kQuietNan64), (Out{kUnordered, 0}));
    EXPECT_EQ(compare(true, true, bits(1.0), kQuietNan64), (Out{kUnordered, kInvalid}));
    EXPECT_EQ(compare(true, false, kSignalingNan64, bits(1.0)), (Out{kUnordered, kInvalid}));
}

TEST_F(FpuTest, the_conditions_after_a_compare_follow_c_for_a_nan)
{
    struct Case
    {
        ConditionCode cond;
        const char *name;
        bool less, equal, greater, unordered;
    };
    // What the flags of vcmp make the condition: eq, mi (<), ls (<=), gt and ge are false for a
    // NaN, ne is true. (lt and le are true for a NaN, so a compiler uses mi and ls.)
    const Case cases[] = {
        {ConditionCode::EQ, "eq", false, true, false, false},
        {ConditionCode::NE, "ne", true, false, true, true},
        {ConditionCode::MI, "mi", true, false, false, false},
        {ConditionCode::LS, "ls", true, true, false, false},
        {ConditionCode::GT, "gt", false, false, true, false},
        {ConditionCode::GE, "ge", false, true, true, false},
        {ConditionCode::LT, "lt", true, false, false, true},
        {ConditionCode::LE, "le", true, true, false, true},
    };
    const word outcomes[] = {kLess, kEq, kGreater, kUnordered};
    for (const Case &c : cases)
    {
        const bool expected[] = {c.less, c.equal, c.greater, c.unordered};
        for (int i = 0; i < 4; i++)
        {
            const word nzcv = outcomes[i];
            const NZCVFlags f = {.n = bool(nzcv & 8), .z = bool(nzcv & 4), .c = bool(nzcv & 2),
                                 .v = bool(nzcv & 1)};
            set_flags(f);
            EXPECT_EQ(check_cond(cpu.get_pstate(), U8(c.cond)), expected[i])
                << c.name << " after outcome " << i;
        }
    }
}

// ----- registers -----

TEST_F(FpuTest, a_double_is_a_pair_with_the_low_word_in_the_lower_register)
{
    start(kRoundNearest);
    fill_registers();
    cpu.write_reg(2, word(bits(1.5)));
    cpu.write_reg(3, word(bits(1.5) >> 32));
    cpu.write_reg(4, word(bits(2.25)));
    cpu.write_reg(5, word(bits(2.25) >> 32));
    const auto before = snapshot_registers();

    execute(Emulator32bit::asm_vop2(kBinaryFn_add, true, 6, 2, 4));

    EXPECT_EQ(cpu.read_reg(6), word(bits(3.75)));
    EXPECT_EQ(cpu.read_reg(7), word(bits(3.75) >> 32));
    expect_registers_unchanged_except(before, {6, 7});
}

TEST_F(FpuTest, the_pairs_may_overlap)
{
    // vadd.f64 x2, x2, x3: the sources are the pairs (x2,x3) and (x3,x4), the destination (x2,x3).
    // Both are read before anything is written.
    start(kRoundNearest);
    cpu.write_reg(2, 0x00000000);
    cpu.write_reg(3, 0x3FF00000); // (x3,x2) is 1.0, and the low word of the second operand
    cpu.write_reg(4, 0x40000000); // (x4,x3) is 2.0 plus a little
    const U64 a = (U64(cpu.read_reg(3)) << 32) | cpu.read_reg(2);
    const U64 b = (U64(cpu.read_reg(4)) << 32) | cpu.read_reg(3);
    const Out expected =
        host_binary<double>(kBinaryFn_add, value_of<double>(a), value_of<double>(b), kRoundNearest);
    ASSERT_NE(value_of<double>(b), 2.0) << "the test needs the overlap to matter";

    execute(Emulator32bit::asm_vop2(kBinaryFn_add, true, 2, 2, 3));

    EXPECT_EQ(get_value(2, true), expected.value);
    EXPECT_EQ(cpu.read_reg(4), 0x40000000u) << "x4 was only read";
}

TEST_F(FpuTest, the_zero_register_is_zero_and_discards_a_result)
{
    start(kRoundNearest);
    fill_registers();
    cpu.write_reg(2, bits(2.5f));

    // xzr is +0.0
    execute(Emulator32bit::asm_vop2(kBinaryFn_add, false, 6, 2, U8(Register::XZR)));
    EXPECT_EQ(cpu.read_reg(6), bits(2.5f));
    execute(Emulator32bit::asm_vop2(kBinaryFn_div, false, 6, U8(Register::XZR), 2));
    EXPECT_EQ(cpu.read_reg(6), 0u);

    // A result written to xzr is dropped, the flags it raised are not.
    cpu.write_sysreg(Emulator32bit::kSysregId_fpsr, 0);
    execute(Emulator32bit::asm_vop2(kBinaryFn_div, false, U8(Register::XZR), 2, U8(Register::XZR)));
    EXPECT_EQ(cpu.read_reg(U8(Register::XZR)), 0u);
    EXPECT_EQ(fpsr(), kDivByZero);
}

TEST_F(FpuTest, a_conversion_takes_the_registers_of_its_types)
{
    start(kRoundNearest);
    fill_registers();
    const auto before = snapshot_registers();

    // double to int: a pair in, one register out
    set_value(kXa, true, bits(-7.9));
    execute(Emulator32bit::asm_vop1(kUnaryFn_tos32, true, kXd, kXa));
    EXPECT_EQ(cpu.read_reg(kXd), word(-7));
    expect_registers_unchanged_except(before, {kXd, kXa, U8(kXa + 1)});

    // int to double: one register in, a pair out
    fill_registers();
    cpu.write_reg(kXa, 100);
    const auto before2 = snapshot_registers();
    execute(Emulator32bit::asm_vop1(kUnaryFn_froms32, true, kXd, kXa));
    EXPECT_EQ(get_value(kXd, true), bits(100.0));
    expect_registers_unchanged_except(before2, {kXd, U8(kXd + 1)});

    // float to double and back
    fill_registers();
    cpu.write_reg(kXa, bits(0.5f));
    const auto before3 = snapshot_registers();
    execute(Emulator32bit::asm_vop1(kUnaryFn_fcvt, false, kXd, kXa)); // f32 -> f64
    EXPECT_EQ(get_value(kXd, true), bits(0.5));
    expect_registers_unchanged_except(before3, {kXd, U8(kXd + 1)});
    execute(Emulator32bit::asm_vop1(kUnaryFn_fcvt, true, kXa, kXd)); // f64 -> f32
    EXPECT_EQ(cpu.read_reg(kXa), bits(0.5f));
}

TEST_F(FpuTest, a_double_cannot_start_in_x29_sp_or_xzr)
{
    for (const U8 reg : {U8(29), U8(30), U8(31)})
    {
        const std::vector<std::pair<const char *, word>> instructions = {
            {"destination of vop2", Emulator32bit::asm_vop2(kBinaryFn_add, true, reg, 2, 4)},
            {"first source of vop2", Emulator32bit::asm_vop2(kBinaryFn_add, true, 6, reg, 4)},
            {"second source of vop2", Emulator32bit::asm_vop2(kBinaryFn_add, true, 6, 2, reg)},
            {"vcmp", Emulator32bit::asm_vcmp(true, false, reg, 4)},
            {"destination of a double result", Emulator32bit::asm_vop1(kUnaryFn_sqrt, true, reg, 2)},
            {"source of a double to int", Emulator32bit::asm_vop1(kUnaryFn_tos32, true, 6, reg)},
            {"destination of int to double", Emulator32bit::asm_vop1(kUnaryFn_froms32, true, reg, 2)},
            {"destination of float to double", Emulator32bit::asm_vop1(kUnaryFn_fcvt, false, reg, 2)},
            {"source of double to float", Emulator32bit::asm_vop1(kUnaryFn_fcvt, true, 6, reg)},
        };
        for (const auto &[what, instruction] : instructions)
        {
            start(kRoundNearest);
            fill_registers();
            const auto before = snapshot_registers();
            const NZCVFlags flags_before = flags();

            const auto result = step(0, instruction);

            EXPECT_EQ(result.status, Emulator32bit::RunResult::Status::FAULT) << what << " x" << int(reg);
            EXPECT_NE(result.message.find("register pair"), std::string::npos) << result.message;
            expect_registers_unchanged_except(before, {}, what);
            EXPECT_EQ(fpsr(), 0u) << what;
            EXPECT_EQ(flags(), flags_before) << what << " leaves the flags alone";
        }
    }

    // x28 is the last register that can start one, and a single does not care.
    start(kRoundNearest);
    execute(Emulator32bit::asm_vop2(kBinaryFn_add, true, 28, 2, 4));
    execute(Emulator32bit::asm_vop2(kBinaryFn_add, false, 29, 2, 4));
    execute(Emulator32bit::asm_vop2(kBinaryFn_add, false, U8(Register::SP), 2, 4));
    execute(Emulator32bit::asm_vop1(kUnaryFn_tos32, true, 29, 2));
}

TEST_F(FpuTest, an_unassigned_function_faults)
{
    fill_registers();
    const auto before = snapshot_registers();
    const auto result = step(0, Emulator32bit::asm_vop1(kUnaryFn_count, false, 6, 2));
    EXPECT_EQ(result.status, Emulator32bit::RunResult::Status::FAULT);
    EXPECT_NE(result.message.find("Undefined vop1 function 15"), std::string::npos)
        << result.message;
    expect_registers_unchanged_except(before, {});

    const auto result2 = step(0, Emulator32bit::asm_vop2(kBinaryFn_count, false, 6, 2, 4));
    EXPECT_EQ(result2.status, Emulator32bit::RunResult::Status::FAULT);
}

// ----- FPCR and FPSR -----

TEST_F(FpuTest, fpsr_collects_the_flags_until_it_is_written)
{
    start(kRoundNearest);
    set_value(kXa, false, bits(1.0f));
    set_value(kXb, false, bits(0.0f));
    execute(Emulator32bit::asm_vop2(kBinaryFn_div, false, kXd, kXa, kXb)); // divide by zero
    EXPECT_EQ(fpsr(), kDivByZero);

    set_value(kXb, false, bits(3.0f));
    execute(Emulator32bit::asm_vop2(kBinaryFn_div, false, kXd, kXa, kXb)); // inexact
    EXPECT_EQ(fpsr(), kDivByZero | kInexact) << "the first one is still there";

    set_value(kXb, false, bits(2.0f));
    execute(Emulator32bit::asm_vop2(kBinaryFn_div, false, kXd, kXa, kXb)); // exact
    EXPECT_EQ(fpsr(), kDivByZero | kInexact) << "an exact result clears nothing";

    cpu.write_sysreg(Emulator32bit::kSysregId_fpsr, 0);
    EXPECT_EQ(fpsr(), 0u);
}

TEST_F(FpuTest, fpcr_and_fpsr_keep_the_bits_that_exist)
{
    cpu.write_sysreg(Emulator32bit::kSysregId_fpcr, 0xFFFFFFFF);
    cpu.write_sysreg(Emulator32bit::kSysregId_fpsr, 0xFFFFFFFF);
    EXPECT_EQ(cpu.read_sysreg(Emulator32bit::kSysregId_fpcr), kFpcrMask);
    EXPECT_EQ(cpu.read_sysreg(Emulator32bit::kSysregId_fpsr), kFpsrMask);
    EXPECT_STREQ(Emulator32bit::sysreg_name(Emulator32bit::kSysregId_fpcr), "fpcr");
    EXPECT_STREQ(Emulator32bit::sysreg_name(Emulator32bit::kSysregId_fpsr), "fpsr");
    EXPECT_EQ(Emulator32bit::sysreg_id("FPSR"), Emulator32bit::kSysregId_fpsr);
}

TEST_F(FpuTest, a_reset_clears_the_floating_point_registers)
{
    cpu.write_sysreg(Emulator32bit::kSysregId_fpcr, kRoundZero);
    cpu.write_sysreg(Emulator32bit::kSysregId_fpsr, kInexact);
    cpu.reset();
    EXPECT_EQ(cpu.read_sysreg(Emulator32bit::kSysregId_fpcr), 0u);
    EXPECT_EQ(cpu.read_sysreg(Emulator32bit::kSysregId_fpsr), 0u);
}

TEST_F(FpuTest, only_vcmp_changes_the_integer_flags)
{
    // Everything but vcmp leaves NZCV as it was.
    set_flags(flags_from_bits(0b1010));
    binary(kBinaryFn_add, false, bits(1.0f), bits(2.0f));
    EXPECT_EQ(flags(), flags_from_bits(0b1010));
    unary(kUnaryFn_sqrt, true, bits(4.0));
    EXPECT_EQ(flags(), flags_from_bits(0b1010));
}
