#!/usr/bin/env python3
"""
IceTrack Edge API mock gateway.

A dependency-free HTTP server that emulates the subset of the on-site IceTrack
Edge API used by the ESP32 firmware. It is intended for bench testing and
classroom demonstrations, not for production.

Implemented endpoints
---------------------
POST /edge/api/v1/readings
    201 + signalling directives (alarmActive, buzzerMuted, signalTest,
    maintenanceMode). Applies local threshold evaluation with a single
    temperature limit, latches buzzer mute until the excursion ends, and
    expires maintenance windows.
POST /edge/api/v1/button-events
    200 + maintenance feedback (ACTIVATED / DEACTIVATED /
    REJECTED_DAILY_LIMIT). Long-press windows use BUTTON origin, a configurable
    duration and a rolling 24-hour activation quota.
POST /edge/api/v1/mute         {"muted": true|false}
POST /edge/api/v1/signal-test  {"active": true}
POST /edge/api/v1/maintenance  {"active": true|false}   (APP origin, 120 min)
GET  /status                   Current simulated state as JSON
GET  /health                   Liveness probe
GET  /                         Small read-only dashboard

Run:
    python tools/mock_gateway.py --port 8080 --threshold 8.0
Then point AppConfig::GATEWAY_BASE_URL at http://<this-host-LAN-IP>:8080.
"""

from __future__ import annotations

import argparse
import json
import logging
import threading
from collections import deque
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Deque, Dict, Optional, Tuple

LOGGER = logging.getLogger("icetrack.gateway")

MAX_BODY_BYTES = 16 * 1024
QUOTA_WINDOW_SECONDS = 24 * 60 * 60
APP_MAINTENANCE_MINUTES = 120


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def iso(moment: Optional[datetime]) -> Optional[str]:
    if moment is None:
        return None
    return moment.isoformat(timespec="seconds").replace("+00:00", "Z")


def as_number(value: Any) -> Optional[float]:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return float(value)


class GatewayState:
    """Thread-safe simulation state shared by all request handlers."""

    def __init__(self, args: argparse.Namespace) -> None:
        self.lock = threading.Lock()
        self.threshold_c = args.threshold
        self.maintenance_minutes = args.maintenance_minutes
        self.daily_limit = args.daily_limit
        self.device_key = args.device_key

        self.maintenance_active = False
        self.maintenance_origin = ""
        self.maintenance_started_at: Optional[datetime] = None
        self.maintenance_expires_at: Optional[datetime] = None
        self.activations: Deque[datetime] = deque()

        self.buzzer_muted = False
        self.signal_test = False

        self.last_reading: Optional[Dict[str, Any]] = None
        self.last_reading_at: Optional[datetime] = None
        self.readings_received = 0
        self.readings_rejected = 0
        self.button_events = 0
        self.last_feedback: Optional[str] = None

    # -- maintenance -------------------------------------------------------

    def prune_quota(self, now: datetime) -> None:
        cutoff = now - timedelta(seconds=QUOTA_WINDOW_SECONDS)
        while self.activations and self.activations[0] < cutoff:
            self.activations.popleft()

    def expire_maintenance(self, now: datetime) -> None:
        if (
            self.maintenance_active
            and self.maintenance_expires_at is not None
            and now >= self.maintenance_expires_at
        ):
            LOGGER.info("maintenance window expired automatically")
            self.maintenance_active = False
            self.maintenance_expires_at = None
            self.maintenance_origin = ""

    def maintenance_payload(self) -> Dict[str, Any]:
        return {
            "active": self.maintenance_active,
            "origin": self.maintenance_origin or None,
            "expiresAt": iso(self.maintenance_expires_at),
        }

    def open_maintenance(self, origin: str, minutes: int, now: datetime) -> None:
        self.maintenance_active = True
        self.maintenance_origin = origin
        self.maintenance_started_at = now
        self.maintenance_expires_at = now + timedelta(minutes=minutes)

    def close_maintenance(self) -> None:
        self.maintenance_active = False
        self.maintenance_origin = ""
        self.maintenance_started_at = None
        self.maintenance_expires_at = None

    def toggle_button_maintenance(self) -> Dict[str, Any]:
        now = utc_now()
        self.expire_maintenance(now)

        if self.maintenance_active:
            self.close_maintenance()
            feedback = "DEACTIVATED"
        else:
            self.prune_quota(now)
            if len(self.activations) >= self.daily_limit:
                feedback = "REJECTED_DAILY_LIMIT"
            else:
                self.open_maintenance("BUTTON", self.maintenance_minutes, now)
                self.activations.append(now)
                feedback = "ACTIVATED"

        self.last_feedback = feedback
        return {
            "feedback": feedback,
            "maintenanceActive": self.maintenance_active,
            "maintenanceMode": self.maintenance_payload(),
        }

    def set_app_maintenance(self, active: bool) -> Dict[str, Any]:
        now = utc_now()
        self.expire_maintenance(now)
        if active:
            self.open_maintenance("APP", APP_MAINTENANCE_MINUTES, now)
        else:
            self.close_maintenance()
        return {"maintenanceActive": self.maintenance_active, "maintenanceMode": self.maintenance_payload()}

    # -- readings ----------------------------------------------------------

    def evaluate_reading(self, payload: Dict[str, Any]) -> Tuple[Optional[int], Dict[str, Any]]:
        """Validates a reading. Returns (http_status, response_or_error)."""
        device_id = payload.get("deviceId")
        temperature = as_number(payload.get("temperature"))

        if not isinstance(device_id, str) or not device_id.strip():
            return 400, {"error": "Invalid reading payload", "detail": "deviceId is required"}
        if temperature is None:
            return 400, {"error": "Invalid reading payload", "detail": "temperature must be numeric"}
        if temperature < 0.0 or temperature > 50.0:
            return 400, {"error": "Invalid reading payload", "detail": "temperature outside 0..50 C"}

        humidity = payload.get("humidity")
        if humidity is not None:
            humidity = as_number(humidity)
            if humidity is None or humidity < 20.0 or humidity > 90.0:
                return 400, {"error": "Invalid reading payload", "detail": "humidity outside 20..90 %"}

        light_level = payload.get("lightLevel")
        if light_level is not None:
            light_level = as_number(light_level)
            if light_level is None or light_level < 0.0 or light_level > 100.0:
                return 400, {"error": "Invalid reading payload", "detail": "lightLevel outside 0..100"}

        presence = payload.get("presence")
        if presence is not None and not isinstance(presence, bool):
            return 400, {"error": "Invalid reading payload", "detail": "presence must be boolean"}

        now = utc_now()
        self.expire_maintenance(now)

        alarm_active = temperature > self.threshold_c
        if not alarm_active:
            # US-49: mute latches only for the duration of the excursion.
            self.buzzer_muted = False

        self.last_reading = payload
        self.last_reading_at = now
        self.readings_received += 1

        response: Dict[str, Any] = {
            "alarmActive": alarm_active,
            "buzzerMuted": self.buzzer_muted,
            "signalTest": False,
            "maintenanceActive": self.maintenance_active,
            "maintenanceMode": self.maintenance_payload(),
            "acceptedAt": iso(now),
            "readingUid": payload.get("readingUid"),
        }

        # Signal test is a one-shot pulse: the next reading clears it, exactly
        # like a pending technician command consumed by the device.
        if self.signal_test:
            response["signalTest"] = True
            self.signal_test = False

        return 201, response

    # -- status ------------------------------------------------------------

    def snapshot(self) -> Dict[str, Any]:
        now = utc_now()
        self.expire_maintenance(now)
        self.prune_quota(now)
        alarm_active = False
        if self.last_reading is not None:
            temperature = as_number(self.last_reading.get("temperature"))
            alarm_active = temperature is not None and temperature > self.threshold_c

        return {
            "status": "ok",
            "time": iso(now),
            "thresholdCelsius": self.threshold_c,
            "alarmActive": alarm_active,
            "buzzerMuted": self.buzzer_muted,
            "signalTestPending": self.signal_test,
            "maintenance": {
                **self.maintenance_payload(),
                "activationsLast24h": len(self.activations),
                "dailyLimit": self.daily_limit,
            },
            "lastReading": self.last_reading,
            "lastReadingAt": iso(self.last_reading_at),
            "stats": {
                "readingsReceived": self.readings_received,
                "readingsRejected": self.readings_rejected,
                "buttonEvents": self.button_events,
                "lastButtonFeedback": self.last_feedback,
            },
        }


class GatewayHandler(BaseHTTPRequestHandler):
    server_version = "IceTrackMockGateway/1.0"
    protocol_version = "HTTP/1.1"

    state: GatewayState  # injected by the server factory

    # -- plumbing ----------------------------------------------------------

    def log_message(self, fmt: str, *args: Any) -> None:
        # Request logs are emitted explicitly with structured context.
        return

    def send_json(self, status: int, payload: Dict[str, Any]) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def send_html(self, status: int, html: str) -> None:
        body = html.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def read_json(self) -> Tuple[Optional[Dict[str, Any]], Optional[str]]:
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            return None, "invalid Content-Length"
        if length <= 0:
            return {}, None
        if length > MAX_BODY_BYTES:
            return None, "body too large"
        raw = self.rfile.read(length)
        try:
            payload = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            return None, f"invalid JSON: {exc}"
        if not isinstance(payload, dict):
            return None, "JSON body must be an object"
        return payload, None

    def authorized(self) -> bool:
        if not self.state.device_key:
            return True
        return self.headers.get("X-Device-Key") == self.state.device_key

    def require_device_key(self) -> bool:
        if self.authorized():
            return True
        self.send_json(401, {"error": "Unauthorized", "detail": "invalid X-Device-Key"})
        return False

    # -- routes ------------------------------------------------------------

    def do_GET(self) -> None:  # noqa: N802 (http.server API)
        path = self.path.split("?", 1)[0]
        if path == "/health":
            self.send_json(200, {"status": "ok", "time": iso(utc_now())})
            return
        if path == "/status":
            with self.state.lock:
                snapshot = self.state.snapshot()
            self.send_json(200, snapshot)
            return
        if path in ("/", "/index.html"):
            self.send_html(200, DASHBOARD_HTML)
            return
        self.send_json(404, {"error": "Not found", "path": path})

    def do_POST(self) -> None:  # noqa: N802 (http.server API)
        path = self.path.split("?", 1)[0]
        if not self.require_device_key():
            return

        payload, error = self.read_json()
        if payload is None:
            self.send_json(400, {"error": "Invalid request body", "detail": error})
            return

        if path == "/edge/api/v1/readings":
            with self.state.lock:
                status, response = self.state.evaluate_reading(payload)
                if status != 201:
                    self.state.readings_rejected += 1
            LOGGER.info(
                "reading device=%s t=%s h=%s light=%s presence=%s -> %s",
                payload.get("deviceId"),
                payload.get("temperature"),
                payload.get("humidity"),
                payload.get("lightLevel"),
                payload.get("presence"),
                status,
            )
            self.send_json(status, response)
            return

        if path == "/edge/api/v1/button-events":
            if payload.get("pressType") != "LONG":
                self.send_json(400, {"error": "Invalid button event", "detail": "pressType must be LONG"})
                return
            if not isinstance(payload.get("deviceId"), str):
                self.send_json(400, {"error": "Invalid button event", "detail": "deviceId is required"})
                return
            with self.state.lock:
                self.state.button_events += 1
                response = self.state.toggle_button_maintenance()
            LOGGER.info(
                "button device=%s event=%s -> %s",
                payload.get("deviceId"),
                payload.get("eventUid"),
                response["feedback"],
            )
            self.send_json(200, response)
            return

        if path == "/edge/api/v1/mute":
            muted = payload.get("muted", True)
            if not isinstance(muted, bool):
                self.send_json(400, {"error": "muted must be boolean"})
                return
            with self.state.lock:
                self.state.buzzer_muted = muted
            LOGGER.info("buzzer muted=%s", muted)
            self.send_json(200, {"ok": True, "buzzerMuted": muted})
            return

        if path == "/edge/api/v1/signal-test":
            active = payload.get("active", True)
            if not isinstance(active, bool):
                self.send_json(400, {"error": "active must be boolean"})
                return
            with self.state.lock:
                self.state.signal_test = active
            LOGGER.info("signal test pending=%s", active)
            self.send_json(200, {"ok": True, "signalTest": active})
            return

        if path == "/edge/api/v1/maintenance":
            active = payload.get("active", True)
            if not isinstance(active, bool):
                self.send_json(400, {"error": "active must be boolean"})
                return
            with self.state.lock:
                response = self.state.set_app_maintenance(active)
            LOGGER.info("APP maintenance active=%s", active)
            self.send_json(200, response)
            return

        self.send_json(404, {"error": "Not found", "path": path})


DASHBOARD_HTML = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>IceTrack mock gateway</title>
<style>
  :root { color-scheme: dark; }
  body { font: 15px/1.5 system-ui, sans-serif; margin: 0; padding: 24px; background: #0d1720; color: #e6edf3; }
  h1 { font-size: 20px; margin: 0 0 4px; }
  p.sub { margin: 0 0 20px; color: #8b98a5; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 14px; }
  .card { background: #151f2b; border: 1px solid #253243; border-radius: 10px; padding: 14px 16px; }
  .card h2 { font-size: 12px; letter-spacing: .08em; text-transform: uppercase; color: #8b98a5; margin: 0 0 8px; }
  .big { font-size: 26px; font-weight: 600; }
  .ok { color: #3fb950; } .warn { color: #d29922; } .bad { color: #f85149; } .muted { color: #8b98a5; }
  pre { background: #0a1118; border: 1px solid #253243; border-radius: 8px; padding: 12px; overflow: auto; font-size: 12px; }
  .row { display: flex; justify-content: space-between; gap: 12px; padding: 3px 0; border-bottom: 1px solid #1c2733; }
  .row:last-child { border: 0; }
</style>
</head>
<body>
<h1>IceTrack mock gateway</h1>
<p class="sub">Bench-only Edge API simulator. Refreshes every 2 seconds.</p>
<div class="grid">
  <div class="card"><h2>Alarm</h2><div id="alarm" class="big muted">-</div></div>
  <div class="card"><h2>Buzzer</h2><div id="buzzer" class="big muted">-</div></div>
  <div class="card"><h2>Maintenance</h2><div id="maintenance" class="big muted">-</div><div id="maintenance-detail" class="muted"></div></div>
  <div class="card"><h2>Last reading</h2><div id="reading" class="big muted">-</div><div id="reading-detail" class="muted"></div></div>
</div>
<div class="card" style="margin-top:14px"><h2>Raw status</h2><pre id="raw">loading...</pre></div>
<script>
const $ = (id) => document.getElementById(id);
async function refresh() {
  try {
    const res = await fetch("/status", { cache: "no-store" });
    const s = await res.json();
    $("alarm").textContent = s.alarmActive ? "EXCURSION" : "NORMAL";
    $("alarm").className = "big " + (s.alarmActive ? "bad" : "ok");
    $("buzzer").textContent = s.buzzerMuted ? "MUTED" : (s.alarmActive ? "SOUNDING" : "SILENT");
    $("buzzer").className = "big " + (s.buzzerMuted ? "warn" : (s.alarmActive ? "bad" : "ok"));
    $("maintenance").textContent = s.maintenance.active ? "ACTIVE" : "INACTIVE";
    $("maintenance").className = "big " + (s.maintenance.active ? "warn" : "muted");
    $("maintenance-detail").textContent = s.maintenance.active
      ? `origin ${s.maintenance.origin} / expires ${s.maintenance.expiresAt}`
      : `${s.maintenance.activationsLast24h}/${s.maintenance.dailyLimit} button activations in 24 h`;
    const r = s.lastReading;
    $("reading").textContent = r ? `${r.temperature} C / ${r.humidity} %` : "none yet";
    $("reading").className = "big " + (r ? "ok" : "muted");
    $("reading-detail").textContent = r
      ? `light ${r.lightLevel}% / presence ${r.presence} / valid ${r.sensorValid}`
      : "waiting for the ESP32";
    $("raw").textContent = JSON.stringify(s, null, 2);
  } catch (err) {
    $("raw").textContent = "gateway unreachable: " + err;
  }
}
refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="IceTrack Edge API mock gateway")
    parser.add_argument("--host", default="0.0.0.0", help="bind address (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8080, help="TCP port (default: 8080)")
    parser.add_argument(
        "--threshold",
        type=float,
        default=8.0,
        help="temperature alarm threshold in C (default: 8.0)",
    )
    parser.add_argument(
        "--maintenance-minutes",
        type=int,
        default=30,
        help="BUTTON maintenance window duration in minutes (default: 30)",
    )
    parser.add_argument(
        "--daily-limit",
        type=int,
        default=3,
        help="BUTTON activations allowed per rolling 24 h (default: 3)",
    )
    parser.add_argument(
        "--device-key",
        default="",
        help="optional X-Device-Key required on every POST (default: disabled)",
    )
    parser.add_argument("--quiet", action="store_true", help="only log warnings and errors")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    logging.basicConfig(
        level=logging.WARNING if args.quiet else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        datefmt="%H:%M:%S",
    )

    GatewayHandler.state = GatewayState(args)
    server = ThreadingHTTPServer((args.host, args.port), GatewayHandler)
    server.daemon_threads = True

    LOGGER.info("IceTrack mock gateway listening on http://%s:%d", args.host, args.port)
    LOGGER.info(
        "threshold=%.1f C, button window=%d min, daily limit=%d, key=%s",
        args.threshold,
        args.maintenance_minutes,
        args.daily_limit,
        "enabled" if args.device_key else "disabled",
    )
    LOGGER.info("dashboard: http://127.0.0.1:%d/", args.port)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        LOGGER.info("stopping")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
