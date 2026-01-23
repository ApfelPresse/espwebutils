# ESP32 Tests

Diese Tests werden direkt auf dem ESP32 ausgeführt.

## Test ausführen

```bash
# Build und Upload
pio run -e test_esp32s3 -t upload

# Monitor öffnen
pio device monitor -e test_esp32s3

# Oder beides in einem Befehl
pio run -e test_esp32s3 -t upload && pio device monitor -e test_esp32s3
```

## Struktur

```
test_esp32/
├── main.cpp              # Test-Hauptdatei (lädt und führt alle Tests aus)
├── test_helpers.h        # Test-Hilfsfunktionen und Makros
├── test_helpers.cpp      # Test-Hilfsimplementierung
└── model_type_test/      # Test-Suite für Model-Typen
    ├── test_model.h      # Tests für StaticString, VarMetaPrefsRw, Var, etc.
    ├── test_list.h       # Tests für List<T, N> Type
    └── test_var_modes.h  # Tests für verschiedene Var-Modi (Ws/Meta, Prefs, Rw/Ro)
```

## Neue Tests hinzufügen

1. Erstelle einen neuen Ordner unter `test_esp32/` (z.B. `wifi_test/`)
2. Füge dort deine Test-Header-Datei hinzu mit einer `runAllTests()` Funktion
3. Includiere die Datei in `main.cpp` und rufe `runAllTests()` auf

Beispiel:
```cpp
// In main.cpp
#include "wifi_test/test_wifi.h"

void setup() {
  // ...
  WifiTest::runAllTests();
  // ...
}
```

## Features

- **Automatisches Cleanup**: Alle Preferences/NVS werden vor und nach den Tests gelöscht
- **Isolierte Tests**: Jeder Test startet mit einem "blanken" ESP32
- **Test-Statistiken**: Automatische Zählung von Pass/Fail
- **Modulare Struktur**: Einfaches Hinzufügen neuer Test-Suites

## Aktuell verfügbare Tests

### model_type_test/test_model.h
- **testStaticStringPersistence**: Testet ob StaticString korrekt in Preferences gespeichert und geladen wird
- **testVarImplicitConversion**: Testet implizite Konvertierung von Var zu const char*
- **testVarAssignment**: Testet verschiedene Zuweisungsoperationen
- **testSecretNeverLeaks**: Testet dass VarMetaPrefsRw-Werte nie über WebSocket gesendet werden (nur Meta-Daten)

### model_type_test/test_list.h
- **testListBasics**: Testet grundlegende List-Operationen (add, clear, size, capacity, isFull)
- **testListIterator**: Testet Iterator-Funktionalität (for-each Loops)
- **testListWithStaticString**: Testet List mit StaticString-Elementen
- **testListSerialization**: Testet JSON-Serialisierung von List (write_ws)
- **testListDeserialization**: Testet JSON-Deserialisierung in List (read)
- **testListInVar**: Testet List innerhalb einer Var<>-Wrapper

### model_type_test/test_var_modes.h
- **testVarWsPrefsRw**: Testet VarWsPrefsRw (Value + Persistence + Read-Write)
- **testVarWsRo**: Testet VarWsRo (Value + No Persistence + Read-Only)
- **testVarMetaPrefsRw**: Testet VarMetaPrefsRw (Meta only + Persistence + Read-Write)
- **testVarMetaRw**: Testet VarMetaRw (Meta only + No Persistence + Read-Write)
- **testPrefsFiltering**: Testet dass nur Felder mit PrefsMode::On in Preferences gespeichert werden
- **testReadOnlyRejection**: Testet dass Read-Only Felder nicht überschrieben werden können
- **testVarOnChange**: Testet onChange-Callbacks bei Wertänderungen
