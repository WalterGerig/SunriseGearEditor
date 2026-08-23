#include "vendor_expression.h"

#include <array>

namespace sunrise::state::build_data::vendors {
namespace {

/** What one stack slot holds. The two kinds never substitute for each other. */
enum class Kind : std::uint8_t {
    boolean,
    number,
};

/** One typed stack slot. */
struct Slot {
    Kind kind{};
    bool boolean{};
    std::int32_t number{};
};

/** Fixed evaluation stack, which refuses rather than growing. */
class Stack final {
public:
    /** @param slot Value to push. @return True when there was room. */
    [[nodiscard]] bool push(const Slot& slot) noexcept {
        if (depth_ == slots_.size()) {
            return false;
        }
        slots_[depth_++] = slot;
        return true;
    }

    /** @param kind Required kind. @param slot Receives the value. @return True when it matched. */
    [[nodiscard]] bool pop(Kind kind, Slot& slot) noexcept {
        if (depth_ == 0 || slots_[depth_ - 1].kind != kind) {
            return false;
        }
        slot = slots_[--depth_];
        return true;
    }

    /** @return Slots currently held. */
    [[nodiscard]] std::size_t depth() const noexcept {
        return depth_;
    }

private:
    std::array<Slot, kExpressionStackCapacity> slots_{};
    std::size_t depth_{};
};

/** @param value Boolean result. @return The stack slot holding it. */
[[nodiscard]] Slot boolean_slot(bool value) noexcept {
    return {Kind::boolean, value, 0};
}

/** @param value Numeric result. @return The stack slot holding it. */
[[nodiscard]] Slot number_slot(std::int32_t value) noexcept {
    return {Kind::number, false, value};
}

/**
 * Runs one instruction against the stack.
 * @param instruction Instruction to run.
 * @param inputs Flag and value readers.
 * @param stack Evaluation stack.
 * @return True when the opcode is known and its operands were there in the right kind.
 */
[[nodiscard]] bool
step(const Instruction& instruction, const Inputs& inputs, Stack& stack) noexcept {
    Slot left{};
    Slot right{};
    switch (instruction.opcode) {
    case Opcode::flag: {
        std::uint8_t logical = 0;
        return inputs.flag(inputs.context, instruction.operand, logical)
               && stack.push(boolean_slot(logical == kFlagActive));
    }
    case Opcode::logicalNot:
        return stack.pop(Kind::boolean, left) && stack.push(boolean_slot(!left.boolean));
    case Opcode::loadValue: {
        std::int32_t value = 0;
        return inputs.value(inputs.context, instruction.operand, value)
               && stack.push(number_slot(value));
    }
    case Opcode::constant:
        return stack.push(number_slot(instruction.operand));
    case Opcode::lessThan:
        // The right operand was pushed last, so it comes off first.
        return stack.pop(Kind::number, right) && stack.pop(Kind::number, left)
               && stack.push(boolean_slot(left.number < right.number));
    default:
        return false;
    }
}

} // namespace

/** Evaluates one expression program. */
bool evaluate(std::span<const Instruction> program, const Inputs& inputs, bool& result) noexcept {
    result = false;
    if (program.empty() || inputs.flag == nullptr || inputs.value == nullptr) {
        return false;
    }
    Stack stack;
    for (const Instruction& instruction : program) {
        if (!step(instruction, inputs, stack)) {
            return false;
        }
    }
    Slot final{};
    if (stack.depth() != 1 || !stack.pop(Kind::boolean, final)) {
        return false;
    }
    result = final.boolean;
    return true;
}

} // namespace sunrise::state::build_data::vendors
