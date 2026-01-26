#pragma once
#include <cstring>
#include "TimeProvider.h"

#include "model/ModelBase.h"
#include "model/ModelTypeButton.h"
#include "model/ModelTypePrimitive.h"
#include "model/ModelTypeList.h"
#include "model/ModelTypePointRingBuffer.h"
#include "model/ModelVar.h"

struct WifiSettings
{
  static const int PASS_LEN = 64;
  static const int SSID_LEN = 32;
  static const int MAX_NETWORKS = 20;

  // WS: value, Prefs: on, writable: on
  fj::VarWsPrefsRw<StringBuffer<SSID_LEN>> ssid;

  // Variant A: WS meta only (never leak), Prefs: on, writable: on
  fj::VarMetaPrefsRw<StringBuffer<PASS_LEN>> pass;

  // Available networks (WS: value, Prefs: off, read-only)
  fj::VarWsRo<List<StringBuffer<SSID_LEN>, MAX_NETWORKS>> available_networks;

  // Log level (WS: value, Prefs: off, read-only) - values: TRACE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4
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


struct MDNSSettings
{
  static const int MDNS_LEN = 64;

  char mdns_domain[MDNS_LEN] = "esp32-device";

  typedef fj::Schema<MDNSSettings,
                     fj::FieldStr<MDNSSettings, MDNS_LEN>>
      SchemaType;

  static const SchemaType &schema()
  {
    static const SchemaType s = fj::makeSchema<MDNSSettings>(
      fj::FieldStr<MDNSSettings, MDNS_LEN>{"mdns_domain", &MDNSSettings::mdns_domain});
    return s;
  }
  void setSaveCallback(std::function<void()> /*cb*/) { }
};

struct OTASettings
{
  static const int PASS_LEN = 32;

  fj::VarWsPrefsRw<StringBuffer<PASS_LEN>> ota_pass;
  Button generate_new_ota_pass;

  typedef fj::Schema<OTASettings,
                     fj::Field<OTASettings, decltype(ota_pass)>,
                     fj::Field<OTASettings, decltype(generate_new_ota_pass)>>
      SchemaType;

  static const SchemaType &schema()
  {
    static const SchemaType s = fj::makeSchema<OTASettings>(
        fj::Field<OTASettings, decltype(ota_pass)>{"ota_pass", &OTASettings::ota_pass},
        fj::Field<OTASettings, decltype(generate_new_ota_pass)>{"generate_new_pass_button", &OTASettings::generate_new_ota_pass});
    return s;
  }
  void setSaveCallback(std::function<void()> cb) {
    ota_pass.setOnChange(cb);
  }
};


class Model : public ModelBase
{
public:
  WifiSettings wifi;
  MDNSSettings mdns;
  OTASettings ota;

  // Callback for when WiFi settings are updated
  std::function<void()> onWifiUpdate = nullptr;

  struct AdminSettings {
    static const int PASS_LEN = 32;
    static const int SESSION_LEN = 64;
    static const int ADMIN_LOG_SIZE = 5;
    fj::VarWsPrefsRw<StringBuffer<PASS_LEN>> pass;
    fj::VarWsPrefsRw<StringBuffer<SESSION_LEN>> session;
    Button generate_new_admin_ui_pass;
    fj::VarWsPrefsRw<PointRingBuffer<ADMIN_LOG_SIZE>> admin_log;

    typedef fj::Schema<AdminSettings,
                       fj::Field<AdminSettings, decltype(pass)>,
                       fj::Field<AdminSettings, decltype(session)>,
                       fj::Field<AdminSettings, decltype(generate_new_admin_ui_pass)>,
                       fj::Field<AdminSettings, decltype(admin_log)>> SchemaType;

    static const SchemaType &schema() {
      static const SchemaType s = fj::makeSchema<AdminSettings>(
        fj::Field<AdminSettings, decltype(pass)>{"pass", &AdminSettings::pass},
        fj::Field<AdminSettings, decltype(session)>{"session", &AdminSettings::session},
        fj::Field<AdminSettings, decltype(generate_new_admin_ui_pass)>{"generate_new_admin_ui_pass", &AdminSettings::generate_new_admin_ui_pass},
        fj::Field<AdminSettings, decltype(admin_log)>{"admin_log", &AdminSettings::admin_log}
      );
      return s;
    }
    void setSaveCallback(std::function<void()> cb) {
      pass.setOnChange(cb);
      session.setOnChange(cb);
      admin_log.setOnChange(cb);
    }
  };

  struct AdminUiSettings {
    static const int CFG_LEN = 512;
    fj::VarWsPrefsRw<StringBuffer<CFG_LEN>> config;

    typedef fj::Schema<AdminUiSettings,
                       fj::Field<AdminUiSettings, decltype(config)>> SchemaType;

    static const SchemaType &schema() {
      static const SchemaType s = fj::makeSchema<AdminUiSettings>(
        fj::Field<AdminUiSettings, decltype(config)>{"config", &AdminUiSettings::config}
      );
      return s;
    }
    void setSaveCallback(std::function<void()> cb) {
      config.setOnChange(cb);
    }
  };

  AdminSettings admin;
  AdminUiSettings admin_ui;

  // Generate a random password of specified length
  static String generatePassword(size_t len = 12) {
    static const char *chars = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789"; // no ambiguous chars
    String out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
      uint32_t r = esp_random();
      out += chars[r % strlen(chars)];
    }
    return out;
  }

  // Initialize model and ensure all required passwords are set
  void begin() {
    LOG_TRACE("[Model] Model::begin() called");
    ModelBase::begin();
    ensurePasswords();
  }

  // Ensure passwords are generated for fields that need them
  void ensurePasswords() {
    bool changed = false;
    
    // Admin password
    const char* adminPw = admin.pass;
    if (!adminPw || adminPw[0] == '\0') {
      String newPw = generatePassword(12);
      admin.pass.set(newPw.c_str());
      LOG_INFO_F("[Model] Generated admin password: %s", newPw.c_str());
      changed = true;
    } else {
      LOG_TRACE_F("[Model] Admin password already set: %s", adminPw);
    }
    
    // OTA password
    const char* otaPw = ota.ota_pass;
    if (!otaPw || otaPw[0] == '\0') {
      String newPw = generatePassword(12);
      ota.ota_pass.set(newPw.c_str());
      LOG_INFO_F("[Model] Generated OTA password: %s", newPw.c_str());
      changed = true;
    } else {
      LOG_TRACE_F("[Model] OTA password already set: %s", otaPw);
    }
    
    if (changed) {
      saveTopic("admin");
      saveTopic("ota");
    }
  }

  Model() : ModelBase(80, "/ws")
  {
    // // // std::strncpy(person_.name, "Max", NAME_LEN_LOCAL - 1);
    // person_.name[NAME_LEN_LOCAL - 1] = '\0';
    // person_.alter = 30;
    // person_.button = 123;

    // std::strncpy(wifi_.ssid, WIFI_SSID, SSID_LEN - 1);
    // wifi_.ssid[SSID_LEN - 1] = '\0';
    //  wifi_.pass.set(WIFI_PASS);
    //  wifi_.dhcp = true;

    // temperature_1.setGraph("temperature");
    // temperature_1.setLabel("°C");
    // temperature_1.setTimeProvider(&time_now_ms, &time_is_synced, nullptr);
    // temperature_1.setCallback(&ModelBase::graphPushCbXY, this);

    // registerTopic("person", person_);
    registerTopic("wifi", wifi);
    registerTopic("ota", ota);
    registerTopic("mdns", mdns);
    registerTopic("admin", admin);
    registerTopic("admin_ui", admin_ui);
    // registerTopic("data_temp_1", temperature_1);
    // registerTopic("text", text_, true, false);

    // Register button callbacks
    ota.generate_new_ota_pass.setCallback([this]() { this->onGenerateNewOtaPassword(); });
    admin.generate_new_admin_ui_pass.setCallback([this]() { this->onGenerateNewAdminUiPassword(); });

    admin.admin_log.get().setGraph("admin_events");
    admin.admin_log.get().setLabel("auth");
    // Direct callback for live graph updates
    admin.admin_log.get().setCallback(&ModelBase::graphPushCbXY, this);
  }

  // void pushDemo()
  // {
  //   static int v = 20;
  //   temperature_1.push(v++);
  // }

protected:
  void on_update(const char *topic) override
  {
    LOG_TRACE_F("[Model] Model update notified for topic: %s", topic);
    
    if (strcmp(topic, "wifi") == 0) {
      LOG_INFO_F("[WiFi] SSID updated to: %s", wifi.ssid.get().c_str());
      LOG_DEBUG("[WiFi] Password field received (value not logged for security)");
      // DEBUG: Print password only at TRACE level for troubleshooting
      const char* pass = wifi.pass.get().c_str();
      LOG_TRACE_F("[WiFi] Password value: '%s'  <<< TRACE DEBUG ONLY (LENGTH: %d)", pass, strlen(pass));
      LOG_TRACE("[WiFi] Triggering reconnect with new credentials");
      if (onWifiUpdate) {
        LOG_TRACE("[WiFi] Calling WiFi update callback");
        onWifiUpdate();
      }
    }
    else if (strcmp(topic, "ota") == 0) {
      LOG_DEBUG("[OTA] OTA settings updated");
    }
    else if (strcmp(topic, "admin") == 0) {
      LOG_DEBUG("[Admin] Admin settings updated");
    }
    else if (strcmp(topic, "mdns") == 0) {
      LOG_DEBUG("[mDNS] mDNS settings updated");
    }
  }

  // Handler for OTA password generation button
  void onGenerateNewOtaPassword() {
    LOG_INFO("[OTA] Generating new OTA password...");
    String newPw = generatePassword(12);
    ota.ota_pass.set(newPw.c_str());
    LOG_INFO_F("[OTA] Generated new OTA password: %s", newPw.c_str());
    saveTopic("ota");
    broadcastTopic("ota");
  }

  // Handler for Admin UI password generation button
  void onGenerateNewAdminUiPassword() {
    LOG_INFO("[Admin] Generating new Admin UI password (basic auth)...");
    String newPw = generatePassword(12);
    admin.pass.set(newPw.c_str());
    LOG_INFO_F("[Admin] Generated new Admin UI password: %s", newPw.c_str());
    saveTopic("admin");
    broadcastTopic("admin");
  }

  // Push a data point to the admin log ring buffer and persist/broadcast updates
  void pushAdminLog(uint64_t x, float y) {
    admin.admin_log.get().push(x, y);
    admin.admin_log.touch();
    saveTopic("admin");
    broadcastTopic("admin");
  }

  // Handle button trigger requests (override from ModelBase)
  void handleButtonTrigger(AsyncWebSocketClient* client, const char* topic, const char* button) override {
    LOG_DEBUG_F("[Model] handleButtonTrigger: topic=%s, button=%s", topic, button);
    
    if (strcmp(topic, "ota") == 0) {
      if (strcmp(button, "generate_new_pass_button") == 0) {
        LOG_INFO("[OTA] Button trigger: generate_new_pass_button");
        onGenerateNewOtaPassword();
        if (client) client->text(R"({"ok":true,"action":"button_triggered"})");
        return;
      }
    }

    if (strcmp(topic, "admin") == 0) {
      if (strcmp(button, "generate_new_admin_ui_pass") == 0) {
        LOG_INFO("[Admin] Button trigger: generate_new_admin_ui_pass");
        onGenerateNewAdminUiPassword();
        if (client) client->text(R"({"ok":true,"action":"button_triggered"})");
        return;
      }
    }
    
    LOG_WARN_F("[Model] Unknown button: topic=%s, button=%s", topic, button);
    if (client) client->text(R"({"ok":false,"error":"unknown_button"})");
  }

private:
  // Person person_;
  // TextConfig text_;
  // PointRingBuffer<50> temperature_1;
};
