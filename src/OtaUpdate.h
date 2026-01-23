#pragma once

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <functional>

class OtaUpdate
{
public:
    using StatusCallback = std::function<void(const String &)>;
    using ProgressCallback = std::function<void(unsigned int, unsigned int)>;

    void setEnabled(bool en = true)
    {
        _enabled = en;
        _writeBool("otaEnabled", en);
    }

    bool isEnabled() const { return _enabled; }

    void setPassword(const String &pass)
    {
        if (pass.length() == 0)
        {
            String gen = _generatePassword(16);
            _password = gen;
            _writeString("otaPass", gen);
        }
        else
        {
            _password = pass;
            _writeString("otaPass", pass);
        }
    }

    String getPassword()
    {
        if (_password.length())
            return _password;
        _password = _readString("otaPass", "");
        if (_password.length() == 0)
            setPassword("");
        return _password;
    }

    void clearPassword()
    {
        _password = "";
        _removeKey("otaPass");
    }

    bool isStarted() const { return _started; }
    uint16_t getPort() const { return _port; }
    uint32_t getWindowSeconds() const { return _windowSeconds; }
    String getHostname() const { return _hostname; }

    uint32_t getRemainingSeconds() const
    {
        if (!_started)
            return 0;
        if (_windowSeconds == 0)
            return 0xFFFFFFFF;
        uint32_t elapsed = (millis() - _bootMs) / 1000UL;
        if (elapsed >= _windowSeconds)
            return 0;
        return _windowSeconds - elapsed;
    }

    void restartWindow()
    {
        if (!_started)
            return;
        _expired = false;
        _bootMs = millis();
    }

    String regeneratePassword()
    {
        clearPassword();
        return getPassword();
    }

    void setHostname(const String &host) { _hostname = host; }
    void setPort(uint16_t port)
    {
        _port = port;
        _writeUShort("otaPort", port);
    }
    void setRebootOnSuccess(bool reboot)
    {
        _rebootOnSuccess = reboot;
        _writeBool("otaReboot", reboot);
    }

    void setWindowSeconds(uint32_t seconds)
    {
        _windowSeconds = seconds;
        _writeUInt("otaWindow", seconds);
    }

    void onStatus(StatusCallback cb) { _onStatus = cb; }
    void onProgress(ProgressCallback cb) { _onProgress = cb; }

    void load()
    {
        _enabled = _readBool("otaEnabled", true);
        _port = _readUShort("otaPort", 3232);
        _rebootOnSuccess = _readBool("otaReboot", true);
        _windowSeconds = _readUInt("otaWindow", 600);

        _password = "";
    }

    void beginIfNeeded(const String &fallbackHostname = "esp32")
    {
        if (_started)
            return;

        if (!_enabled)
            return;
        if (!WiFi.isConnected())
            return;

        String host = _hostname.length() ? _hostname : fallbackHostname;

        ArduinoOTA.setMdnsEnabled(false);
        ArduinoOTA.setHostname(host.c_str());
        ArduinoOTA.setPort(_port);
        ArduinoOTA.setRebootOnSuccess(_rebootOnSuccess);

        String pass = getPassword();
        ArduinoOTA.setPassword(pass.c_str());

        ArduinoOTA.onStart([this]()
                           { _emitStatus("[OTA] start"); });
        ArduinoOTA.onEnd([this]()
                         { _emitStatus("[OTA] end"); });
        ArduinoOTA.onProgress([this](unsigned int p, unsigned int t)
                              {
      if (_onProgress) _onProgress(p, t); });
        ArduinoOTA.onError([this](ota_error_t err)
                           { _emitStatus("[OTA] error: " + String((int)err)); });

        ArduinoOTA.begin();
        _started = true;
        _bootMs = millis();
        _emitStatus("[OTA] ready: host=" + host + " port=" + String(_port));
    }

    void handle()
    {
        if (_expired)
            return;
        if (!WiFi.isConnected())
            return;

        if (_windowSeconds > 0)
        {
            uint32_t now = millis();
            if ((uint32_t)(now - _bootMs) > (_windowSeconds * 1000UL))
            {
                _expired = true;
                _emitStatus("[OTA] window expired");
                return;
            }
        }

        ArduinoOTA.handle();
    }

private:
    static constexpr const char *_ns = "wifi";

    bool _enabled = true;
    bool _rebootOnSuccess = true;

    bool _started = false;
    bool _expired = false;
    uint16_t _port = 3232;
    uint32_t _windowSeconds = 600;
    uint32_t _bootMs = 0;

    String _hostname;
    String _password;

    StatusCallback _onStatus;
    ProgressCallback _onProgress;

    void _emitStatus(const String &s)
    {
        if (_onStatus)
            _onStatus(s);
    }

    String _readString(const char *key, const String &def)
    {
        Preferences p;
        p.begin(_ns, true);

        if (!p.isKey(key))
        {
            p.end();
            return def;
        }

        String v = p.getString(key, def);
        p.end();
        return v;
    }

    bool _readBool(const char *key, bool def)
    {
        Preferences p;
        p.begin(_ns, true);
        bool v = p.getBool(key, def);
        p.end();
        return v;
    }

    uint16_t _readUShort(const char *key, uint16_t def)
    {
        Preferences p;
        p.begin(_ns, true);
        uint16_t v = p.getUShort(key, def);
        p.end();
        return v;
    }

    uint32_t _readUInt(const char *key, uint32_t def)
    {
        Preferences p;
        p.begin(_ns, true);
        uint32_t v = p.getUInt(key, def);
        p.end();
        return v;
    }

    void _writeString(const char *key, const String &v)
    {
        Preferences p;
        p.begin(_ns, false);
        p.putString(key, v);
        p.end();
    }

    void _writeBool(const char *key, bool v)
    {
        Preferences p;
        p.begin(_ns, false);
        p.putBool(key, v);
        p.end();
    }

    void _writeUShort(const char *key, uint16_t v)
    {
        Preferences p;
        p.begin(_ns, false);
        p.putUShort(key, v);
        p.end();
    }

    void _writeUInt(const char *key, uint32_t v)
    {
        Preferences p;
        p.begin(_ns, false);
        p.putUInt(key, v);
        p.end();
    }

    void _removeKey(const char *key)
    {
        Preferences p;
        p.begin(_ns, false);
        p.remove(key);
        p.end();
    }

    String _generatePassword(size_t len)
    {
        static const char alphabet[] =
            "ABCDEFGHJKLMNPQRSTUVWXYZ"
            "abcdefghijkmnopqrstuvwxyz"
            "23456789"
            "!-_@";
        const size_t n = sizeof(alphabet) - 1;

        randomSeed(esp_random());

        String out;
        out.reserve(len);
        for (size_t i = 0; i < len; i++)
        {
            out += alphabet[random(0, (int)n)];
        }
        return out;
    }
};
