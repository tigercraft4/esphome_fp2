// spike_handler.h
//
// SPIKE-01 (Phase 10 Architecture Validation Spike) — minimal AsyncWebHandler
// subclass serving the /spike stub route (D-08: fixed canned response, no
// attacker-controlled input) so the soak (plan 10-03) can exercise
// web_server_base::add_handler() alongside full UART/WiFi/native-API load.
//
// D-10: this file is TEMPORARY — deleted from the project after the soak
// completes, regardless of go/no-go verdict.
//
// Intentional deviation from the project's normal component-header
// convention: no wrapping project namespace. This is throwaway spike code,
// not a real ESPHome external_component (justified by D-10).
//
// Source: pattern derived from esphome/esphome web_server_idf.h @ 2025.12.4
#pragma once

#include "esphome/components/web_server_idf/web_server_idf.h"
#include <esp_heap_caps.h>

class SpikeHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    return request->method() == HTTP_GET && request->url() == "/spike";
  }
  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    request->send(200, "text/plain", "spike-ok");
  }
};
