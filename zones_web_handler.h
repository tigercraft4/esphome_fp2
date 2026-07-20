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
    if (method == HTTP_GET &&
        (url == "/api/zones" || url == "/api/zones/status" || url == "/api/zones/free-slots")) {
      return true;
    }
    if (method == HTTP_POST &&
        (url == "/api/zones/save" || url == "/api/zones/create" || url == "/api/zones/delete")) {
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
    } else if (request->method() == HTTP_GET && url == "/api/zones/free-slots") {
      this->handle_get_free_slots_(request);
    } else if (request->method() == HTTP_POST && url == "/api/zones/save") {
      this->handle_post_save_(request);
    } else if (request->method() == HTTP_POST && url == "/api/zones/create") {
      this->handle_post_create_(request);
    } else if (request->method() == HTTP_POST && url == "/api/zones/delete") {
      this->handle_post_delete_(request);
    }
  }

 protected:
  // GET /api/zones - WEBUI-01: read-only zone list + live geometry fields
  // for the /zones page's list view and live overlay.
  // CR-01 fix (12-REVIEW iter2): zones_/mounting fields are NOT immutable
  // post-setup anymore - add_zone_at_runtime()/remove_zone_at_runtime() run
  // on the main-loop task and mutate zones_ while this handler runs on the
  // httpd task. Safety now comes from FP2Component::zones_'s own
  // reserve(32)-at-boot + never-erase/deactivate-in-place invariants (see
  // fp2_component.h's zones_ comment), which keep this vector's backing
  // storage stable for a concurrent range-for like json_get_map_data()'s.
  // An empty (or all-inactive) zones_ serializes to an empty zones array,
  // not an error.
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

  // POST /api/zones/create - ZONEMGMT-01: submit-then-poll deferred zone
  // creation, sharing the /api/zones/save handler's WR-02/WR-03/CR-01
  // precedents verbatim. zone_id and sensitivity are required; zone_type is
  // optional, defaulting to -1 (unset). zone_id's valid range is tighter
  // here (0-31) than save's 0-255, matching the registry's 32-slot ceiling.
  // The only add_zone_at_runtime() reference is inside the scheduler lambda
  // below - this method never mutates FP2Component state directly on the
  // httpd task.
  //
  // WR-03 (12-REVIEW): this endpoint has NO CSRF protection - no origin/
  // referrer check, no CSRF token, and (per project convention) no auth by
  // default. A malicious auto-submitting form on any page a LAN user's
  // browser visits can create a zone with zero interaction with this
  // device's own UI. Larger blast radius than /api/zones/save (which can
  // only mutate an already-compiled zone, not delete one) - see the
  // matching warning logged at setup() time. Mitigate by enabling the
  // ESPHome `web_server: auth:` block if this device is reachable by
  // untrusted clients on the LAN.
  void handle_post_create_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    if (request->getParam("zone_id") == nullptr || request->getParam("sensitivity") == nullptr) {
      request->send(400, "application/json", R"({"error":"zone_id and sensitivity are required"})");
      return;
    }

    // WR-03: shared in-flight guard across save/create/delete - a create
    // must not be allowed to race a save or a delete already in progress.
    if (this->fp2_->save_pending()) {
      request->send(409, "application/json", R"({"error":"a save is already in progress"})");
      return;
    }

    // WR-02: strtol()+endptr+range validation before any narrowing cast, so
    // garbage input is rejected with 400 instead of silently coerced.
    const std::string zone_id_str = request->arg("zone_id");
    char *end = nullptr;
    long zone_id_l = strtol(zone_id_str.c_str(), &end, 10);
    if (end == zone_id_str.c_str() || *end != '\0' || zone_id_l < 0 || zone_id_l > 31) {
      request->send(400, "application/json", R"({"error":"zone_id must be an integer 0-31"})");
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

    // CR-01: mark the mutation as pending synchronously, before scheduling,
    // so a GET /api/zones/status landing immediately after this 202 can
    // never observe a stale {pending:false}.
    this->fp2_->mark_editor_save_queued();

    esphome::aqara_fp2::FP2Component *fp2 = this->fp2_;
    esphome::App.scheduler.set_timeout(this->fp2_, "zone_add", 1,
        [fp2, zone_id, sensitivity, zone_type]() {
          fp2->add_zone_at_runtime((uint8_t) zone_id, (uint8_t) sensitivity, zone_type);
        });

    request->send(202, "application/json", R"({"status":"pending"})");
  }

  // POST /api/zones/delete - ZONEMGMT-02: submit-then-poll deferred zone
  // removal, sharing the same WR-02/WR-03/CR-01 precedents. Only zone_id is
  // required. The scheduler name is deliberately distinct ("zone_remove")
  // from both "zone_add" and "zone_editor_save" - a delete must never be
  // able to silently cancel a concurrent create (or vice versa) by
  // colliding on the same (component, name) scheduler slot (WR-03 root
  // cause). remove_zone_at_runtime() is referenced ONLY inside the
  // scheduler lambda below.
  //
  // WR-03 (12-REVIEW): NO CSRF protection - see handle_post_create_()'s
  // identical warning above. This is the more dangerous of the two: a
  // one-line auto-submitting HTML form silently deletes a configured zone
  // with zero user interaction with this device's own UI.
  void handle_post_delete_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    if (request->getParam("zone_id") == nullptr) {
      request->send(400, "application/json", R"({"error":"zone_id is required"})");
      return;
    }

    // WR-03: shared in-flight guard across save/create/delete.
    if (this->fp2_->save_pending()) {
      request->send(409, "application/json", R"({"error":"a save is already in progress"})");
      return;
    }

    const std::string zone_id_str = request->arg("zone_id");
    char *end = nullptr;
    long zone_id_l = strtol(zone_id_str.c_str(), &end, 10);
    if (end == zone_id_str.c_str() || *end != '\0' || zone_id_l < 0 || zone_id_l > 31) {
      request->send(400, "application/json", R"({"error":"zone_id must be an integer 0-31"})");
      return;
    }

    int zone_id = (int) zone_id_l;

    // CR-01: mark the mutation as pending synchronously, before scheduling.
    this->fp2_->mark_editor_save_queued();

    esphome::aqara_fp2::FP2Component *fp2 = this->fp2_;
    esphome::App.scheduler.set_timeout(this->fp2_, "zone_remove", 1, [fp2, zone_id]() {
      fp2->remove_zone_at_runtime((uint8_t) zone_id);
    });

    request->send(202, "application/json", R"({"status":"pending"})");
  }

  // GET /api/zones/free-slots - ZONEMGMT-04: dropdown data source for the
  // Add-Zone UI (D-01/D-07). Mirrors handle_get_zones_ exactly - a
  // read-only scan of the live zones_ union. CR-01 fix (12-REVIEW iter2):
  // see handle_get_zones_()'s updated comment above - safety against the
  // main loop's concurrent push_back()/deactivate-in-place comes from
  // zones_'s reserve(32)-at-boot + never-erase invariants, not from zones_
  // being immutable.
  void handle_get_free_slots_(esphome::web_server_idf::AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    this->fp2_->json_get_free_slots(root);
    std::string out;
    serializeJson(doc, out);
    request->send(200, "application/json", out.c_str());
  }

  esphome::aqara_fp2::FP2Component *fp2_;
};
