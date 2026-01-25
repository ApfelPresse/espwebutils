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

  fj::VarMetaPrefsRw<StringBuffer<PASS_LEN>> ota_pass;

  typedef fj::Schema<OTASettings,
                     fj::Field<OTASettings, decltype(ota_pass)>>
      SchemaType;

  static const SchemaType &schema()
  {
    static const SchemaType s = fj::makeSchema<OTASettings>(
        fj::Field<OTASettings, decltype(ota_pass)>{"ota_pass", &OTASettings::ota_pass});
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
    fj::VarMetaPrefsRw<StringBuffer<PASS_LEN>> pass;
    fj::VarWsPrefsRw<StringBuffer<SESSION_LEN>> session;

    typedef fj::Schema<AdminSettings,
                       fj::Field<AdminSettings, decltype(pass)>,
                       fj::Field<AdminSettings, decltype(session)>> SchemaType;

    static const SchemaType &schema() {
      static const SchemaType s = fj::makeSchema<AdminSettings>(
        fj::Field<AdminSettings, decltype(pass)>{"pass", &AdminSettings::pass},
        fj::Field<AdminSettings, decltype(session)>{"session", &AdminSettings::session}
      );
      return s;
    }
    void setSaveCallback(std::function<void()> cb) {
      pass.setOnChange(cb);
      session.setOnChange(cb);
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

    // Initialize log_level to INFO (2) by default
    wifi.log_level = 0;

    // registerTopic("person", person_);
    registerTopic("wifi", wifi);
    registerTopic("ota", ota);
    registerTopic("mdns", mdns);
    registerTopic("admin", admin);
    registerTopic("admin_ui", admin_ui);
    // registerTopic("data_temp_1", temperature_1);
    // registerTopic("text", text_, true, false);
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

private:
  // Person person_;
  // TextConfig text_;
  // PointRingBuffer<50> temperature_1;
};
