#pragma once
#include <cstring>
#include "TimeProvider.h"

#include "model/ModelBase.h"
#include "model/ModelTypeButton.h"
#include "model/ModelTypeStaticString.h"
#include "model/ModelTypeList.h"
#include "model/ModelTypePointRingBuffer.h"
#include "model/ModelVar.h"

struct WifiSettings
{
  static const int PASS_LEN = 64;
  static const int SSID_LEN = 32;
  static const int MAX_NETWORKS = 20;

  // WS: value, Prefs: on, writable: on
  fj::VarWsPrefsRw<StaticString<SSID_LEN>> ssid;

  // Variant A: WS meta only (never leak), Prefs: on, writable: on
  fj::VarMetaPrefsRw<StaticString<PASS_LEN>> pass;

  // Available networks (direct List, not in Var)
  // Managed programmatically, not persisted or user-writable
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

  fj::VarMetaPrefsRw<StaticString<PASS_LEN>> ota_pass;

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

  struct AdminSettings {
    static const int PASS_LEN = 32;
    static const int SESSION_LEN = 64;
    fj::VarMetaPrefsRw<StaticString<PASS_LEN>> pass;
    fj::VarWsPrefsRw<StaticString<SESSION_LEN>> session;

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
    fj::VarWsPrefsRw<StaticString<CFG_LEN>> config;

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
    Serial.print("Model updated: ");
    Serial.println(topic);
  }

private:
  // Person person_;
  // TextConfig text_;
  // PointRingBuffer<50> temperature_1;
};
