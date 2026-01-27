#pragma once
#include <cstring>
#include "TimeProvider.h"

#include "model/ModelBase.h"
#include "model/types/ModelTypeButton.h"
#include "model/types/ModelTypePrimitive.h"
#include "model/types/ModelTypeList.h"
#include "model/types/ModelTypePointRingBuffer.h"
#include "model/ModelVar.h"

#include "build_info.h"

#ifndef ESPWEBUTILS_LIBRARY_VERSION
#define ESPWEBUTILS_LIBRARY_VERSION "unknown"
#endif

#ifndef ESPWEBUTILS_WEBFILES_HASH
#define ESPWEBUTILS_WEBFILES_HASH "unknown"
#endif

struct BuildInfo
{
  static const int VERSION_LEN = 32;
  static const int HASH_LEN = 80;

  // Build metadata (WS: value, Prefs: off, read-only)
  fj::VarWsRo<StringBuffer<VERSION_LEN>> library_version;
  fj::VarWsRo<StringBuffer<HASH_LEN>> webfiles_hash;

  typedef fj::Schema<BuildInfo,
                     fj::Field<BuildInfo, decltype(library_version)>,
                     fj::Field<BuildInfo, decltype(webfiles_hash)>>
      SchemaType;

  static const SchemaType &schema()
  {
    static const SchemaType s = fj::makeSchema<BuildInfo>(
        fj::Field<BuildInfo, decltype(library_version)>{"library_version", &BuildInfo::library_version},
        fj::Field<BuildInfo, decltype(webfiles_hash)>{"webfiles_hash", &BuildInfo::webfiles_hash});
    return s;
  }

  void setSaveCallback(std::function<void()> /*cb*/) { }
};

struct WifiSettings
{
  static const int PASS_LEN = 64;
  static const int SSID_LEN = 32;
  static const int MAX_NETWORKS = 20;

  // WS: value, Prefs: on, writable: on
  fj::VarWsPrefsRw<StringBuffer<SSID_LEN>> ssid;

  // Access Point SSID for provisioning mode (Prefs: on, writable: on)
  fj::VarWsPrefsRw<StringBuffer<SSID_LEN>> ap_ssid;

  // Variant A: WS meta only (never leak), Prefs: on, writable: on
  fj::VarMetaPrefsRw<StringBuffer<PASS_LEN>> pass;

  // Available networks (WS: value, Prefs: off, read-only)
  fj::VarWsRo<List<StringBuffer<SSID_LEN>, MAX_NETWORKS>> available_networks;

  // Log level (WS: value, Prefs: off, read-only) - values: TRACE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4
  fj::VarWsRo<int> log_level;

  typedef fj::Schema<WifiSettings,
                     fj::Field<WifiSettings, decltype(ssid)>,
                     fj::Field<WifiSettings, decltype(ap_ssid)>,
                     fj::Field<WifiSettings, decltype(pass)>,
                     fj::Field<WifiSettings, decltype(available_networks)>,
                     fj::Field<WifiSettings, decltype(log_level)>>
      SchemaType;

  static const SchemaType &schema()
  {
    static const SchemaType s = fj::makeSchema<WifiSettings>(
        fj::Field<WifiSettings, decltype(ssid)>{"ssid", &WifiSettings::ssid},
        fj::Field<WifiSettings, decltype(ap_ssid)> {"ap_ssid", &WifiSettings::ap_ssid},
        fj::Field<WifiSettings, decltype(pass)>{"pass", &WifiSettings::pass},
        fj::Field<WifiSettings, decltype(available_networks)>{"available_networks", &WifiSettings::available_networks},
        fj::Field<WifiSettings, decltype(log_level)>{"log_level", &WifiSettings::log_level});
    return s;
  }

  void setSaveCallback(std::function<void()> cb) {
    ssid.setOnChange(cb);
    ap_ssid.setOnChange(cb);
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
  // OTA update window in seconds; 0 means unlimited.
  fj::VarWsPrefsRw<int> window_seconds = 600;
  // Remaining seconds in the current OTA window; -1 means unlimited, 0 means expired/not started.
  fj::VarWsRo<int> remaining_seconds = 0;

  Button generate_new_ota_pass;
  Button extend_ota_window;

  typedef fj::Schema<OTASettings,
                     fj::Field<OTASettings, decltype(ota_pass)>,
                     fj::Field<OTASettings, decltype(window_seconds)>,
                     fj::Field<OTASettings, decltype(remaining_seconds)>,
                     fj::Field<OTASettings, decltype(generate_new_ota_pass)>,
                     fj::Field<OTASettings, decltype(extend_ota_window)>>
      SchemaType;

  static const SchemaType &schema()
  {
    static const SchemaType s = fj::makeSchema<OTASettings>(
        fj::Field<OTASettings, decltype(ota_pass)>{"ota_pass", &OTASettings::ota_pass},
        fj::Field<OTASettings, decltype(window_seconds)>{"window_seconds", &OTASettings::window_seconds},
        fj::Field<OTASettings, decltype(remaining_seconds)>{"remaining_seconds", &OTASettings::remaining_seconds},
        fj::Field<OTASettings, decltype(generate_new_ota_pass)>{"generate_new_pass_button", &OTASettings::generate_new_ota_pass},
        fj::Field<OTASettings, decltype(extend_ota_window)>{"extend_window_button", &OTASettings::extend_ota_window});
    return s;
  }
  void setSaveCallback(std::function<void()> cb) {
    ota_pass.setOnChange(cb);
    window_seconds.setOnChange(cb);
  }
};


class AdminModel : public ModelBase
{
public:
  WifiSettings wifi;
  MDNSSettings mdns;
  OTASettings ota;
  BuildInfo build;

  struct TimeSettings {
    static const int TZ_LEN = 64;
    static const int NOW_LEN = 32;

    // POSIX TZ string used by TimeSync (persisted)
    fj::VarWsPrefsRw<StringBuffer<TZ_LEN>> tz;

    // Display-only status (not persisted)
    fj::VarWsRo<StringBuffer<NOW_LEN>> now;
    fj::VarWsRo<bool> synced;

    Button sync_now;

    typedef fj::Schema<TimeSettings,
                       fj::Field<TimeSettings, decltype(tz)>,
                       fj::Field<TimeSettings, decltype(now)>,
                       fj::Field<TimeSettings, decltype(synced)>,
                       fj::Field<TimeSettings, decltype(sync_now)>> SchemaType;

    static const SchemaType &schema() {
      static const SchemaType s = fj::makeSchema<TimeSettings>(
        fj::Field<TimeSettings, decltype(tz)>{"tz", &TimeSettings::tz},
        fj::Field<TimeSettings, decltype(now)>{"now", &TimeSettings::now},
        fj::Field<TimeSettings, decltype(synced)>{"synced", &TimeSettings::synced},
        fj::Field<TimeSettings, decltype(sync_now)>{"sync_now", &TimeSettings::sync_now}
      );
      return s;
    }

    void setSaveCallback(std::function<void()> cb) {
      tz.setOnChange(cb);
    }
  };

  TimeSettings time;

  // Callback for when WiFi settings are updated
  std::function<void()> onWifiUpdate = nullptr;

  // Callback for when UI requests a WiFi scan (triggered via WS action button_trigger)
  std::function<void()> onWifiScanRequest = nullptr;

  // Callback for when OTA settings are changed (password/window)
  std::function<void()> onOtaUpdate = nullptr;

  // Callback for when UI requests OTA window extend
  std::function<void()> onOtaExtendRequest = nullptr;

  // Callback for when UI requests WiFi reset (clear credentials + restart)
  std::function<void()> onResetRequest = nullptr;

  // Callback for when mDNS settings are changed (hostname)
  std::function<void()> onMdnsUpdate = nullptr;

  // Callback for when admin settings are changed
  std::function<void()> onAdminUpdate = nullptr;

  // Callback for when time settings are changed
  std::function<void()> onTimeUpdate = nullptr;

  // Callback for when UI requests a time resync (button)
  std::function<void()> onTimeSyncNow = nullptr;

  struct AdminSettings {
    static const int PASS_LEN = 32;
    static const int SESSION_LEN = 64;
    static const int HEAP_SIZE = 5;
    fj::VarWsPrefsRw<StringBuffer<PASS_LEN>> pass;
    fj::VarWsPrefsRw<StringBuffer<SESSION_LEN>> session;
    fj::VarWsPrefsRw<int> heap_send_time_ms = 5000;
    Button generate_new_admin_ui_pass;
    Button reset_wifi_button;
    fj::VarWsPrefsRw<PointRingBuffer<HEAP_SIZE>> heap;

    typedef fj::Schema<AdminSettings,
                       fj::Field<AdminSettings, decltype(pass)>,
                       fj::Field<AdminSettings, decltype(session)>,
                       fj::Field<AdminSettings, decltype(heap_send_time_ms)>,
                       fj::Field<AdminSettings, decltype(generate_new_admin_ui_pass)>,
                       fj::Field<AdminSettings, decltype(reset_wifi_button)>,
                       fj::Field<AdminSettings, decltype(heap)>> SchemaType;

    static const SchemaType &schema() {
      static const SchemaType s = fj::makeSchema<AdminSettings>(
        fj::Field<AdminSettings, decltype(pass)>{"pass", &AdminSettings::pass},
        fj::Field<AdminSettings, decltype(session)>{"session", &AdminSettings::session},
        fj::Field<AdminSettings, decltype(heap_send_time_ms)>{"heap_send_time_ms", &AdminSettings::heap_send_time_ms},
        fj::Field<AdminSettings, decltype(generate_new_admin_ui_pass)>{"generate_new_admin_ui_pass", &AdminSettings::generate_new_admin_ui_pass},
        fj::Field<AdminSettings, decltype(reset_wifi_button)>{"reset_wifi_button", &AdminSettings::reset_wifi_button},
        fj::Field<AdminSettings, decltype(heap)>{"heap", &AdminSettings::heap}
      );
      return s;
    }
    void setSaveCallback(std::function<void()> cb) {
      pass.setOnChange(cb);
      session.setOnChange(cb);
      heap_send_time_ms.setOnChange(cb);
      heap.setOnChange(cb);
    }
  };

  AdminSettings admin;

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
    LOG_TRACE("[Model] AdminModel::begin() called");
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
      LOG_DEBUG("[Model] Generated admin password");
      changed = true;
    } else {
      LOG_TRACE_F("[Model] Admin password already set: %s", adminPw);
    }
    
    // OTA password
    const char* otaPw = ota.ota_pass;
    if (!otaPw || otaPw[0] == '\0') {
      String newPw = generatePassword(12);
      ota.ota_pass.set(newPw.c_str());
      LOG_DEBUG("[Model] Generated OTA password");
      changed = true;
    } else {
      LOG_TRACE_F("[Model] OTA password already set: %s", otaPw);
    }
    
    if (changed) {
      saveTopic("admin");
      saveTopic("ota");
    }
  }

  AdminModel() : ModelBase(80, "/ws")
  {
    // Defaults (Preferences will override these in begin())
    time.tz = "CET-1CEST,M3.5.0/2,M10.5.0/3";
    wifi.ap_ssid = "ESP-Setup";

    registerTopic("wifi", wifi);
    registerTopic("ota", ota);
    registerTopic("mdns", mdns);
    registerTopic("admin", admin);
    registerTopic("time", time);
    registerTopic("build", build);

    // Register button callbacks
    ota.generate_new_ota_pass.setCallback([this]() { this->onGenerateNewOtaPassword(); });
    ota.extend_ota_window.setCallback([this]() {
      if (this->onOtaExtendRequest) this->onOtaExtendRequest();
    });
    admin.generate_new_admin_ui_pass.setCallback([this]() { this->onGenerateNewAdminUiPassword(); });
    admin.reset_wifi_button.setCallback([this]() {
      if (this->onResetRequest) this->onResetRequest();
    });

    time.sync_now.setCallback([this]() {
      if (this->onTimeSyncNow) this->onTimeSyncNow();
    });

    admin.heap.get().setGraph("heap");
    admin.heap.get().setLabel("bytes");
    // Live graph updates over WebSocket
    admin.heap.get().setCallback(&ModelBase::graphPushCbXY, this);

    // Publish build metadata
    build.library_version.set(ESPWEBUTILS_LIBRARY_VERSION);
    build.webfiles_hash.set(ESPWEBUTILS_WEBFILES_HASH);
  }

protected:
  void on_update(const char *topic) override
  {
    LOG_TRACE_F("[Model] Model update notified for topic: %s", topic);
    
    if (strcmp(topic, "wifi") == 0) {
      LOG_INFO_F("[WiFi] SSID updated to: %s", wifi.ssid.get().c_str());
      LOG_DEBUG("[WiFi] Password field received (value not logged for security)");
      // For troubleshooting only: the password value is logged at TRACE.
      const char* pass = wifi.pass.get().c_str();
      LOG_TRACE_F("[WiFi] Password value (TRACE only): '%s' (len=%d)", pass, strlen(pass));
      LOG_TRACE("[WiFi] Triggering reconnect with new credentials");
      if (onWifiUpdate) {
        LOG_TRACE("[WiFi] Calling WiFi update callback");
        onWifiUpdate();
      }
    }
    else if (strcmp(topic, "ota") == 0) {
      LOG_DEBUG("[OTA] OTA settings updated");
      if (onOtaUpdate) onOtaUpdate();
    }
    else if (strcmp(topic, "admin") == 0) {
      LOG_DEBUG("[Admin] Admin settings updated");
      if (onAdminUpdate) onAdminUpdate();
    }
    else if (strcmp(topic, "mdns") == 0) {
      LOG_DEBUG("[mDNS] mDNS settings updated");
      if (onMdnsUpdate) onMdnsUpdate();
    }
    else if (strcmp(topic, "time") == 0) {
      LOG_DEBUG("[Time] Time settings updated");
      if (onTimeUpdate) onTimeUpdate();
    }
  }

  // Handler for OTA password generation button
  void onGenerateNewOtaPassword() {
    LOG_DEBUG("[OTA] Generating new OTA password...");
    String newPw = generatePassword(12);
    ota.ota_pass.set(newPw.c_str());
    LOG_DEBUG("[OTA] Generated new OTA password");
  }

  // Handler for Admin UI password generation button
  void onGenerateNewAdminUiPassword() {
    LOG_DEBUG("[Admin] Generating new Admin UI password (basic auth)...");
    String newPw = generatePassword(12);
    admin.pass.set(newPw.c_str());
    LOG_DEBUG("[Admin] Generated new Admin UI password");
  }

  // Push a data point to the heap ring buffer and persist/broadcast updates
  void pushHeap(uint64_t x, float y) {
    admin.heap.get().push(x, y);
    admin.heap.touch();
  }

  // Handle button trigger requests (override from ModelBase)
  void handleButtonTrigger(AsyncWebSocketClient* client, const char* topic, const char* button) override {
    LOG_DEBUG_F("[Model] handleButtonTrigger: topic=%s, button=%s", topic, button);

    if (strcmp(topic, "wifi") == 0) {
      if (strcmp(button, "scan_networks") == 0 || strcmp(button, "wifi_scan") == 0) {
        LOG_DEBUG("[WiFi] Button trigger: scan networks");
        if (onWifiScanRequest) onWifiScanRequest();
        if (client) client->text(R"({\"ok\":true,\"action\":\"scan_requested\"})");
        return;
      }
    }
    
    if (strcmp(topic, "ota") == 0) {
      if (strcmp(button, "generate_new_pass_button") == 0) {
        LOG_DEBUG("[OTA] Button trigger: generate_new_pass_button");
        onGenerateNewOtaPassword();
        if (client) client->text(R"({"ok":true,"action":"button_triggered"})");
        return;
      }

      if (strcmp(button, "extend_window_button") == 0) {
        LOG_DEBUG("[OTA] Button trigger: extend_window_button");
        if (onOtaExtendRequest) onOtaExtendRequest();
        if (client) client->text(R"({"ok":true,"action":"button_triggered"})");
        return;
      }
    }

    if (strcmp(topic, "admin") == 0) {
      if (strcmp(button, "generate_new_admin_ui_pass") == 0) {
        LOG_DEBUG("[Admin] Button trigger: generate_new_admin_ui_pass");
        onGenerateNewAdminUiPassword();
        if (client) client->text(R"({"ok":true,"action":"button_triggered"})");
        return;
      }

      if (strcmp(button, "reset_wifi_button") == 0) {
        LOG_WARN("[Admin] Button trigger: reset_wifi_button");
        if (onResetRequest) onResetRequest();
        if (client) client->text(R"({"ok":true,"action":"button_triggered"})");
        return;
      }
    }

    if (strcmp(topic, "time") == 0) {
      if (strcmp(button, "sync_now") == 0) {
        LOG_DEBUG("[Time] Button trigger: sync_now");
        if (onTimeSyncNow) onTimeSyncNow();
        if (client) client->text(R"({"ok":true,"action":"button_triggered"})");
        return;
      }
    }
    
    LOG_WARN_F("[Model] Unknown button: topic=%s, button=%s", topic, button);
    if (client) client->text(R"({"ok":false,"error":"unknown_button"})");
  }

private:
};
