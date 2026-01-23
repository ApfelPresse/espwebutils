#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>

class LiveGraphManager
{
public:
    struct Point
    {
        double x;
        double y;
    };

    explicit LiveGraphManager(AsyncWebSocket &ws, size_t maxPoints = 20)
        : _ws(ws), _maxPoints(maxPoints) {}

    void begin()
    {
        _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                           AwsEventType type, void *arg, uint8_t *data, size_t len)
                    {
      (void)server; (void)arg; (void)data; (void)len;

      if (type == WS_EVT_CONNECT) {
        // Snapshot beim Connect
        sendSnapshot(client);
      } });
    }

    using NowFunc = std::function<double(void)>;

    void setNowProvider(NowFunc fn) { _now = fn; }

    void pushData(const String &graph, const String &label, double y)
    {
        double x = _now ? _now() : 0.0;
        pushData(graph, label, x, y);
    }

    void cleanup() { _ws.cleanupClients(); }

    void pushData(const String &graph, const String &label, double x, double y)
    {
        Series *s = _getOrCreateSeries(graph, label);
        _pushRing(*s, {x, y});

        StaticJsonDocument<256> doc;
        doc["type"] = "data";
        doc["graph"] = graph;
        doc["label"] = label;
        doc["x"] = x;
        doc["y"] = y;

        Serial.println("[LiveGraph] Pushing data: graph=" + graph + " label=" + label + " x=" + String(x) + " y=" + String(y));
        String json;
        serializeJson(doc, json);
        _ws.textAll(json);
    }

    void clearAll() { _series.clear(); }

private:
    struct Series
    {
        String graph;
        String label;

        std::vector<Point> buf;
        size_t head = 0;
        size_t count = 0;

        Series(const String &g, const String &l, size_t maxPoints)
            : graph(g), label(l), buf(maxPoints) {}
    };
    NowFunc _now;
    AsyncWebSocket &_ws;
    size_t _maxPoints;
    std::vector<Series> _series;

    Series *_getOrCreateSeries(const String &graph, const String &label)
    {
        for (auto &s : _series)
        {
            if (s.graph == graph && s.label == label)
                return &s;
        }
        _series.emplace_back(graph, label, _maxPoints);
        return &_series.back();
    }

    void _pushRing(Series &s, const Point &p)
    {
        s.buf[s.head] = p;
        s.head = (s.head + 1) % _maxPoints;
        if (s.count < _maxPoints)
            s.count++;
    }

    void sendSnapshot(AsyncWebSocketClient *client)
    {
        for (auto &s : _series)
        {
            StaticJsonDocument<1024> doc;
            doc["type"] = "init";
            doc["graph"] = s.graph;
            doc["label"] = s.label;

            JsonArray pts = doc.createNestedArray("points");

            size_t start = (s.count == _maxPoints) ? s.head : 0;
            for (size_t i = 0; i < s.count; i++)
            {
                size_t idx = (start + i) % _maxPoints;
                JsonObject p = pts.createNestedObject();
                p["x"] = s.buf[idx].x;
                p["y"] = s.buf[idx].y;
            }

            String json;
            serializeJson(doc, json);
            client->text(json);
        }

        {
            StaticJsonDocument<96> done;
            done["type"] = "init_done";
            String json;
            serializeJson(done, json);
            client->text(json);
        }
    }
};
