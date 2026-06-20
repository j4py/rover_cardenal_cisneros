const CAM_CONFIG = {
  OFFLINE:    { color: "text-red-500",    dot: "bg-red-700",    label: "OFFLINE",    animate: "" },
  CONNECTING: { color: "text-yellow-400", dot: "bg-yellow-500", label: "CONNECTING", animate: "animate-pulse" },
  STREAMING:  { color: "text-green-400",  dot: "bg-green-500",  label: "STREAMING",  animate: "" },
  DEGRADED:   { color: "text-yellow-400", dot: "bg-yellow-500", label: "DEGRADED",   animate: "animate-pulse" },
};

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

function VideoHud({ telemetry, camStatus, onGimbalMove }) {
   const [pitch, setPitch] = React.useState(90);
   const [yaw, setYaw] = React.useState(90);
   const isDraggingYawRef = React.useRef(false);
   const isDraggingPitchRef = React.useRef(false);
   const pitchIntervalRef = React.useRef(null);
   const yawIntervalRef = React.useRef(null);

   // Usar una referencia para onGimbalMove para evitar recrear handleGimbal y limpiar los intervalos en cada renderizado
   const onGimbalMoveRef = React.useRef(onGimbalMove);
   React.useEffect(() => {
      onGimbalMoveRef.current = onGimbalMove;
   }, [onGimbalMove]);

   // Simulate gimbal move (callback estable)
   const handleGimbal = React.useCallback((axis, value) => {
      if (axis === 'yaw') {
         if (value === 0) {
            setYaw(0);
            if (yawIntervalRef.current) clearInterval(yawIntervalRef.current);
            yawIntervalRef.current = setInterval(() => {
               if (onGimbalMoveRef.current) onGimbalMoveRef.current('yaw', -25); // Giro izquierda continuo (-25 pasos)
            }, 80);
         } else if (value === 180) {
            setYaw(180);
            if (yawIntervalRef.current) clearInterval(yawIntervalRef.current);
            yawIntervalRef.current = setInterval(() => {
               if (onGimbalMoveRef.current) onGimbalMoveRef.current('yaw', 25); // Giro derecha continuo (25 pasos)
            }, 80);
         } else if (value === 'release') {
            setYaw(90);
            if (yawIntervalRef.current) {
               clearInterval(yawIntervalRef.current);
               yawIntervalRef.current = null;
            }
         }
         return;
      }
      if (axis === 'pitch') {
         if (value === 'up') {
            setPitch(180); // Visual UP highlight
            if (pitchIntervalRef.current) clearInterval(pitchIntervalRef.current);
            pitchIntervalRef.current = setInterval(() => {
               if (onGimbalMoveRef.current) onGimbalMoveRef.current('pitch', 0.5); // Subir 0.5 grados continuo
            }, 80);
         } else if (value === 'down') {
            setPitch(0); // Visual DOWN highlight
            if (pitchIntervalRef.current) clearInterval(pitchIntervalRef.current);
            pitchIntervalRef.current = setInterval(() => {
               if (onGimbalMoveRef.current) onGimbalMoveRef.current('pitch', -0.5); // Bajar 0.5 grados continuo
            }, 80);
         } else if (value === 'center') {
            setPitch(90); // Visual CENTER highlight
            if (pitchIntervalRef.current) clearInterval(pitchIntervalRef.current);
            if (onGimbalMoveRef.current) onGimbalMoveRef.current('pitch', 'CENTER');
         } else if (value === 'release') {
            if (pitchIntervalRef.current) {
               clearInterval(pitchIntervalRef.current);
               pitchIntervalRef.current = null;
            }
            setPitch(null);
         }
      }
   }, []);

   // Efecto para los eventos globales de liberaciÃ³n
   React.useEffect(() => {
      const handleRelease = () => {
         if (isDraggingYawRef.current) {
            isDraggingYawRef.current = false;
            handleGimbal('yaw', 'release');
         }
         if (isDraggingPitchRef.current) {
            isDraggingPitchRef.current = false;
            handleGimbal('pitch', 'release');
         }
      };

      window.addEventListener('mouseup', handleRelease);
      window.addEventListener('touchend', handleRelease);
      window.addEventListener('touchcancel', handleRelease);
      window.addEventListener('pointerup', handleRelease);
      window.addEventListener('pointercancel', handleRelease);

      return () => {
         window.removeEventListener('mouseup', handleRelease);
         window.removeEventListener('touchend', handleRelease);
         window.removeEventListener('touchcancel', handleRelease);
         window.removeEventListener('pointerup', handleRelease);
         window.removeEventListener('pointercancel', handleRelease);
      };
   }, [handleGimbal]);

   // Limpieza estricta de intervalos Ãºnicamente al desmontar el componente
   React.useEffect(() => {
      return () => {
         if (pitchIntervalRef.current) clearInterval(pitchIntervalRef.current);
         if (yawIntervalRef.current) clearInterval(yawIntervalRef.current);
      };
   }, []);

   const [isConnected, setIsConnected] = React.useState(false);
   const [fps, setFps] = React.useState(0);
   const videoRef = React.useRef(null);
   const wsRef = React.useRef(null);
   const videoContainerRef = React.useRef(null);
   const [rotateScale, setRotateScale] = React.useState(1);

   React.useEffect(() => {
      const el = videoContainerRef.current;
      if (!el) return;
      const ro = new ResizeObserver(([entry]) => {
         const { width, height } = entry.contentRect;
         if (height > 0) setRotateScale(width / height);
      });
      ro.observe(el);
      return () => ro.disconnect();
   }, []);

   React.useEffect(() => {
      // Endpoint de vídeo: lo fija deploy.sh en mqtt.js (window.VIDEO_WS_URL); fallback a LAN.
      const WS_URL = window.VIDEO_WS_URL || `ws://${window.location.hostname}:9002`;

      let ws = null;
      let reconnectDelay = 1000; // Empieza en 1s, sube exponencialmente hasta 30s
      let reconnectTimer = null;
      let isMounted = true;

      const connect = () => {
         if (!isMounted) return;
         console.log(`[VIDEO] Conectando a ${WS_URL}... (prÃ³ximo reintento en ${reconnectDelay/1000}s si falla)`);
         ws = new WebSocket(WS_URL);
         wsRef.current = ws;
         ws.binaryType = 'arraybuffer';

         let frameCount = 0;
         let lastFpsUpdate = performance.now();

         ws.onopen = () => {
            console.log("[VIDEO] Conectado!");
            reconnectDelay = 1000; // Resetear backoff al conectar
            setIsConnected(true);
         };

         ws.onmessage = (event) => {
            frameCount++;
            const now = performance.now();
            if (now - lastFpsUpdate >= 1000) {
               const calculatedFps = Math.round((frameCount * 1000) / (now - lastFpsUpdate));
               setFps(calculatedFps);
               frameCount = 0;
               lastFpsUpdate = now;
            }

            if (videoRef.current) {
               const blob = new Blob([event.data], { type: "image/jpeg" });
               const url = URL.createObjectURL(blob);
               videoRef.current.onload = () => URL.revokeObjectURL(url);
               videoRef.current.src = url;
            }
         };

         ws.onerror = (e) => {
            console.error("[VIDEO] Error:", e);
            setIsConnected(false);
         };

         ws.onclose = () => {
            console.log(`[VIDEO] Desconectado. Reintentando en ${reconnectDelay/1000}s...`);
            setIsConnected(false);
            setFps(0);
            if (isMounted) {
               reconnectTimer = setTimeout(() => {
                  reconnectDelay = Math.min(reconnectDelay * 2, 30000); // Backoff exponencial, mÃ¡x 30s
                  connect();
               }, reconnectDelay);
            }
         };
      };

      connect();

      return () => {
         isMounted = false;
         clearTimeout(reconnectTimer);
         if (ws && ws.readyState === WebSocket.OPEN) {
            ws.close();
         }
      };
   }, []);

   const heading = telemetry.heading;

   // Helper for cardinal directions
   const getCardinal = (angle) => {
      const directions = ['Norte', 'Noreste', 'Este', 'Sureste', 'Sur', 'Suroeste', 'Oeste', 'Noroeste'];
      return directions[Math.round(angle / 45) % 8];
   };

   return (
      <div className="w-full h-full relative bg-black group overflow-hidden">
         {/* Video Stream from ESP32-CAM via WebSocket */}
         <div ref={videoContainerRef} className="absolute inset-0 flex items-center justify-center bg-slate-900">

            {/* Dynamic Image from WebSocket */}
            <img
               ref={videoRef}
               className="w-full h-full object-contain"
               alt="Rover Live Stream"
               style={{ display: isConnected ? 'block' : 'none', transform: `rotate(90deg) scale(${rotateScale * 0.75})` }}
            />

            {/* Placeholder - Se muestra cuando no hay conexiÃ³n */}
            <div className="absolute inset-0 flex items-center justify-center text-center opacity-30"
               style={{ display: !isConnected ? 'flex' : 'none' }}>
               <div>
                  <p className="font-mono text-sm">CONNECTING TO VIDEO SOCKET...</p>
                  <p className="text-xs mt-2 text-orange-500">WSS Stream</p>
               </div>
            </div>

            {/* Grid Overlay for aiming */}
            <div className="absolute inset-0 pointer-events-none opacity-20"
               style={{ backgroundImage: 'linear-gradient(rgba(255,255,255,0.1) 1px, transparent 1px), linear-gradient(90deg, rgba(255,255,255,0.1) 1px, transparent 1px)', backgroundSize: '100px 100px' }}>
            </div>
            {/* Tactical Crosshair */}
            <div className="absolute inset-0 pointer-events-none flex items-center justify-center">

               {/* Centering Arrows */}
               <div className="relative w-10 h-10 flex items-center justify-center">
                  {/* Up */}
                  <div className="absolute top-0 opacity-50">
                     <svg width="10" height="6" viewBox="0 0 10 6" fill="none"><path d="M1 1L5 5L9 1" stroke="#f97316" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" /></svg>
                  </div>
                  {/* Down */}
                  <div className="absolute bottom-0 opacity-50">
                     <svg width="10" height="6" viewBox="0 0 10 6" fill="none"><path d="M1 5L5 1L9 5" stroke="#f97316" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" /></svg>
                  </div>
                  {/* Left */}
                  <div className="absolute left-0 opacity-50">
                     <svg width="6" height="10" viewBox="0 0 6 10" fill="none"><path d="M1 1L5 5L1 9" stroke="#f97316" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" /></svg>
                  </div>
                  {/* Right */}
                  <div className="absolute right-0 opacity-50">
                     <svg width="6" height="10" viewBox="0 0 6 10" fill="none"><path d="M5 1L1 5L5 9" stroke="#f97316" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" /></svg>
                  </div>

                  {/* Tiny center reference dot */}
                  <div className="w-1 h-1 bg-orange-500 rounded-full animate-[pulse_0.5s_infinite] shadow-[0_0_5px_rgba(249,115,22,0.5)]"></div>
               </div>

               {/* Axis Markers */}
               <div className="absolute h-px w-10 bg-gradient-to-r from-transparent via-orange-500/30 to-transparent -translate-x-12"></div>
               <div className="absolute h-px w-10 bg-gradient-to-r from-transparent via-orange-500/30 to-transparent translate-x-12"></div>
               <div className="absolute w-px h-10 bg-gradient-to-b from-transparent via-orange-500/30 to-transparent -translate-y-12"></div>
               <div className="absolute w-px h-10 bg-gradient-to-b from-transparent via-orange-500/30 to-transparent translate-y-12"></div>
            </div>
         </div>

         {/* HUD Telemetry Overlay */}
         <div className="absolute inset-0 pointer-events-none p-3 lg:p-6 flex flex-col justify-between">
            {/* Top Data Bar */}
            <div className="flex justify-between items-start">
               <div className="flex flex-col lg:flex-row gap-1 lg:gap-4">
                  <div className="bg-black/50 backdrop-blur px-2 lg:px-3 py-1 rounded border border-white/10 text-[10px] lg:text-xs font-mono text-orange-400 w-fit">
                     BAT: {telemetry.battery.toFixed(1)}V
                  </div>
                  <div className="bg-black/50 backdrop-blur px-2 lg:px-3 py-1 rounded border border-white/10 text-[10px] lg:text-xs font-mono text-blue-400 w-fit">
                     Rumbo: {heading}° - {getCardinal(heading)}
                  </div>
               </div>
               <div className="mr-20 lg:mr-24">
                  <CamStatusPanel camStatus={camStatus} />
               </div>
            </div>

            {/* Bottom Info - Repositioned for mobile to avoid Yaw slider */}
            <div className="flex justify-between items-end pb-20 lg:pb-0">
               <div className="text-[10px] lg:text-xs font-mono text-slate-400 bg-black/50 px-2 py-1 rounded border border-white/5">
                  FPS: {fps} <br />
                  LAT: {telemetry.lat.toFixed(4)} <br />
                  LON: {telemetry.lon.toFixed(4)}
               </div>
            </div>
         </div>

         {/* Pan Controls (Horizontal - Bottom) */}
         <div className="absolute bottom-6 inset-x-0 flex flex-col justify-center items-center gap-2 pointer-events-auto z-50">
            <div className="flex items-center bg-black/40 px-6 py-3 rounded-xl backdrop-blur-sm border border-white/10 gap-4">
               <button
                  onPointerDown={(e) => { e.preventDefault(); isDraggingYawRef.current = true; handleGimbal('yaw', 0); }}
                  className={`w-10 h-10 flex items-center justify-center rounded-lg font-mono font-bold transition-all text-sm select-none border border-white/5 active:scale-95 ${yaw === 0 ? 'bg-blue-600 text-white shadow-[0_0_15px_rgba(37,99,235,0.6)] border-blue-400' : 'bg-slate-900/80 hover:bg-slate-800 text-slate-300'}`}
               >
                  L
               </button>

               <div className="flex flex-col items-center min-w-[80px]">
                  <span className="text-[10px] text-slate-300 font-mono font-bold tracking-widest">PAN</span>
                  <span className="text-[10px] font-mono text-blue-400 mt-0.5">{yaw === 90 ? 'PARADO' : (yaw === 0 ? 'â—€ IZQ' : 'DER â–¶')}</span>
               </div>

               <button
                  onPointerDown={(e) => { e.preventDefault(); isDraggingYawRef.current = true; handleGimbal('yaw', 180); }}
                  className={`w-10 h-10 flex items-center justify-center rounded-lg font-mono font-bold transition-all text-sm select-none border border-white/5 active:scale-95 ${yaw === 180 ? 'bg-blue-600 text-white shadow-[0_0_15px_rgba(37,99,235,0.6)] border-blue-400' : 'bg-slate-900/80 hover:bg-slate-800 text-slate-300'}`}
               >
                  R
               </button>
            </div>
         </div>

         {/* Tilt Controls (Vertical - Right) */}
         <div className="absolute right-4 inset-y-0 flex flex-col justify-center items-center gap-4 pointer-events-auto z-50">
            <div className="flex flex-col items-center gap-2 px-3 py-4 bg-black/40 rounded-xl backdrop-blur-sm border border-white/10 min-w-[80px]">
               <span className="text-[10px] text-slate-300 font-mono font-bold tracking-widest mb-2">TILT</span>

               <button
                  onPointerDown={(e) => { e.preventDefault(); isDraggingPitchRef.current = true; handleGimbal('pitch', 'up'); }}
                  className={`w-14 py-2 rounded-lg font-mono text-[10px] font-bold transition-all border border-white/5 active:scale-95 text-center ${pitch === 180 ? 'bg-orange-600 text-white shadow-[0_0_15px_rgba(249,115,22,0.6)] border-orange-400' : 'bg-slate-900/80 hover:bg-slate-800 text-slate-300'}`}
               >
                  UP
               </button>

               <button
                  onClick={() => handleGimbal('pitch', 'center')}
                  className={`w-14 py-2 rounded-lg font-mono text-[10px] font-bold transition-all border border-white/5 active:scale-95 text-center ${pitch === 90 ? 'bg-orange-600 text-white shadow-[0_0_15px_rgba(249,115,22,0.6)] border-orange-400' : 'bg-slate-900/80 hover:bg-slate-800 text-slate-300'}`}
               >
                  CENTER
               </button>

               <button
                  onPointerDown={(e) => { e.preventDefault(); isDraggingPitchRef.current = true; handleGimbal('pitch', 'down'); }}
                  className={`w-14 py-2 rounded-lg font-mono text-[10px] font-bold transition-all border border-white/5 active:scale-95 text-center ${pitch === 0 ? 'bg-orange-600 text-white shadow-[0_0_15px_rgba(249,115,22,0.6)] border-orange-400' : 'bg-slate-900/80 hover:bg-slate-800 text-slate-300'}`}
               >
                  DOWN
               </button>
            </div>
         </div>
      </div>
   );
}





