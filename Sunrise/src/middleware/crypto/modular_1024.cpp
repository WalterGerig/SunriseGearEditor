#include "modular_1024.h"

#include <Windows.h>

#include <intrin.h>

namespace sunrise::middleware::crypto::modular {

namespace {

/** Bits in one limb. */
constexpr std::size_t kLimbBits = 64;
/** Bits in the whole number, which is also the Montgomery radix exponent. */
constexpr std::size_t kValueBits = kLimbBits * kLimbCount;
/** Bytes in one limb. */
constexpr std::size_t kLimbBytes = sizeof(std::uint64_t);
/** Bits in one byte of the big-endian wire form. */
constexpr std::size_t kByteBits = 8;
/** Mask of one wire byte. */
constexpr std::uint64_t kByteMask = 0xFF;
/** Montgomery reduction needs two limbs of headroom above the product. */
constexpr std::size_t kProductLimbs = kLimbCount + 2;
/** Each Newton round doubles the correct bits, so 6 lift one bit to the full limb. */
constexpr unsigned kInverseRounds = 6;

/**
 * Multiplies and accumulates one limb.
 * @param left First factor.
 * @param right Second factor.
 * @param addend Value added to the product.
 * @param carry Carried in, and replaced by the high half.
 * @return Low half of left*right + addend + carry.
 */
[[nodiscard]] std::uint64_t multiply_accumulate(std::uint64_t left,
                                                std::uint64_t right,
                                                std::uint64_t addend,
                                                std::uint64_t& carry) noexcept {
    std::uint64_t high = 0;
    std::uint64_t low = _umul128(left, right, &high);
    // Two 64-bit addends cannot overflow the 128-bit product of two 64-bit factors.
    high += _addcarry_u64(0, low, addend, &low);
    high += _addcarry_u64(0, low, carry, &low);
    carry = high;
    return low;
}

/**
 * Subtracts the modulus once.
 * @param value Limbs reduced in place.
 * @param modulus Modulus limbs.
 * @return Borrow out of the most significant limb.
 */
[[nodiscard]] unsigned char subtract_modulus(Number& value, const Number& modulus) noexcept {
    unsigned char borrow = 0;
    for (std::size_t index = 0; index < kLimbCount; ++index) {
        borrow = _subborrow_u64(borrow, value[index], modulus[index], &value[index]);
    }
    return borrow;
}

/**
 * Conditionally subtracts the modulus so the result stays reduced.
 * @param value Limbs to reduce in place.
 * @param modulus Modulus limbs.
 * @param overflow Carry that left the most significant limb.
 */
void reduce_once(Number& value, const Number& modulus, unsigned char overflow) noexcept {
    if (overflow != 0 || !less_than(value, modulus)) {
        Number reduced = value;
        // The borrow is discarded: an overflowing sum is exactly one modulus too large.
        (void)subtract_modulus(reduced, modulus);
        value = reduced;
        SecureZeroMemory(reduced.data(), reduced.size() * kLimbBytes);
    }
}

/**
 * Computes the negated inverse of one limb modulo two to the power of 64.
 * @param limb Odd limb 0 of the modulus.
 * @return Value whose product with the limb is the all-ones limb plus one.
 */
[[nodiscard]] std::uint64_t negated_inverse(std::uint64_t limb) noexcept {
    std::uint64_t inverse = 1;
    for (unsigned round = 0; round < kInverseRounds; ++round) {
        inverse *= 2 - limb * inverse;
    }
    return 0 - inverse;
}

/**
 * Multiplies in the Montgomery domain.
 * @param modulus Prepared modulus.
 * @param left Value below the modulus.
 * @param right Value below the modulus.
 * @param output Receives left*right divided by the radix, reduced.
 */
void montgomery_multiply(const Modulus& modulus,
                         const Number& left,
                         const Number& right,
                         Number& output) noexcept {
    std::array<std::uint64_t, kProductLimbs> product{};
    for (std::size_t outer = 0; outer < kLimbCount; ++outer) {
        std::uint64_t carry = 0;
        for (std::size_t inner = 0; inner < kLimbCount; ++inner) {
            product[inner] = multiply_accumulate(left[inner], right[outer], product[inner], carry);
        }
        const unsigned char spill =
            _addcarry_u64(0, product[kLimbCount], carry, &product[kLimbCount]);
        product[kLimbCount + 1] = spill;

        // One reduction step clears limb 0, so the window slides down by one limb.
        const std::uint64_t factor = product[0] * modulus.inverse;
        carry = 0;
        (void)multiply_accumulate(factor, modulus.value[0], product[0], carry);
        for (std::size_t inner = 1; inner < kLimbCount; ++inner) {
            product[inner - 1] =
                multiply_accumulate(factor, modulus.value[inner], product[inner], carry);
        }
        const unsigned char last =
            _addcarry_u64(0, product[kLimbCount], carry, &product[kLimbCount - 1]);
        product[kLimbCount] = product[kLimbCount + 1] + last;
    }
    Number result{};
    for (std::size_t index = 0; index < kLimbCount; ++index) {
        result[index] = product[index];
    }
    reduce_once(result, modulus.value, static_cast<unsigned char>(product[kLimbCount] != 0));
    output = result;
    // Both locals hold the value, which is secret on the exponent paths.
    SecureZeroMemory(product.data(), product.size() * kLimbBytes);
    SecureZeroMemory(result.data(), result.size() * kLimbBytes);
}

/**
 * Doubles a reduced value.
 * @param modulus Prepared modulus.
 * @param value Value below the modulus, doubled in place.
 */
void double_value(const Modulus& modulus, Number& value) noexcept {
    unsigned char carry = 0;
    for (std::size_t index = 0; index < kLimbCount; ++index) {
        carry = _addcarry_u64(carry, value[index], value[index], &value[index]);
    }
    reduce_once(value, modulus.value, carry);
}

/** @return Bit at the given position, counted from the least significant. */
[[nodiscard]] bool bit_at(const Number& value, std::size_t position) noexcept {
    const std::uint64_t limb = value[position / kLimbBits];
    return ((limb >> (position % kLimbBits)) & 1U) != 0;
}

} // namespace

/** Reads a fixed-width big-endian integer. */
void import_big_endian(std::span<const std::byte, kByteSize> input, Number& output) noexcept {
    output = {};
    for (std::size_t index = 0; index < kByteSize; ++index) {
        const std::size_t limb = (kByteSize - 1 - index) / kLimbBytes;
        const std::size_t shift = ((kByteSize - 1 - index) % kLimbBytes) * kByteBits;
        output[limb] |= static_cast<std::uint64_t>(std::to_integer<unsigned char>(input[index]))
                        << shift;
    }
}

/** Writes a fixed-width big-endian integer. */
void export_big_endian(const Number& value, std::span<std::byte, kByteSize> output) noexcept {
    for (std::size_t index = 0; index < kByteSize; ++index) {
        const std::size_t limb = (kByteSize - 1 - index) / kLimbBytes;
        const std::size_t shift = ((kByteSize - 1 - index) % kLimbBytes) * kByteBits;
        output[index] = static_cast<std::byte>((value[limb] >> shift) & kByteMask);
    }
}

/** Sets one number to a small constant. */
void set_small(Number& output, std::uint64_t value) noexcept {
    output = {};
    output[0] = value;
}

/** Reports an all-zero value. */
bool is_zero(const Number& value) noexcept {
    for (const std::uint64_t limb : value) {
        if (limb != 0) {
            return false;
        }
    }
    return true;
}

/** Compares two values. */
bool less_than(const Number& left, const Number& right) noexcept {
    for (std::size_t index = kLimbCount; index > 0; --index) {
        const std::uint64_t high = left[index - 1];
        const std::uint64_t low = right[index - 1];
        if (high != low) {
            return high < low;
        }
    }
    return false;
}

/** Derives the Montgomery constants for one modulus. */
bool prepare(const Number& modulus, Modulus& output) noexcept {
    const bool odd = (modulus[0] & 1U) != 0;
    const bool normalized = (modulus[kLimbCount - 1] >> (kLimbBits - 1)) != 0;
    if (!odd || !normalized) {
        return false;
    }
    Modulus candidate{};
    candidate.value = modulus;
    candidate.inverse = negated_inverse(modulus[0]);

    // The radix is below twice a normalized modulus, so its residue is one subtraction away.
    Number residue{};
    unsigned char borrow = 0;
    for (std::size_t index = 0; index < kLimbCount; ++index) {
        borrow = _subborrow_u64(borrow, 0, modulus[index], &residue[index]);
    }
    // Doubling the radix residue once per bit of the radix yields the squared factor.
    for (std::size_t round = 0; round < kValueBits; ++round) {
        double_value(candidate, residue);
    }
    candidate.montgomeryFactor = residue;
    candidate.ready = true;
    output = candidate;
    return true;
}

/** Adds two reduced values. */
void add(const Modulus& modulus, const Number& left, const Number& right, Number& output) noexcept {
    Number result{};
    unsigned char carry = 0;
    for (std::size_t index = 0; index < kLimbCount; ++index) {
        carry = _addcarry_u64(carry, left[index], right[index], &result[index]);
    }
    reduce_once(result, modulus.value, carry);
    output = result;
    SecureZeroMemory(result.data(), result.size() * kLimbBytes);
}

/** Multiplies two reduced values. */
void multiply(const Modulus& modulus,
              const Number& left,
              const Number& right,
              Number& output) noexcept {
    Number product{};
    montgomery_multiply(modulus, left, right, product);
    montgomery_multiply(modulus, product, modulus.montgomeryFactor, output);
    SecureZeroMemory(product.data(), product.size() * kLimbBytes);
}

/** Raises a reduced value to a power. */
void power(const Modulus& modulus,
           const Number& base,
           const Number& exponent,
           Number& output) noexcept {
    Number one{};
    set_small(one, 1);
    Number accumulator{};
    montgomery_multiply(modulus, one, modulus.montgomeryFactor, accumulator);
    Number factor{};
    montgomery_multiply(modulus, base, modulus.montgomeryFactor, factor);

    std::size_t highest = 0;
    for (std::size_t position = kValueBits; position > 0; --position) {
        if (bit_at(exponent, position - 1)) {
            highest = position;
            break;
        }
    }
    for (std::size_t position = highest; position > 0; --position) {
        montgomery_multiply(modulus, accumulator, accumulator, accumulator);
        if (bit_at(exponent, position - 1)) {
            montgomery_multiply(modulus, accumulator, factor, accumulator);
        }
    }
    Number one_montgomery{};
    set_small(one_montgomery, 1);
    montgomery_multiply(modulus, accumulator, one_montgomery, output);
    SecureZeroMemory(accumulator.data(), accumulator.size() * kLimbBytes);
    SecureZeroMemory(factor.data(), factor.size() * kLimbBytes);
}

} // namespace sunrise::middleware::crypto::modular
