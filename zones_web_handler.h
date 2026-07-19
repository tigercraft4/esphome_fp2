// zones_web_handler.h
//
// WEBUI-01/WEBUI-02/WEBUI-05 (11-03): device-hosted /zones editor transport
// layer. Two AsyncWebHandler subclasses:
//   - ZonesPageHandler serves the flash-resident /zones page (zones_page_bundle.h,
//     11-02) at its own sub-path, not device root / (D-03) - the built-in
//     web_server diagnostics dashboard at / is unaffected.
//   - ZonesApiHandler serves the /api/zones* JSON contract the 11-02 bundle
//     already assumes: GET /api/zones (list), GET /api/zones/status (honest
//     {pending, ok, error} confirmation state), and POST /api/zones/save
//     (deferred-only mutation, never called from the httpd task itself).
//
// Follows the same "manual, non-blocking, main-loop-driven" philosophy the
// DIAG-02 telnet bridge already established in this codebase (fp2_component.cpp) -
// no FreeRTOS task, mutex, semaphore, or queue primitive is introduced here.
#pragma once

#include "esphome/components/web_server_idf/web_server_idf.h"
#include "esphome/core/application.h"
#include "esphome/components/aqara_fp2/fp2_component.h"
#include "zones_page_bundle.h"
#include <ArduinoJson.h>
#include <cstdlib>

// Serves the /zones editor page from the 11-02 PROGMEM bundle.
class ZonesPageHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    return request->method() == HTTP_GET && request->url() == "/zones";
  }

  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    auto *response = request->beginResponse(200, "text/html",
        reinterpret_cast<const uint8_t *>(ZONES_PAGE_HTML), ZONES_PAGE_HTML_SIZE);
    request->send(response);
  }
};

// Serves the /api/zones* JSON contract: list, save (deferred), status.
class ZonesApiHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  explicit ZonesApiHandler(esphome::aqara_fp2::FP2Component *fp2) : fp2_(fp2) {}

  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    auto method = request->method();
    std::string url = request->url();
    if (method == HTTP_GET && (url == "/api/zones" || url == "/api/zones/status")) {
      return true;
    }
    if (method == HTTP_POST && url == "/api/zones/save") {
      return true;
    }
    return false;
  }

  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    std::string url = request->url();
    if (request->method() == HTTP_GET && url == "/api/zones") {
      this->handle_get_zones_(request);
    } else if (request->method() == HTTP_GET && url == "/api/zones/status") {
      this->handle_get_status_(request);
    } else if (request->method() == HTTP_POST && url == "/api/zones/save") {
      this->handle_post_save_(request);
    }
  }

 protected:
  // GET /api/zones - WEBUI-01: read-only zone list + live geometry fields
  // for the /zones page's list view and live overlay. Safe from the httpd
  // task because zones_/mounting fields are immutable post-setup (Pitfall 3).
  // An empty zones_ serializes to an empty zones array, not an error.
  void handle_get_zones_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    this->fp2_->json_get_map_data(root);
    std::string out;
    serializeJson(doc, out);
    request->send(200, "application/json", out.c_str());
  }

  // GET /api/zones/status - WEBUI-05: honest {pending, ok, error} save
  // confirmation state, read synchronously (plain field reads, safe on the
  // httpd task) - never an optimistic client-side guess.
  void handle_get_status_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["pending"] = this->fp2_->save_pending();
    doc["ok"] = this->fp2_->save_ok();
    doc["error"] = this->fp2_->save_error();
    std::string out;
    serializeJson(doc, out);
    request->send(200, "application/json", out.c_str());
  }

  // POST /api/zones/save - WEBUI-02: submit-then-poll deferred mutation.
  // zone_id/sensitivity are required; a missing one is rejected with 400 and
  // nothing is scheduled (empty/null-input probe). zone_type is optional,
  // defaulting to -1 (unset). The grid is never accepted from the client -
  // save_zone_from_editor() looks it up server-side by zone_id (D-01). The
  // only mutation reference is inside the scheduler lambda below - this
  // method never calls a FP2Component write method directly on the httpd
  // task, and never waits on save_pending() (no spin-wait, no wait_until:
  // port).
  void handle_post_save_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    if (request->getParam("zone_id") == nullptr || request->getParam("sensitivity") == nullptr) {
      request->send(400, "application/json", R"({"error":"zone_id and sensitivity are required"})");
      return;
    }

    // WR-03: reject a second save while one is already in flight instead of
    // silently superseding it. App.scheduler.set_timeout() below uses a
    // fixed "zone_editor_save" name, and ESPHome's scheduler
    // cancels/replaces an existing pending timeout registered under the
    // same (component, name) pair - a second POST would otherwise silently
    // drop the first save even though it already received a 202 promising
    // it would happen. save_pending() is now synchronous with the 202
    // response (CR-01), so this check reliably catches the in-flight window.
    if (this->fp2_->save_pending()) {
      request->send(409, "application/json", R"({"error":"a save is already in progress"})");
      return;
    }

    // WR-02: atoi() returns 0 for non-numeric input and the (uint8_t) cast
    // in the scheduler lambda below silently truncates out-of-range values
    // modulo 256 (e.g. sensitivity=259 -> atoi -> 259 -> (uint8_t)259 -> 3,
    // a "valid"-looking sensitivity saved with no error surfaced anywhere).
    // Validate with strtol()+endptr+range before narrowing so garbage input
    // is rejected with 400 instead of silently coerced into a plausible
    // value.
    const std::string zone_id_str = request->arg("zone_id");
    char *end = nullptr;
    long zone_id_l = strtol(zone_id_str.c_str(), &end, 10);
    if (end == zone_id_str.c_str() || *end != '\0' || zone_id_l < 0 || zone_id_l > 255) {
      request->send(400, "application/json", R"({"error":"zone_id must be an integer 0-255"})");
      return;
    }

    const std::string sensitivity_str = request->arg("sensitivity");
    end = nullptr;
    long sensitivity_l = strtol(sensitivity_str.c_str(), &end, 10);
    if (end == sensitivity_str.c_str() || *end != '\0' || sensitivity_l < 1 || sensitivity_l > 3) {
      request->send(400, "application/json", R"({"error":"sensitivity must be 1-3"})");
      return;
    }

    int zone_id = (int) zone_id_l;
    int sensitivity = (int) sensitivity_l;
    int zone_type = -1;
    if (request->getParam("zone_type") != nullptr) {
      const std::string zone_type_str = request->arg("zone_type");
      end = nullptr;
      long zone_type_l = strtol(zone_type_str.c_str(), &end, 10);
      if (end == zone_type_str.c_str() || *end != '\0') {
        request->send(400, "application/json", R"({"error":"zone_type must be an integer"})");
        return;
      }
      zone_type = (int) zone_type_l;
    }

    // CR-01: mark the save as pending synchronously, before scheduling the
    // deferred mutation, so a GET /api/zones/status that lands immediately
    // after this 202 response can never observe a stale {pending:false}.
    // This is in-memory bookkeeping only (no radar write, no FreeRTOS
    // primitive) - the actual save_zone_from_editor() call remains inside
    // the scheduler lambda below, unchanged.
    this->fp2_->mark_editor_save_queued();

    esphome::aqara_fp2::FP2Component *fp2 = this->fp2_;
    esphome::App.scheduler.set_timeout(this->fp2_, "zone_editor_save", 1,
        [fp2, zone_id, sensitivity, zone_type]() {
          fp2->save_zone_from_editor((uint8_t) zone_id, (uint8_t) sensitivity, zone_type);
        });

    request->send(202, "application/json", R"({"status":"pending"})");
  }

  esphome::aqara_fp2::FP2Component *fp2_;
};
