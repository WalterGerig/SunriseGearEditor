#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::middleware::crypto::modular {

/** Direct-association integers are 1024 bits and travel as 128 big-endian bytes. */
inline constexpr std::size_t kByteSize = 128;
/** The value is held in 64-bit limbs, least significant first. */
inline constexpr std::size_t kLimbCount = kByteSize / sizeof(std::uint64_t);

/** One 1024-bit unsigned value, limb 0 least significant. */
using Number = std::array<std::uint64_t, kLimbCount>;

/**
 * One odd modulus and the Montgomery constants derived from it.
 * Build it once per process: preparing it costs far more than a single multiply.
 */
struct Modulus {
    Number value{};
    /** R squared modulo the value, where R is two to the power of 1024. */
    Number montgomeryFactor{};
    /** Negated inverse of limb 0 modulo two to the power of 64. */
    std::uint64_t inverse{};
    bool ready{};
};

/**
 * Reads a fixed-width big-endian integer.
 * @param input Exactly 128 bytes, left zero padded as the wire carries them.
 * @param output Receives the limbs.
 */
void import_big_endian(std::span<const std::byte, kByteSize> input, Number& output) noexcept;

/**
 * Writes a fixed-width big-endian integer.
 * @param value Number to write.
 * @param output Receives exactly 128 bytes, left zero padded.
 */
void export_big_endian(const Number& value, std::span<std::byte, kByteSize> output) noexcept;

/** Sets one number to a small constant. */
void set_small(Number& output, std::uint64_t value) noexcept;

/** @return True when every limb is zero. */
[[nodiscard]] bool is_zero(const Number& value) noexcept;

/** @return True when left is strictly below right. */
[[nodiscard]] bool less_than(const Number& left, const Number& right) noexcept;

/**
 * Derives the Montgomery constants for one modulus.
 * @param modulus Odd modulus with its most significant bit set.
 * @param output Receives the prepared modulus only on success.
 * @return True when the modulus is odd and normalized.
 */
[[nodiscard]] bool prepare(const Number& modulus, Modulus& output) noexcept;

/**
 * Adds two reduced values.
 * @param modulus Prepared modulus.
 * @param left Value below the modulus.
 * @param right Value below the modulus.
 * @param output Receives the reduced sum. Aliasing an input is allowed.
 */
void add(const Modulus& modulus, const Number& left, const Number& right, Number& output) noexcept;

/**
 * Multiplies two reduced values.
 * @param modulus Prepared modulus.
 * @param left Value below the modulus.
 * @param right Value below the modulus.
 * @param output Receives the reduced product. Aliasing an input is allowed.
 */
void multiply(const Modulus& modulus,
              const Number& left,
              const Number& right,
              Number& output) noexcept;

/**
 * Raises a reduced value to a power.
 * The exponent is read as a plain integer, so leading zero limbs cost nothing.
 * @param modulus Prepared modulus.
 * @param base Value below the modulus.
 * @param exponent Exponent, not reduced.
 * @param output Receives the reduced power. Aliasing an input is allowed.
 */
void power(const Modulus& modulus,
           const Number& base,
           const Number& exponent,
           Number& output) noexcept;

} // namespace sunrise::middleware::crypto::modular
