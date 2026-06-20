const GPS_CONFIG = {
  OFF:       { color: "text-red-500",    dot: "bg-red-700",    label: "OFFLINE",   animate: "" },
  SEARCHING: { color: "text-yellow-400", dot: "bg-yellow-500", label: "SEARCHING", animate: "animate-pulse" },
  FIX:       { color: "text-green-400",  dot: "bg-green-500",  label: "FIX",       animate: "" },
};

const GpsPanel = ({ gps_state, gps_sats }) => {
  const state = GPS_CONFIG[gps_state] || GPS_CONFIG.OFF;
  const sats = gps_sats || 0;
  const MAX_SATS = 12;
  const isOff = !gps_state || gps_state === "OFF";

  return (
    <div className="bg-slate-800/50 px-3 py-2.5 rounded-lg border border-slate-700/50">
      <div className="flex items-center justify-between mb-2">
        <span className="text-xs text-slate-400 font-semibold">GPS</span>
        <div className={"flex items-center gap-1.5 " + state.animate}>
          <div className={"w-2.5 h-2.5 rounded-full " + state.dot}></div>
          <span className={"text-xs font-bold font-mono " + state.color}>{state.label}</span>
        </div>
      </div>
      <div className="flex items-center gap-2">
        <div className="flex gap-0.5 flex-1">
          {Array.from({ length: MAX_SATS }, (_, i) => (
            <div
              key={i}
              className={"h-2 flex-1 rounded-sm transition-all duration-500 " + (!isOff && i < sats ? "bg-green-500" : "bg-slate-700")}
            ></div>
          ))}
        </div>
        <span className={"text-xs font-mono w-10 text-right " + (isOff ? "text-slate-600" : "text-slate-400")}>
          {isOff ? "-- SATs" : sats + " SATs"}
        </span>
      </div>
    </div>
  );
};

function SystemStatus({ mqttStatus, wifiStatus, gpsStatus, battery, batteryPct, mode, autoMode, onEmergency, onModeToggle, ip, ip_gps, ip_cam, gps_state, gps_sats }) {
  
  const StatusLed = ({ label, active, color = "bg-green-600" }) => (
    <div className="flex items-center justify-between bg-slate-800/50 px-3 py-2 rounded-lg border border-slate-700/50">
        <span className="text-xs text-slate-400 font-semibold">{label}</span>
        <div className={"w-3 h-3 rounded-full shadow-lg transition-all duration-300 " + (active ? color + " shadow-" + color + "/50" : "bg-red-700/70")}></div>
    </div>
  );

  return (
    <div className="flex flex-col gap-4">
      
      {/* Top Status Bar */}
      <div className="glass-panel flex flex-col gap-2">
         <div className="grid grid-cols-2 gap-2">
            <StatusLed label="MQTT" active={mqttStatus} color="bg-green-600" />
            <StatusLed label="WIFI" active={wifiStatus} color="bg-green-600" />
         </div>
         <GpsPanel gps_state={gps_state} gps_sats={gps_sats} />
         {wifiStatus && (ip || ip_gps || ip_cam) && (
            <div className="text-center text-[10px] font-mono text-slate-400 bg-slate-950/40 py-1.5 rounded border border-slate-800/60 flex flex-col gap-0.5">
              {ip && <div>IP Rover: <span className="text-emerald-400 font-bold">{ip}</span></div>}
              {ip_gps && <div>IP GPS: <span className="text-cyan-400 font-bold">{ip_gps}</span></div>}
              {ip_cam && <div>IP Cam: <span className="text-violet-400 font-bold">{ip_cam}</span></div>}
            </div>
         )}
      </div>

      {/* Battery Panel */}
      <div className="glass-panel flex flex-col gap-2 p-3">
         <div className="flex justify-between items-end">
            <span className="text-xs text-slate-400 font-bold tracking-wider">PWR SYSTEM</span>
            <div className="flex items-baseline gap-2">
               <span className={"text-xs font-mono font-bold " + (battery < 11.0 ? "text-red-500 animate-pulse" : "text-slate-400")}>
                  {battery.toFixed(2)}V
               </span>
               <span className={"text-lg font-black tracking-tight " + (batteryPct < 20 ? "text-red-500" : batteryPct < 50 ? "text-yellow-400" : "text-green-400")}>
                  {batteryPct}%
               </span>
            </div>
         </div>
         <div className="h-2 w-full bg-slate-900 rounded-full overflow-hidden border border-slate-700/50 shadow-inner">
            <div 
               className={"h-full transition-all duration-1000 relative overflow-hidden " + (
                  batteryPct > 50 ? "bg-gradient-to-r from-green-600 to-green-400 shadow-[0_0_10px_rgba(74,222,128,0.5)]" : 
                  batteryPct > 20 ? "bg-gradient-to-r from-yellow-600 to-yellow-400 shadow-[0_0_10px_rgba(250,204,21,0.5)]" : 
                  "bg-gradient-to-r from-red-600 to-red-400 shadow-[0_0_10px_rgba(248,113,113,0.8)]"
               )}
               style={{ width: Math.max(0, Math.min(100, batteryPct)) + "%" }}
            >
               <div className="absolute inset-0 bg-white/20 w-full h-full" style={{ animation: "shimmer 2s infinite" }}></div>
            </div>
         </div>
      </div>

      {/* Mode Switch & E-STOP */}
      <div className="glass-panel flex flex-col gap-4 p-4">
          
          {/* Mode Switcher */}
          <div className="flex bg-slate-800 p-1 rounded-lg">
             <button 
                onClick={() => onModeToggle && onModeToggle("MANUAL")}
                className={"flex-1 py-2 rounded-md text-xs font-bold transition-all " + (!autoMode ? "bg-blue-600 text-white shadow-lg" : "text-slate-500 hover:text-slate-300")}
             >
                MANUAL
             </button>
             <button 
                 onClick={() => onModeToggle && onModeToggle("AUTO")}
                className={"flex-1 py-2 rounded-md text-xs font-bold transition-all " + (autoMode ? "bg-purple-600 text-white shadow-lg" : "text-slate-500 hover:text-slate-300")}
             >
                AUTONOMOUS
             </button>
          </div>

          {/* EMERGENCY STOP */}
          <button 
             onClick={onEmergency}
             className="w-full py-6 bg-gradient-to-br from-red-600 to-red-800 hover:from-red-500 hover:to-red-700 active:scale-95 transition-all rounded-xl border-2 border-red-500 shadow-[0_0_30px_rgba(220,38,38,0.4)] flex flex-col items-center justify-center group"
          >
             <span className="text-2xl font-black text-white tracking-[0.2em] mb-1 group-hover:text-red-100">E-STOP</span>
             <span className="text-[10px] text-red-200 uppercase font-semibold opacity-70">Emergency Cutoff</span>
          </button>

      </div>
    </div>
  );
}