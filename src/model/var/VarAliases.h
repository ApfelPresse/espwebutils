#pragma once

#include "Var.h"

namespace fj {

// ---- Convenience type aliases for common Var configurations ----
// Convention: Var + WsMode (Ws/Meta) + PrefsMode (Prefs/nothing) + WriteMode (Rw/Ro)
// If PrefsMode::Off, omit "Prefs" from name
//
// Examples:
//   VarWsPrefsRw   = value broadcast + persistent + writable (most common)
//   VarWsRo        = value broadcast + read-only (display-only)
//   VarMetaPrefsRw = metadata only + persistent + writable (secure passwords, Variant A)

template <typename T> using VarWsPrefsRw   = Var<T, WsMode::Value, PrefsMode::On,  WriteMode::On>;
template <typename T> using VarWsRw        = Var<T, WsMode::Value, PrefsMode::Off, WriteMode::On>;
template <typename T> using VarWsPrefsRo   = Var<T, WsMode::Value, PrefsMode::On,  WriteMode::Off>;
template <typename T> using VarWsRo        = Var<T, WsMode::Value, PrefsMode::Off, WriteMode::Off>;
template <typename T> using VarMetaPrefsRw = Var<T, WsMode::Meta,  PrefsMode::On,  WriteMode::On>;
template <typename T> using VarMetaRw      = Var<T, WsMode::Meta,  PrefsMode::Off, WriteMode::On>;
template <typename T> using VarMetaPrefsRo = Var<T, WsMode::Meta,  PrefsMode::On,  WriteMode::Off>;
template <typename T> using VarMetaRo      = Var<T, WsMode::Meta,  PrefsMode::Off, WriteMode::Off>;

} // namespace fj
