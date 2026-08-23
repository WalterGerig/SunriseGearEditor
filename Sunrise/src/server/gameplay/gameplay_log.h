#pragma once

#include "../../core/logging/log.h"

namespace sunrise::server::gameplay {

/**
 * Writes one bounded key-value event on the server channel.
 * @param level Severity, checked against the channel threshold before formatting.
 * @param format Printf-style format holding one event line.
 */
void report(core::log::Level level, const char* format, ...) noexcept;

} // namespace sunrise::server::gameplay
