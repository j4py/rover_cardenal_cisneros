# Camera Telemetry Overlay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a camera status overlay to VideoHud that shows ESP32-CAM health (RSSI, RAM, frames, state) in real time via MQTT, analogous to the GPS panel in SystemStatus.

**Architecture:** Both camera firmwares publish diagnostics to `rover/cam/status` every 5s. `control.html` subscribes, derives a state (OFFLINE/CONNECTING/STREAMING/DEGRADED), and passes it as a `camStatus` prop to `VideoHud`. `VideoHud` renders a `CamStatusPanel` sub-component in the top-right corner of the overlay.

**Tech Stack:** Arduino C++ (PubSubClient already present), React 18 (UMD, no bundler), Tailwind CSS (CDN), Paho MQTT (already in control.html)

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `ESP32/Camera_Marcelo_20fps_480x320.ino` | Modify | Add `publishCamStatus()`, call in 5s cycle |
| `ESP32/Camera_Marcelo_5fps_640x480.ino` | Modify | Same — different `mode` string |
| `control.html` | Modify | Subscribe `rover/cam/status`, derive state, pass `camStatus` prop |
| `components/Dashboard/VideoHud.js` | Modify | Add `CAM_CONFIG`, `CamStatusPanel` component, wire prop |

---

## Task 1: Firmware 20fps — publish cam status via MQTT

**Files:**
- Modify: `ESP32/Camera_Marcelo_20fps_480x320.ino`

- [ ] **Step 1: Add `publishCamStatus()` function after `publishIp()`**

Open `Camera_Marcelo_20fps_480x320.ino`. After the closing brace of `publishIp()` (line 77), add:

```cpp
void publishCamStatus() {
    if (!mqttClient.connected()) return;
    const char* state = webSocket.isConnected() ? "STREAMING" : "CONNECTING";
    char payload[200];
    snprintf(payload, sizeof(payload),
        "{\"state\":\"%s\",\"mode\":\"20fps-hvga\",\"rssi\":%d,\"ram_kb\":%u,\"frames_sent\":%lu,\"frames_failed\":%lu,\"ws\":%s}",
        state,
        WiFi.RSSI(),
        ESP.getFreeHeap() / 1024,
        framesSent,
        framesFailed,
        webSocket.isConnected() ? "true" : "false"
    );
    mqttClient.publish("rover/cam/status", payload);
}
```

- [ ] **Step 2: Call `publishCamStatus()` in the 5s diagnostic cycle**

In `loop()`, find the `if (now - lastStatusPrint >= 5000)` block (currently lines 230-238). Add the call after the existing `Serial.printf`:

```cpp
if (now - lastStatusPrint >= 5000) {
    lastStatusPrint = now;
    Serial.printf("[DIAG] Env:%lu Fail:%lu RSSI:%d dBm RAM:%uKB Red:%s WS:%s MQTT:%s\n",
                  framesSent, framesFailed,
                  WiFi.RSSI(), ESP.getFreeHeap() / 1024,
                  WiFi.SSID().c_str(),
                  webSocket.isConnected() ? "OK" : "OFF",
                  mqttClient.connected() ? "OK" : "OFF");
    publishCamStatus();  // <-- ADD THIS LINE
}
```

- [ ] **Step 3: Verify it compiles**

In Arduino IDE: Sketch > Verify/Compile (Ctrl+R).
Expected: "Done compiling." with no errors.

- [ ] **Step 4: Flash via OTA and verify via Serial Monitor**

Flash OTA: Tools > Port > cam-20fps-hvga.local, then Upload.
Open Serial Monitor (115200 baud). After ~5s you should see the existing DIAG line plus MQTT activity:

```
[DIAG] Env:42 Fail:0 RSSI:-61 dBm RAM:145KB Red:sk1ll24 WS:OK MQTT:OK
[MQTT] ... (publish to rover/cam/status)
```

- [ ] **Step 5: Verify MQTT payload with an MQTT client**

Using MQTT Explorer or `mosquitto_sub` on any machine:
```
mosquitto_sub -h direct.example.com -p 1883 -u oliver -P •••••••• -t rover/cam/status
```
Expected output every 5s:
```json
{"state":"STREAMING","mode":"20fps-hvga","rssi":-61,"ram_kb":145,"frames_sent":42,"frames_failed":0,"ws":true}
```

---

## Task 2: Firmware 5fps — publish cam status via MQTT

**Files:**
- Modify: `ESP32/Camera_Marcelo_5fps_640x480.ino`

- [ ] **Step 1: Add `publishCamStatus()` function after `publishIp()`**

Open `Camera_Marcelo_5fps_640x480.ino`. After the closing brace of `publishIp()` (line 77), add:

```cpp
void publishCamStatus() {
    if (!mqttClient.connected()) return;
    const char* state = webSocket.isConnected() ? "STREAMING" : "CONNECTING";
    char payload[200];
    snprintf(payload, sizeof(payload),
        "{\"state\":\"%s\",\"mode\":\"5fps-vga\",\"rssi\":%d,\"ram_kb\":%u,\"frames_sent\":%lu,\"frames_failed\":%lu,\"ws\":%s}",
        state,
        WiFi.RSSI(),
        ESP.getFreeHeap() / 1024,
        framesSent,
        framesFailed,
        webSocket.isConnected() ? "true" : "false"
    );
    mqttClient.publish("rover/cam/status", payload);
}
```

Note: the only difference from Task 1 is `"5fps-vga"` in the mode field.

- [ ] **Step 2: Call `publishCamStatus()` in the 5s diagnostic cycle**

Find the `if (now - lastStatusPrint >= 5000)` block (lines 230-238) and add the call:

```cpp
if (now - lastStatusPrint >= 5000) {
    lastStatusPrint = now;
    Serial.printf("[DIAG] Env:%lu Fail:%lu RSSI:%d dBm RAM:%uKB Red:%s WS:%s MQTT:%s\n",
                  framesSent, framesFailed,
                  WiFi.RSSI(), ESP.getFreeHeap() / 1024,
                  WiFi.SSID().c_str(),
                  webSocket.isConnected() ? "OK" : "OFF",
                  mqttClient.connected() ? "OK" : "OFF");
    publishCamStatus();  // <-- ADD THIS LINE
}
```

- [ ] **Step 3: Verify it compiles**

Sketch > Verify/Compile (Ctrl+R).
Expected: "Done compiling." with no errors.

- [ ] **Step 4: Flash via OTA**

Tools > Port > cam-5fps-vga.local, then Upload.

- [ ] **Step 5: Verify MQTT payload**

```
mosquitto_sub -h direct.example.com -p 1883 -u oliver -P •••••••• -t rover/cam/status
```
Expected (mode field should be `5fps-vga`):
```json
{"state":"STREAMING","mode":"5fps-vga","rssi":-61,"ram_kb":145,"frames_sent":12,"frames_failed":0,"ws":true}
```

---

## Task 3: control.html — subscribe to cam status and derive state

**Files:**
- Modify: `control.html` (the `useMqttSystem` hook, lines 67-215)

- [ ] **Step 1: Add `camStatus` state and offline timer ref**

Inside `useMqttSystem()`, after the existing `useRef` declarations (after line 86 `const gpsTimerRef = useRef(null);`), add:

```js
const [camStatus, setCamStatus] = React.useState({
  state: "OFFLINE", mode: null, rssi: null,
  ram_kb: null, frames_sent: 0, frames_failed: 0, ws: false
});
const camOfflineTimerRef = React.useRef(null);
```

- [ ] **Step 2: Subscribe to `rover/cam/status` in `onConnect`**

In the `onConnect` function (around line 110), add the subscription:

```js
const onConnect = () => {
  console.log("[MQTT] Connected!");
  setMqttStatus(true);
  client.subscribe("rover/status");
  client.subscribe("rover/telemetry");
  client.subscribe("rover/mode");
  client.subscribe("rover/cam/status");  // <-- ADD THIS LINE
};
```

- [ ] **Step 3: Handle `rover/cam/status` messages**

In `onMessageArrived`, after the `else if (topic === "rover/mode")` block (around line 156), add:

```js
else if (topic === "rover/cam/status" && isJson) {
  if (camOfflineTimerRef.current) clearTimeout(camOfflineTimerRef.current);

  const { ws, rssi, frames_sent, frames_failed } = payload;
  let state = "CONNECTING";
  if (ws) {
    const failRatio = frames_sent > 0 ? frames_failed / frames_sent : 0;
    state = (rssi < -80 || failRatio > 0.05) ? "DEGRADED" : "STREAMING";
  }

  setCamStatus({ ...payload, state });

  camOfflineTimerRef.current = setTimeout(() => {
    setCamStatus(prev => ({ ...prev, state: "OFFLINE" }));
  }, 10000);
}
```

- [ ] **Step 4: Clean up the offline timer on unmount**

In the `useEffect` cleanup return (around line 186), add the timer cleanup:

```js
return () => {
  if (camOfflineTimerRef.current) clearTimeout(camOfflineTimerRef.current);
  if (client.isConnected()) {
    console.log("[MQTT] Disconnecting...");
    client.disconnect();
  }
};
```

- [ ] **Step 5: Return `camStatus` from the hook**

Change the return statement of `useMqttSystem` (line 214):

```js
return { mqttStatus, wifiStatus, gpsStatus, telemetry, camStatus, sendCommand, commandMode };
```

- [ ] **Step 6: Pass `camStatus` prop to `VideoHud`**

In the `App` component, destructure `camStatus` and pass it to `<VideoHud>`:

```js
function App() {
  const { mqttStatus, wifiStatus, gpsStatus, telemetry, camStatus, sendCommand, commandMode } = useMqttSystem();
  // ...
  return (
    // ...
    <VideoHud
      telemetry={telemetry}
      camStatus={camStatus}
      onGimbalMove={(axis, val) => sendCommand(`rover/gimbal/${axis}`, val)}
    />
```

- [ ] **Step 7: Verify in browser console**

Open `control.html` in the browser. Open DevTools > Console. Within 10s of the cam firmware publishing, you should see no errors. In the Console, type:
```js
// No direct access, but you can verify by checking the React component tree
// via React DevTools or checking for console errors
```
The page should load without errors. The cam panel will show OFFLINE until Task 4 is done.

---

## Task 4: VideoHud.js — add CamStatusPanel overlay

**Files:**
- Modify: `components/Dashboard/VideoHud.js`

- [ ] **Step 1: Add `CAM_CONFIG` constant at the top of the file**

Before the `function VideoHud(` declaration (line 1), add:

```js
const CAM_CONFIG = {
  OFFLINE:    { color: "text-red-500",    dot: "bg-red-700",    label: "OFFLINE",    animate: "" },
  CONNECTING: { color: "text-yellow-400", dot: "bg-yellow-500", label: "CONNECTING", animate: "animate-pulse" },
  STREAMING:  { color: "text-green-400",  dot: "bg-green-500",  label: "STREAMING",  animate: "" },
  DEGRADED:   { color: "text-yellow-400", dot: "bg-yellow-500", label: "DEGRADED",   animate: "animate-pulse" },
};
```

- [ ] **Step 2: Add `CamStatusPanel` component after `CAM_CONFIG`**

```js
function CamStatusPanel({ camStatus }) {
  const { state = "OFFLINE", mode, rssi, ram_kb, frames_sent, frames_failed } = camStatus || {};
  const cfg = CAM_CONFIG[state] || CAM_CONFIG.OFFLINE;
  const isOffline = state === "OFFLINE";

  const RSSI_SEGMENTS = 10;
  const rssiPct = rssi != null
    ? Math.round(Math.max(0, Math.min(RSSI_SEGMENTS, (rssi + 90) / 50 * RSSI_SEGMENTS)))
    : 0;

  return (
    <div className="bg-black/50 backdrop-blur px-2 py-1.5 rounded border border-white/10 text-[10px] lg:text-xs font-mono flex flex-col gap-1 min-w-[130px]">
      <div className={"flex items-center justify-between gap-2 " + cfg.animate}>
        <div className="flex items-center gap-1.5">
          <div className={"w-2 h-2 rounded-full shrink-0 " + cfg.dot}></div>
          <span className={"font-bold " + cfg.color}>{cfg.label}</span>
        </div>
        {mode && <span className="text-slate-500 text-[9px]">{mode}</span>}
      </div>

      {!isOffline && rssi != null && (
        <div className="flex items-center gap-1.5">
          <div className="flex gap-0.5 flex-1">
            {Array.from({ length: RSSI_SEGMENTS }, (_, i) => (
              <div
                key={i}
                className={"h-1.5 flex-1 rounded-sm " + (i < rssiPct ? "bg-green-500" : "bg-slate-700")}
              />
            ))}
          </div>
          <span className="text-slate-400 w-16 text-right shrink-0">{rssi} dBm</span>
        </div>
      )}

      {!isOffline && (
        <div className="text-slate-500 leading-tight">
          Env:{frames_sent} Fail:{frames_failed} RAM:{ram_kb}KB
        </div>
      )}
    </div>
  );
}
```

- [ ] **Step 3: Accept `camStatus` prop in `VideoHud`**

Change the function signature (line 1 of the original file, which is now further down):

```js
function VideoHud({ telemetry, camStatus, onGimbalMove }) {
```

- [ ] **Step 4: Add `CamStatusPanel` to the top data bar**

Find the `{/* Top Data Bar */}` section (around line 249 in the original, now shifted). The current structure is:

```jsx
<div className="flex justify-between items-start">
  <div className="flex flex-col lg:flex-row gap-1 lg:gap-4">
    <div className="bg-black/50 ...">BAT: ...</div>
    <div className="bg-black/50 ...">Rumbo: ...</div>
  </div>
</div>
```

Change it to add `CamStatusPanel` on the right, with right margin to avoid overlapping the TILT controls:

```jsx
<div className="flex justify-between items-start">
  <div className="flex flex-col lg:flex-row gap-1 lg:gap-4">
    <div className="bg-black/50 backdrop-blur px-2 lg:px-3 py-1 rounded border border-white/10 text-[10px] lg:text-xs font-mono text-orange-400 w-fit">
      BAT: {telemetry.battery.toFixed(1)}V
    </div>
    <div className="bg-black/50 backdrop-blur px-2 lg:px-3 py-1 rounded border border-white/10 text-[10px] lg:text-xs font-mono text-blue-400 w-fit">
      Rumbo: {telemetry.heading}° - {getCardinal(telemetry.heading)}
    </div>
  </div>
  <div className="mr-20 lg:mr-24">
    <CamStatusPanel camStatus={camStatus} />
  </div>
</div>
```

The `mr-20 lg:mr-24` offset prevents overlap with the TILT control buttons (`absolute right-4`, ~80px wide).

- [ ] **Step 5: Open control.html in the browser and verify the panel renders**

Navigate to `control.html`. The panel should appear top-right of the video area showing `OFFLINE` in red.

- [ ] **Step 6: Verify STREAMING state**

With the cam firmware running and publishing to MQTT, within 10s the panel should update to show:
- Green dot + `STREAMING` label
- Mode badge (`20fps-hvga` or `5fps-vga`)
- RSSI bar with dBm value
- Counters row: `Env:N Fail:N RAM:NKB`

- [ ] **Step 7: Verify DEGRADED state**

Temporarily lower the RSSI threshold in `control.html` to test: change `rssi < -80` to `rssi < -40`. Reload — if RSSI is between -40 and -80, you should see yellow `DEGRADED`. Revert after verifying.

- [ ] **Step 8: Verify OFFLINE state**

Disconnect the cam firmware from WiFi (or stop publishing). After 10 seconds the panel should revert to red `OFFLINE`.

---

## Self-Review Notes

- **Divide-by-zero guard:** `frames_sent > 0` check in Task 3 Step 3 prevents NaN on fresh boot.
- **Prop default:** `camStatus || {}` in `CamStatusPanel` handles the brief moment before state initializes.
- **Timer leak:** camOfflineTimerRef cleanup added in Task 3 Step 4.
- **TILT overlap:** `mr-20 lg:mr-24` margin in Task 4 Step 4 keeps panel clear of right-side buttons.
- **No new libraries:** firmware uses existing `PubSubClient`; frontend uses existing Paho MQTT subscription.
