# Camera Telemetry Overlay — Design Spec

**Date:** 2026-06-18

## Goal

Show the real-time state of the ESP32-CAM (what it is doing, signal strength, health) as a HUD overlay on the VideoHud component, analogous to how GPS state is shown in SystemStatus.

## Scope

- Modify both camera firmwares to publish diagnostics via MQTT
- Subscribe to new topic in `control.html` and derive camera state
- Add a status panel overlay to `VideoHud.js` (top-right corner)

## Firmware Changes

**Files:** `ESP32/Camera_Marcelo_20fps_480x320.ino` and `ESP32/Camera_Marcelo_5fps_640x480.ino`

Both firmwares are structurally identical. Changes are the same in both.

### New MQTT publish — `rover/cam/status`

Published every 5 seconds, merged into the existing diagnostic cycle (`lastStatusPrint` / 5000ms).

**Payload:**

```json
{
  "state": "STREAMING",
  "mode": "20fps-hvga",
  "rssi": -62,
  "ram_kb": 142,
  "frames_sent": 1840,
  "frames_failed": 3,
  "ws": true
}
```

- `state`: `"CONNECTING"` if WebSocket is not connected, `"STREAMING"` if WS connected and sending frames.
- `mode`: `"20fps-hvga"` for the 480x320 firmware, `"5fps-vga"` for the 640x480 firmware. Hardcoded string constant.
- `rssi`: `WiFi.RSSI()` integer dBm.
- `ram_kb`: `ESP.getFreeHeap() / 1024` already used in Serial print.
- `frames_sent` / `frames_failed`: existing counters.
- `ws`: `webSocket.isConnected()`.

The existing `publishIp()` / `mqttPublishInterval` (30s) cycle is kept unchanged. The new status publish uses the 5s `lastStatusPrint` cycle. A new `publishCamStatus()` helper is added and called alongside `Serial.printf` in that cycle.

## Data Flow

```
ESP32-CAM
  -- rover/cam/status (every 5s) --> MQTT broker
                                          |
                                   control.html
                                   useMqttSystem()
                                   subscribe rover/cam/status
                                   derive camStatus state
                                          |
                                   VideoHud (prop: camStatus)
                                   CamStatusPanel overlay
```

## State Machine (frontend)

Derived in `control.html` from incoming `rover/cam/status` payloads:

| State        | Condition                                                           |
| ------------ | ------------------------------------------------------------------- |
| `OFFLINE`    | No MQTT received from cam in >10s (timeout)                         |
| `CONNECTING` | MQTT alive but `ws: false`                                          |
| `STREAMING`  | `ws: true`                                                          |
| `DEGRADED`   | `ws: true` AND (`rssi < -80` OR `frames_failed/frames_sent > 0.05`) |

A `useRef` timeout of 10s resets state to `OFFLINE` if no message arrives.

### State display config (mirrors GPS_CONFIG pattern)

```js
const CAM_CONFIG = {
  OFFLINE: {
    color: "text-red-500",
    dot: "bg-red-700",
    label: "OFFLINE",
    animate: "",
  },
  CONNECTING: {
    color: "text-yellow-400",
    dot: "bg-yellow-500",
    label: "CONNECTING",
    animate: "animate-pulse",
  },
  STREAMING: {
    color: "text-green-400",
    dot: "bg-green-500",
    label: "STREAMING",
    animate: "",
  },
  DEGRADED: {
    color: "text-yellow-400",
    dot: "bg-yellow-500",
    label: "DEGRADED",
    animate: "animate-pulse",
  },
};
```

## Frontend Changes

### `control.html` — `useMqttSystem()`

1. Add `camStatus` state: `{ state: "OFFLINE", mode: null, rssi: null, ram_kb: null, frames_sent: 0, frames_failed: 0, ws: false }`.
2. Subscribe to `rover/cam/status` in `onConnect`.
3. On message: parse payload, compute derived state (STREAMING/DEGRADED/CONNECTING), update `camStatus`, reset 10s offline timeout.
4. Pass `camStatus` as prop to `<VideoHud>`.

### `VideoHud.js` — `CamStatusPanel`

New sub-component rendered as an overlay in the top-right corner of the HUD div.

**Visual layout:**

```
+-----------------------------+
| * STREAMING    20fps-hvga   |
| ########..  -62 dBm         |
| Env:1840  Fail:3  RAM:142KB |
+-----------------------------+
```

- **Dot + label**: colored per state, `animate-pulse` on CONNECTING/DEGRADED.
- **Mode badge**: `"20fps-hvga"` or `"5fps-vga"`, slate-500 monospace text.
- **RSSI bar**: 10 segments, filled proportionally from -40 dBm (full) to -90 dBm (empty). Hides when OFFLINE.
- **Counters row**: `Env`, `Fail`, `RAM` visible only when state is STREAMING or DEGRADED.

Styled with `bg-black/50 backdrop-blur px-2 py-1.5 rounded border border-white/10 font-mono` matches existing HUD chips.

Position: `absolute top-3 right-16 lg:top-6 lg:right-28` offset from right to avoid overlapping the TILT controls.

## Files Changed

| File                                     | Change                                                |
| ---------------------------------------- | ----------------------------------------------------- |
| `ESP32/Camera_Marcelo_20fps_480x320.ino` | Add `publishCamStatus()`, call in 5s cycle            |
| `ESP32/Camera_Marcelo_5fps_640x480.ino`  | Same                                                  |
| `control.html`                           | Subscribe `rover/cam/status`, derive state, pass prop |
| `components/Dashboard/VideoHud.js`       | Add `CamStatusPanel` overlay, accept `camStatus` prop |

## Out of Scope

- Gimbal angle display (separate feature)
- Changes to `SystemStatus.js` (camera state lives in the video overlay only)
- Any changes to the video proxy or WebSocket server
