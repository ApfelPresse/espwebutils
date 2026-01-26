#pragma once

#include <Arduino.h>

namespace fj {

// Policy flags that control per-field behavior.
//
// - WsMode::Value: send the real value over WebSocket
// - WsMode::Meta:  send metadata only (e.g. {type:"secret", initialized:...})
// - WsMode::None:  omit from WebSocket output
//
// - PrefsMode::On:  persist to preferences
// - PrefsMode::Off: do not persist
//
// - WriteMode::On:  accept incoming remote updates
// - WriteMode::Off: treat as read-only

enum class WsMode    : uint8_t { Value, Meta, None };
enum class PrefsMode : uint8_t { On, Off };
enum class WriteMode : uint8_t { On, Off };

} // namespace fj
