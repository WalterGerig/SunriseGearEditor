#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::state::build_data::vendors {

/** Stack slots one expression may use. Known gates need two, so this leaves headroom. */
inline constexpr std::size_t kExpressionStackCapacity = 16;

/** Opcodes the client's expression evaluator implements. */
enum class Opcode : std::uint8_t {
    /** Pushes true when the flag's logical byte is `kFlagActive`. Operand is the flag slot. */
    flag = 1,
    /** Pops one boolean and pushes its negation. */
    logicalNot = 2,
    /** Pushes one progression value. Operand is the value slot. */
    loadValue = 10,
    /** Pushes the operand as a constant. */
    constant = 11,
    /** Pops right then left and pushes left less than right. */
    lessThan = 15,
};

/** A flag is active only for this logical byte. Logical 0 and 1 are both inactive. */
inline constexpr std::uint8_t kFlagActive = 2;

/** One expression instruction. Opcodes with no operand leave it unread. */
struct Instruction {
    Opcode opcode{};
    std::uint16_t operand{};
};

/** Reads one flag's logical byte. Returns false when the slot has no value. */
using FlagReader = bool (*)(void* context, std::uint16_t slot, std::uint8_t& logical) noexcept;
/** Reads one progression value. Returns false when the slot has no value. */
using ValueReader = bool (*)(void* context, std::uint16_t slot, std::int32_t& value) noexcept;

/** The two readers one evaluation needs, and the caller state they share. */
struct Inputs {
    FlagReader flag{};
    ValueReader value{};
    void* context{};
};

/**
 * Evaluates one expression program.
 * Every failure is refused, never defaulted: bad opcode, bad stack, bad kind, unreadable slot.
 * @param program Instructions in evaluation order.
 * @param inputs Flag and value readers.
 * @param result Receives the expression result, and is cleared on any refusal.
 * @return True only when the whole program evaluated to one boolean.
 */
[[nodiscard]] bool
evaluate(std::span<const Instruction> program, const Inputs& inputs, bool& result) noexcept;

} // namespace sunrise::state::build_data::vendors
