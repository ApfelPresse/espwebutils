#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/ModelVar.h"
#include "../../src/model/ModelTypeStaticString.h"
#include "../../src/model/ModelTypeList.h"

// Forward declare WifiSettings (without including full Model.h which needs AsyncWebServer)
struct WifiSettingsMinimal
{
  static const int PASS_LEN = 64;
  static const int SSID_LEN = 32;

  fj::VarWsPrefsRw<StaticString<SSID_LEN>> ssid;
  fj::VarMetaPrefsRw<StaticString<PASS_LEN>> pass;

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

  fj::VarWsPrefsRw<StaticString<SSID_LEN>> ssid;
  fj::VarMetaPrefsRw<StaticString<PASS_LEN>> pass;
  // Direct List, not wrapped in Var (managed programmatically)
  List<StaticString<SSID_LEN>, MAX_NETWORKS> available_networks;

  typedef fj::Schema<WifiSettings,
                     fj::Field<WifiSettings, decltype(ssid)>,
                     fj::Field<WifiSettings, decltype(pass)>>
      SchemaType;

  static const SchemaType &schema()
  {
    static const SchemaType s = fj::makeSchema<WifiSettings>(
        fj::Field<WifiSettings, decltype(ssid)>{"ssid", &WifiSettings::ssid},
        fj::Field<WifiSettings, decltype(pass)>{"pass", &WifiSettings::pass});
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
  wifi.available_networks.clear();
  wifi.available_networks.add(StaticString<32>("HomeNetwork"));
  wifi.available_networks.add(StaticString<32>("OfficeWiFi"));
  wifi.available_networks.add(StaticString<32>("GuestNetwork"));
  
  TEST_ASSERT(wifi.available_networks.size() == 3, "Should have 3 networks");
  TEST_ASSERT(strcmp(wifi.available_networks[0].c_str(), "HomeNetwork") == 0, "First network should be HomeNetwork");
  TEST_ASSERT(strcmp(wifi.available_networks[1].c_str(), "OfficeWiFi") == 0, "Second network should be OfficeWiFi");
  
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
  
  TEST_ASSERT(json.indexOf("MyNetwork") > 0, "Preferences should contain SSID");
  TEST_ASSERT(json.indexOf("MyPassword123") > 0, "Preferences should contain password");
  
  // Deserialize back
  StaticJsonDocument<512> loadDoc;
  deserializeJson(loadDoc, json);
  
  bool success = fj::readFieldsTolerant(wifi2, WifiSettings::schema(), loadDoc.as<JsonObject>());
  TEST_ASSERT(success, "Deserialization should succeed");
  TEST_ASSERT(strcmp(wifi2.ssid.c_str(), "MyNetwork") == 0, "SSID should be restored");
  TEST_ASSERT(strcmp(wifi2.pass.c_str(), "MyPassword123") == 0, "Password should be restored");
  
  TEST_END();
}

// Test WiFi WebSocket serialization (should NOT leak secret)
void testWiFiWebSocketSerialization() {
  TEST_START("WiFi WebSocket Serialization (secret hiding)");
  
  WifiSettings wifi;
  wifi.ssid = "PublicNetwork";
  wifi.pass = "SecretPassword";
  wifi.available_networks.add(StaticString<32>("Network1"));
  wifi.available_networks.add(StaticString<32>("Network2"));
  
  // Serialize for WebSocket (should use write_ws)
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  // For Var<T, WsMode>, write_value calls write_ws (no "value" wrapper for List)
  fj::writeFields(wifi, WifiSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  Serial.print("WS JSON: ");
  Serial.println(json);
  
  // SSID should be visible (VarWsPrefsRw)
  TEST_ASSERT(json.indexOf("PublicNetwork") > 0, "SSID should be in WS output");
  
  // Password should NOT be visible in WS (VarMetaPrefsRw shows only metadata)
  TEST_ASSERT(json.indexOf("SecretPassword") == -1, "Password should NOT be in WS output");
  TEST_ASSERT(json.indexOf("secret") > 0, "Should show meta type indicator");
  
  // Networks should be visible
  TEST_ASSERT(json.indexOf("Network1") > 0, "Network1 should be visible");
  TEST_ASSERT(json.indexOf("Network2") > 0, "Network2 should be visible");
  
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
  
  TEST_ASSERT(success, "Update should succeed");
  TEST_ASSERT(strcmp(wifi.ssid.c_str(), "NewNetwork") == 0, "SSID should update");
  TEST_ASSERT(strcmp(wifi.pass.c_str(), "NewPassword") == 0, "Password should update");
  
  TEST_END();
}

// Test available_networks read-only property
void testAvailableNetworksReadOnly() {
  TEST_START("Available Networks Read-Only Property");
  
  WifiSettings wifi;
  
  // Direct manipulation of the list
  wifi.available_networks.add(StaticString<32>("TestNetwork"));
  
  TEST_ASSERT(wifi.available_networks.size() == 1, "Should have 1 network");
  TEST_ASSERT(strcmp(wifi.available_networks[0].c_str(), "TestNetwork") == 0, "Network should be TestNetwork");
  
  // Note: available_networks is read-only from JSON perspective
  // The internal list can still be manipulated directly
  wifi.available_networks.add(StaticString<32>("SecondNetwork"));
  TEST_ASSERT(wifi.available_networks.size() == 2, "Should be able to add directly");
  
  TEST_END();
}

// Test available_networks List serialization format
void testAvailableNetworksSerialization() {
  TEST_START("Available Networks List Serialization");
  
  WifiSettings wifi;
  wifi.available_networks.add(StaticString<32>("WiFi-A"));
  wifi.available_networks.add(StaticString<32>("WiFi-B"));
  wifi.available_networks.add(StaticString<32>("WiFi-C"));
  
  // Serialize to JSON
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::writeFields(wifi, WifiSettings::schema(), root);
  
  String json;
  serializeJson(root, json);
  Serial.print("Networks JSON: ");
  Serial.println(json);
  
  // Parse back and verify structure
  StaticJsonDocument<512> parseDoc;
  deserializeJson(parseDoc, json);
  
  JsonObject obj = parseDoc.as<JsonObject>();
  JsonObject networksObj = obj["available_networks"];
  
  TEST_ASSERT(!networksObj.isNull(), "Should have available_networks object");
  TEST_ASSERT(networksObj.containsKey("items"), "Should have items array");
  
  JsonArray itemsArr = networksObj["items"];
  TEST_ASSERT(itemsArr.size() == 3, "Items array should have 3 elements");
  TEST_ASSERT(strcmp(itemsArr[0], "WiFi-A") == 0, "First item should be WiFi-A");
  TEST_ASSERT(strcmp(itemsArr[1], "WiFi-B") == 0, "Second item should be WiFi-B");
  TEST_ASSERT(strcmp(itemsArr[2], "WiFi-C") == 0, "Third item should be WiFi-C");
  
  TEST_END();
}

// Test WiFi Model integration
void testWiFiModelIntegration() {
  TEST_START("WiFi Settings Integration");
  
  WifiSettings wifi;
  
  // Simulate receiving WiFi updates
  wifi.ssid = "ConnectedNetwork";
  wifi.pass = "ConnectedPass";
  
  wifi.available_networks.clear();
  wifi.available_networks.add(StaticString<32>("Network1"));
  wifi.available_networks.add(StaticString<32>("Network2"));
  
  TEST_ASSERT(strcmp(wifi.ssid.c_str(), "ConnectedNetwork") == 0, "SSID should be set");
  TEST_ASSERT(strcmp(wifi.pass.c_str(), "ConnectedPass") == 0, "Pass should be set");
  TEST_ASSERT(wifi.available_networks.size() == 2, "WiFiSettings should have 2 networks");
  
  TEST_END();
}

} // namespace WiFiIntegrationTest
