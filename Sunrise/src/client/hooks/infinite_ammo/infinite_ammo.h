#pragma once

namespace sunrise::client::hooks::infinite_ammo {

/**
 * Attaches to the reserve, magazine and sword setters. While enabled, magazine and reserve counts
 * are held full and sword supply is kept topped up by its native setter.
 * @return True when all three resolved and the detours attached.
 */
[[nodiscard]] bool install() noexcept;

/** Detaches every detour. */
void uninstall() noexcept;

} // namespace sunrise::client::hooks::infinite_ammo
