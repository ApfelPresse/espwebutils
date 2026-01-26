#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/ModelVar.h"
#include "../../src/model/types/ModelTypePrimitive.h"
#include "../../src/model/types/ModelTypeList.h"

// Forward declare WifiSettings (without including full Model.h which needs AsyncWebServer)
struct WifiSettingsMinimal
{
  static const int PASS_LEN = 64;
  static const int SSID_LEN = 32;

  fj::VarWsPrefsRw<StringBuffer<SSID_LEN>> ssid;
  fj::VarMetaPrefsRw<StringBuffer<PASS_LEN>> pass;

  typedef fj::Schema<WifiSettingsMinimal,
                     fj::Field<WifiSettingsMinimal, decltype(ssid)>,
                     fj::Field<WifiSettingsMinimal, decltype(pass)>>
      SchemaType;

  static const SchemaType &schema()
  {
    static const SchemaType s = fj::makeSchema<WifiSettingsMinimal>(
        fj::Field<WifiSettingsMinimal, decltype(ssid)>{"ssid", &WifiSettingsMinimal::ssid},
        fj::Field<WifiSettingsMinimal, decltype(pass)>{"pass", &WifiSettingsMinimal::pass});
    return s;
  }
};

struct WifiSettings
{
  static const int PASS_LEN = 64;
  static const int SSID_LEN = 32;
  static const int MAX_NETWORKS = 20;

  fj::VarWsPrefsRw<StringBuffer<SSID_LEN>> ssid;
  fj::VarMetaPrefsRw<StringBuffer<PASS_LEN>> pass;
  fj::VarWsPrefsRo<List<StringBuffer<SSID_LEN>, MAX_NETWORKS>> available_networks;
  fj::VarWsRo<int> log_level;

  typedef fj::Schema<WifiSettings,
                     fj::Field<WifiSettings, decltype(ssid)>,
                     fj::Field<WifiSettings, decltype(pass)>,
                     fj::Field<WifiSettings, decltype(available_networks)>,
                     fj::Field<WifiSettings, decltype(log_level)>>
      SchemaType;

  static const SchemaType &schema()
  {
    static const SchemaType s = fj::makeSchema<WifiSettings>(
        fj::Field<WifiSettings, decltype(ssid)>{"ssid", &WifiSettings::ssid},
        fj::Field<WifiSettings, decltype(pass)>{"pass", &WifiSettings::pass},
        fj::Field<WifiSettings, decltype(available_networks)>{"available_networks", &WifiSettings::available_networks},
        fj::Field<WifiSettings, decltype(log_level)>{"log_level", &WifiSettings::log_level});
    return s;
  }

  void setSaveCallback(std::function<void()> cb) {
    ssid.setOnChange(cb);
    pass.setOnChange(cb);
  }
};

namespace WiFiIntegrationTest {

// Mock WiFi scan results
void simulateWiFiScan() {
  TEST_START("WiFi Scan Simulation");
  
  WifiSettings wifi;
  
  // Simulate scan results
  wifi.available_networks.get().clear();
  wifi.available_networks.get().add(StringBuffer<32>("HomeNetwork"));
  wifi.available_networks.get().add(StringBuffer<32>("OfficeWiFi"));
  wifi.available_networks.get().add(StringBuffer<32>("GuestNetwork"));
  
  CUSTOM_ASSERT(wifi.available_networks.get().size() == 3, "Should have 3 networks");
  CUSTOM_ASSERT(strcmp(wifi.available_networks.get()[0].c_str(), "HomeNetwork") == 0, "First network should be HomeNetwork");
  CUSTOM_ASSERT(strcmp(wifi.available_networks.get()[1].c_str(), "OfficeWiFi") == 0, "Second network should be OfficeWiFi");
  
  TEST_END();
}

// Test WiFi settings persistence
void testWiFiSettingsPersistence() {
  TEST_START("WiFi Settings Persistence");
  
  WifiSettings wifi1, wifi2;
  wifi1.ssid = "MyNetwork";
  wifi1.pass = "MyPassword123";
  
  // Serialize to preferences
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFieldsPrefs(wifi1, WifiSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  Serial.print("Prefs JSON: ");
  Serial.println(json);
  
  CUSTOM_ASSERT(json.indexOf("MyNetwork") > 0, "Preferences should contain SSID");
  CUSTOM_ASSERT(json.indexOf("MyPassword123") > 0, "Preferences should contain password");
  
  // Deserialize back
  StaticJsonDocument<512> loadDoc;
  deserializeJson(loadDoc, json);
  
  bool success = fj::readFieldsTolerant(wifi2, WifiSettings::schema(), loadDoc.as<JsonObject>());
  CUSTOM_ASSERT(success, "Deserialization should succeed");
  CUSTOM_ASSERT(strcmp(wifi2.ssid.c_str(), "MyNetwork") == 0, "SSID should be restored");
  CUSTOM_ASSERT(strcmp(wifi2.pass.c_str(), "MyPassword123") == 0, "Password should be restored");
  
  TEST_END();
}

// Test WiFi WebSocket serialization (should NOT leak secret)
void testWiFiWebSocketSerialization() {
  TEST_START("WiFi WebSocket Serialization (secret hiding)");
  
  WifiSettings wifi;
  wifi.ssid = "PublicNetwork";
  wifi.pass = "SecretPassword";
  
  // Serialize for WebSocket (Var fields only - available_networks is direct List)
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  fj::writeFields(wifi, WifiSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  Serial.print("WS JSON: ");
  Serial.println(json);
  
  // SSID should be visible (VarWsPrefsRw)
  CUSTOM_ASSERT(json.indexOf("PublicNetwork") > 0, "SSID should be in WS output");
  
  // Password should NOT be visible in WS (VarMetaPrefsRw shows only metadata)
  CUSTOM_ASSERT(json.indexOf("SecretPassword") == -1, "Password should NOT be in WS output");
  CUSTOM_ASSERT(json.indexOf("secret") > 0, "Should show meta type indicator");
  
  // available_networks will be serialized via TypeAdapter now (VarWsRo)
  
  TEST_END();
}

// Test WiFi settings update from WebSocket
void testWiFiSettingsUpdate() {
  TEST_START("WiFi Settings Update from WS message");
  
  WifiSettingsMinimal wifi;
  wifi.ssid = "OldNetwork";
  wifi.pass = "OldPassword";
  
  // Simulate receiving a WS update (only ssid and pass, not available_networks)
  StaticJsonDocument<256> doc;
  doc["ssid"] = "NewNetwork";
  doc["pass"] = "NewPassword";
  
  bool success = fj::readFieldsTolerant(wifi, WifiSettingsMinimal::schema(), doc.as<JsonObject>());
  
  CUSTOM_ASSERT(success, "Update should succeed");
  CUSTOM_ASSERT(strcmp(wifi.ssid.c_str(), "NewNetwork") == 0, "SSID should update");
  CUSTOM_ASSERT(strcmp(wifi.pass.c_str(), "NewPassword") == 0, "Password should update");
  
  TEST_END();
}

// Test available_networks read-only property
void testAvailableNetworksReadOnly() {
  TEST_START("Available Networks Read-Only Property");
  
  WifiSettings wifi;
  
  // Direct manipulation of the list
  wifi.available_networks.get().add(StringBuffer<32>("TestNetwork"));
  
  CUSTOM_ASSERT(wifi.available_networks.get().size() == 1, "Should have 1 network");
  CUSTOM_ASSERT(strcmp(wifi.available_networks.get()[0].c_str(), "TestNetwork") == 0, "Network should be TestNetwork");
  
  // Note: available_networks is read-only from JSON perspective
  // The internal list can still be manipulated directly
  wifi.available_networks.get().add(StringBuffer<32>("SecondNetwork"));
  CUSTOM_ASSERT(wifi.available_networks.get().size() == 2, "Should be able to add directly");
  
  TEST_END();
}

// Test available_networks List serialization format
void testAvailableNetworksSerialization() {
  TEST_START("Available Networks List Serialization");
  
  WifiSettings wifi;
  wifi.available_networks.get().add(StringBuffer<32>("WiFi-A"));
  wifi.available_networks.get().add(StringBuffer<32>("WiFi-B"));
  wifi.available_networks.get().add(StringBuffer<32>("WiFi-C"));
  
  // Direct List serialization (outside of schema)
  // Since available_networks is a direct List, not in Var wrapper,
  // we serialize it manually using TypeAdapter::write_ws
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  // Use TypeAdapter for List - call write_ws directly
  JsonObject networksObj = root.createNestedObject("available_networks");
  fj::TypeAdapter<List<StringBuffer<32>, 20>>::write_ws(
    wifi.available_networks.get(), networksObj);
  
  String json;
  serializeJson(root, json);
  Serial.print("Networks JSON: ");
  Serial.println(json);
  
  // Verify the serialized structure contains our networks
  CUSTOM_ASSERT(json.indexOf("WiFi-A") > 0, "Should contain WiFi-A");
  CUSTOM_ASSERT(json.indexOf("WiFi-B") > 0, "Should contain WiFi-B");
  CUSTOM_ASSERT(json.indexOf("WiFi-C") > 0, "Should contain WiFi-C");
  CUSTOM_ASSERT(json.indexOf("items") > 0, "Should have items array");
  
  TEST_END();
}

// Test WiFi Model integration
void testWiFiModelIntegration() {
  TEST_START("WiFi Settings Integration");
  
  WifiSettings wifi;
  
  // Simulate receiving WiFi updates
  wifi.ssid = "ConnectedNetwork";
  wifi.pass = "ConnectedPass";
  
  wifi.available_networks.get().clear();
  wifi.available_networks.get().add(StringBuffer<32>("Network1"));
  wifi.available_networks.get().add(StringBuffer<32>("Network2"));
  
  CUSTOM_ASSERT(strcmp(wifi.ssid.c_str(), "ConnectedNetwork") == 0, "SSID should be set");
  CUSTOM_ASSERT(strcmp(wifi.pass.c_str(), "ConnectedPass") == 0, "Pass should be set");
  CUSTOM_ASSERT(wifi.available_networks.get().size() == 2, "WiFiSettings should have 2 networks");
  
  TEST_END();
}

} // namespace WiFiIntegrationTest
