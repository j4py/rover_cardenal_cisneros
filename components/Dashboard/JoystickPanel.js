function JoystickPanel({ onMove }) {
  const [throttle, setThrottle] = React.useState(0); // Y: -1 to 1 (Acelerador)
  const [steering, setSteering] = React.useState(0); // X: -1 to 1 (Dirección)
  const [draggingSteering, setDraggingSteering] = React.useState(false);
  const [draggingThrottle, setDraggingThrottle] = React.useState(false);
  const [draggingJoy, setDraggingJoy] = React.useState(false);
  const [spinning, setSpinning] = React.useState(null); // null | "left" | "right"
  const [throttleAutoReturn, setThrottleAutoReturn] = React.useState(true);
  const [steeringAutoReturn, setSteeringAutoReturn] = React.useState(true);

  const throttleTrackRef = React.useRef(null);
  const steeringTrackRef = React.useRef(null);
  const joyContainerRef = React.useRef(null);

  // Configuración
  const maxRadius = 50; // Radio máximo de desplazamiento del joystick central

  // Enviar comando unificado
  const updateMove = (sVal, tVal) => {
    if (onMove) {
      onMove({ x: sVal, y: tVal });
    }
  };

  // Alternar el modo del acelerador (si se activa Muelle, reinicia a cero)
  const toggleThrottleMode = () => {
    setThrottleAutoReturn((prev) => {
      const nextVal = !prev;
      if (nextVal && throttle !== 0) {
        setThrottle(0);
        updateMove(steering, 0);
      }
      return nextVal;
    });
  };

  // Alternar el modo de dirección (si se activa Muelle, reinicia a cero)
  const toggleSteeringMode = () => {
    setSteeringAutoReturn((prev) => {
      const nextVal = !prev;
      if (nextVal && steering !== 0) {
        setSteering(0);
        updateMove(0, throttle);
      }
      return nextVal;
    });
  };

  // --- LÓGICA ACELERADOR (DESLIZADOR VERTICAL) ---
  const handleThrottleStart = (e) => {
    e.preventDefault();
    setDraggingThrottle(true);
    e.target.setPointerCapture(e.pointerId);
    handleThrottleMove(e, true);
  };

  const handleThrottleMove = (e, force = false) => {
    if (!draggingThrottle && !force) return;
    if (!throttleTrackRef.current) return;
    e.preventDefault();
    
    const rect = throttleTrackRef.current.getBoundingClientRect();
    const relativeY = e.clientY - rect.top;
    
    // Mapeamos: Arriba = +1.00 (Adelante), Abajo = -1.00 (Atrás)
    let val = 1 - (relativeY / rect.height) * 2;
    val = Math.max(-1, Math.min(1, val));
    
    // Zona muerta central de seguridad (snap a 0)
    if (Math.abs(val) < 0.12) {
      val = 0;
    }
    
    setThrottle(val);
    updateMove(steering, val);
  };

  const handleThrottleEnd = (e) => {
    setDraggingThrottle(false);
    if (throttleAutoReturn) {
      setThrottle(0);
      updateMove(steering, 0);
    }
    e.target.releasePointerCapture(e.pointerId);
  };

  // --- LÓGICA DIRECCIÓN (DESLIZADOR HORIZONTAL) ---
  const handleSteeringStart = (e) => {
    e.preventDefault();
    setDraggingSteering(true);
    e.target.setPointerCapture(e.pointerId);
    handleSteeringMove(e, true);
  };

  const handleSteeringMove = (e, force = false) => {
    if (!draggingSteering && !force) return;
    if (!steeringTrackRef.current) return;
    e.preventDefault();
    
    const rect = steeringTrackRef.current.getBoundingClientRect();
    const relativeX = e.clientX - rect.left;
    
    // Mapeamos: Izquierda = -1.00 (IZQ), Derecha = +1.00 (DER)
    let val = (relativeX / rect.width) * 2 - 1;
    val = Math.max(-1, Math.min(1, val));
    
    // Zona muerta pequeña para centrado perfecto
    if (Math.abs(val) < 0.08) {
      val = 0;
    }
    
    setSteering(val);
    updateMove(val, throttle);
  };

  const handleSteeringEnd = (e) => {
    setDraggingSteering(false);
    if (steeringAutoReturn) {
      setSteering(0); // Retorno automático al centro
      updateMove(0, throttle);
    }
    e.target.releasePointerCapture(e.pointerId);
  };

  // --- LÓGICA JOYSTICK CENTRAL (PS2 STYLE) ---
  const handleJoyStart = (e) => {
    e.preventDefault();
    setDraggingJoy(true);
    e.target.setPointerCapture(e.pointerId);
    handleJoyMove(e, true);
  };

  const handleJoyMove = (e, force = false) => {
    if (!draggingJoy && !force) return;
    if (!joyContainerRef.current) return;
    e.preventDefault();

    const rect = joyContainerRef.current.getBoundingClientRect();
    const centerX = rect.left + rect.width / 2;
    const centerY = rect.top + rect.height / 2;

    let deltaX = e.clientX - centerX;
    let deltaY = e.clientY - centerY;

    // Calcular distancia y limitar al radio máximo
    const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
    if (distance > maxRadius) {
      deltaX = (deltaX / distance) * maxRadius;
      deltaY = (deltaY / distance) * maxRadius;
    }

    // Convertir a valores normalizados -1.00 a 1.00
    let sVal = deltaX / maxRadius;
    let tVal = -deltaY / maxRadius; // Invertir eje Y para que arriba sea positivo

    // Aplicar zonas muertas de seguridad
    if (Math.abs(sVal) < 0.08) sVal = 0;
    if (Math.abs(tVal) < 0.12) tVal = 0;

    setSteering(sVal);
    setThrottle(tVal);
    updateMove(sVal, tVal);
  };

  const handleJoyEnd = (e) => {
    setDraggingJoy(false);
    
    let finalSteering = steering;
    let finalThrottle = throttle;

    if (steeringAutoReturn) {
      finalSteering = 0;
    }
    if (throttleAutoReturn) {
      finalThrottle = 0;
    }

    setSteering(finalSteering);
    setThrottle(finalThrottle);
    updateMove(finalSteering, finalThrottle);

    e.target.releasePointerCapture(e.pointerId);
  };

  const handleSpinStart = (direction) => {
    setSpinning(direction);
    setSteering(0);
    setThrottle(0);
    if (onMove) onMove({ x: direction === "right" ? 1 : -1, y: 0, spin: true });
  };

  const handleSpinEnd = () => {
    setSpinning(null);
    if (onMove) onMove({ x: 0, y: 0, spin: false });
  };

  return (
    <div className="flex items-center justify-around w-full h-full px-4 select-none touch-none">
       
       {/* 1. SECCIÓN DIRECCIÓN (STEERING - IZQUIERDA) */}
       <div className="flex items-center gap-2">
          {/* Selector de modo Dirección */}
          <div className="flex flex-col justify-center h-28 text-[9px] font-mono text-slate-500 py-1 pr-1">
             <span className="text-slate-500 font-bold tracking-wide text-center mb-1.5 uppercase">Modo</span>
             <button 
                onClick={toggleSteeringMode}
                className={`px-2 py-2 flex flex-col items-center justify-center gap-1 active:scale-95 border rounded-xl transition-all duration-200 font-mono text-[9px] font-bold shadow-lg w-14 ${
                   steeringAutoReturn 
                      ? 'bg-blue-500/10 border-blue-500/30 text-blue-400 hover:bg-blue-500/20' 
                      : 'bg-amber-500/10 border-amber-500/30 text-amber-400 hover:bg-amber-500/20 shadow-amber-950/20'
                }`}
                title={steeringAutoReturn ? "Modo Muelle: retorna al centro al soltar" : "Modo Fijo: mantiene la dirección actual"}
             >
                {steeringAutoReturn ? (
                   <>
                      <svg className="w-4 h-4 text-blue-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                         <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2.5" d="M3 10h10a8 8 0 018 8v2M3 10l6 6m-6-6l6-6" />
                      </svg>
                      <span className="text-[8px] tracking-tight">MUELLE</span>
                   </>
                ) : (
                   <>
                      <svg className="w-4 h-4 text-amber-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                         <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2.5" d="M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z" />
                      </svg>
                      <span className="text-[8px] tracking-tight">FIJO</span>
                   </>
                )}
             </button>
          </div>

          {/* El slider de Dirección */}
          <div className="flex flex-col items-center gap-1">
             <span className="text-[9px] font-mono uppercase text-slate-500 font-bold tracking-wider mb-2">Dirección</span>
             
             <div className="flex items-center gap-2">
                <span className="text-[9px] font-mono text-slate-500 font-bold">◀</span>
                
                <div 
                   ref={steeringTrackRef}
                   className={`w-28 h-7 bg-slate-950/60 rounded-full border relative flex items-center cursor-ew-resize shadow-inner transition-colors duration-200 ${
                      steeringAutoReturn ? 'border-blue-800/80' : 'border-amber-800/80'
                   }`}
                   onPointerDown={handleSteeringStart}
                   onPointerMove={handleSteeringMove}
                   onPointerUp={handleSteeringEnd}
                   onPointerCancel={handleSteeringEnd}
                >
                   {/* Línea divisoria central */}
                   <div className="absolute left-1/2 top-0 h-full w-[1px] bg-slate-800"></div>

                   {/* Relleno dinámico de color */}
                   <div 
                      className={`absolute top-0 h-full rounded-full transition-colors duration-200 ${
                         steeringAutoReturn ? 'bg-blue-500/20' : 'bg-amber-500/20'
                      }`}
                      style={{
                         width: `${Math.abs(steering) * 50}%`,
                         left: steering >= 0 ? '50%' : 'auto',
                         right: steering < 0 ? '50%' : 'auto'
                      }}
                   ></div>

                   {/* Volante (Handle) */}
                   <div 
                      className={`absolute w-7 h-7 rounded-full shadow-lg border-2 top-[1px] transition-all duration-75 flex items-center justify-center ${
                         draggingSteering 
                            ? steeringAutoReturn 
                               ? 'bg-blue-500 border-blue-300 scale-95 shadow-[0_0_12px_rgba(59,130,246,0.4)]' 
                               : 'bg-amber-500 border-amber-300 scale-95 shadow-[0_0_12px_rgba(245,158,11,0.4)]'
                            : steeringAutoReturn
                               ? 'bg-blue-600 border-blue-500 shadow-[0_0_8px_rgba(59,130,246,0.3)] hover:border-blue-400'
                               : 'bg-amber-600 border-amber-500 shadow-[0_0_8px_rgba(245,158,11,0.3)] hover:border-amber-400'
                      }`}
                      style={{ 
                         left: `${(steering + 1) * 50}%`,
                         transform: 'translateX(-50%)'
                      }}
                   >
                      <div className="h-3 w-[2px] bg-white/40 rounded-full"></div>
                   </div>
                </div>
                
                <span className="text-[9px] font-mono text-slate-500 font-bold">▶</span>
             </div>
             
             <span className="text-[10px] font-mono text-slate-400 font-bold mt-1">{(steering).toFixed(2)}</span>
          </div>
       </div>

       {/* Línea divisora decorativa izquierda */}
       <div className="h-24 w-[1px] bg-slate-800/40"></div>

       {/* 2. SECCIÓN JOYSTICK VIRTUAL CENTRAL (PS2 STYLE - CENTRO) */}
       <div className="flex flex-col items-center gap-1">
          <span className="text-[9px] font-mono uppercase text-slate-500 font-bold tracking-wider mb-2">Joystick Central</span>

          <div className="flex items-center gap-3">

            {/* SPIN LEFT */}
            <button
              onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); handleSpinStart("left"); }}
              onPointerUp={handleSpinEnd}
              onPointerCancel={handleSpinEnd}
              className={"w-10 h-28 border rounded-xl text-[8px] font-mono font-bold transition-all duration-150 active:scale-95 select-none touch-none flex flex-col items-center justify-center gap-2 " + (
                spinning === "left"
                  ? "bg-violet-500/20 border-violet-400/60 text-violet-300 shadow-[0_0_12px_rgba(139,92,246,0.35)]"
                  : "bg-slate-800/60 border-slate-700 text-slate-500 hover:border-violet-500/50 hover:text-violet-400"
              )}
              title="Girar sobre si mismo a la izquierda"
            >
              <svg className="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
                <path d="M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8"/>
                <path d="M3 3v5h5"/>
              </svg>
              <span className="leading-none">S<br/>P<br/>I<br/>N<br/><br/>L</span>
            </button>

            {/* JOYSTICK */}
            <div 
               ref={joyContainerRef}
               className={`w-28 h-28 bg-slate-950/60 rounded-full border relative flex items-center justify-center shadow-inner cursor-pointer transition-colors duration-200 ${
                  draggingJoy 
                     ? 'border-blue-500/50 shadow-[0_0_15px_rgba(59,130,246,0.15)]' 
                     : 'border-slate-800/80'
               }`}
               onPointerDown={handleJoyStart}
               onPointerMove={handleJoyMove}
               onPointerUp={handleJoyEnd}
               onPointerCancel={handleJoyEnd}
            >
               {/* Ejes guía (cruceta interior) */}
               <div className="absolute inset-x-0 top-1/2 h-[1px] bg-slate-800/40"></div>
               <div className="absolute inset-y-0 left-1/2 w-[1px] bg-slate-800/40"></div>
               
               {/* Círculo límite interior */}
               <div className="absolute w-20 h-20 border border-slate-800/20 rounded-full"></div>

               {/* Relleno radial dinámico */}
               <div 
                  className="absolute inset-0 rounded-full opacity-10 pointer-events-none transition-all duration-200"
                  style={{
                     background: `radial-gradient(circle at ${50 + steering * 50}% ${50 - throttle * 50}%, rgba(59, 130, 246, 0.4) 0%, transparent 60%)`
                  }}
               ></div>

               {/* El Stick (Knob / Palanca) */}
               <div 
                  className={`absolute w-10 h-10 rounded-full shadow-2xl border-2 transition-all duration-75 flex items-center justify-center cursor-pointer select-none ${
                     draggingJoy 
                        ? 'bg-gradient-to-b from-blue-500 to-blue-600 border-blue-300 scale-95 shadow-[0_0_15px_rgba(59,130,246,0.5)]'
                        : 'bg-gradient-to-b from-slate-700 to-slate-800 border-slate-600 hover:border-slate-500 shadow-black/80'
                  }`}
                  style={{
                     transform: `translate(${steering * maxRadius}px, ${-throttle * maxRadius}px)`
                  }}
               >
                  {/* Textura interior de la palanca PS2 */}
                  <div className="w-5 h-5 rounded-full border border-white/10 flex items-center justify-center bg-black/10">
                     <div className="w-2 h-2 rounded-full bg-white/20"></div>
                  </div>
               </div>
            </div>

            {/* SPIN RIGHT */}
            <button
              onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); handleSpinStart("right"); }}
              onPointerUp={handleSpinEnd}
              onPointerCancel={handleSpinEnd}
              className={"w-10 h-28 border rounded-xl text-[8px] font-mono font-bold transition-all duration-150 active:scale-95 select-none touch-none flex flex-col items-center justify-center gap-2 " + (
                spinning === "right"
                  ? "bg-violet-500/20 border-violet-400/60 text-violet-300 shadow-[0_0_12px_rgba(139,92,246,0.35)]"
                  : "bg-slate-800/60 border-slate-700 text-slate-500 hover:border-violet-500/50 hover:text-violet-400"
              )}
              title="Girar sobre si mismo a la derecha"
            >
              <svg className="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
                <path d="M21 12a9 9 0 1 1-9-9 9.75 9.75 0 0 1 6.74 2.74L21 8"/>
                <path d="M21 3v5h-5"/>
              </svg>
              <span className="leading-none">S<br/>P<br/>I<br/>N<br/><br/>R</span>
            </button>

          </div>
          
          <div className="flex gap-3 text-[9px] font-mono text-slate-400 font-bold mt-1.5">
             <span className="w-10 text-left">X: {steering.toFixed(2)}</span>
             <span className="w-10 text-right">Y: {throttle.toFixed(2)}</span>
          </div>

          <button 
             onClick={() => {
                setSteering(0);
                setThrottle(0);
                updateMove(0, 0);
             }}
             className="mt-1 px-2.5 py-1 bg-slate-800/80 hover:bg-slate-700 active:scale-95 border border-slate-700 text-slate-300 rounded-lg text-[9px] font-mono font-bold transition-all duration-150 flex items-center gap-1 shadow-md shadow-black/30"
             title="Resetear dirección y aceleración a cero"
          >
             <svg xmlns="http://www.w3.org/2000/svg" width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round"><circle cx="12" cy="12" r="10"/><line x1="22" y1="12" x2="18" y2="12"/><line x1="6" y1="12" x2="2" y2="12"/><line x1="12" y1="6" x2="12" y2="2"/><line x1="12" y1="22" x2="12" y2="18"/></svg>
             CENTRAR
          </button>
       </div>

       {/* Línea divisora decorativa derecha */}
       <div className="h-24 w-[1px] bg-slate-800/40"></div>

       {/* 3. SECCIÓN ACELERADOR (THROTTLE - DERECHA) */}
       <div className="flex items-center gap-2">
          {/* Pista del deslizador vertical */}
          <div className="flex flex-col items-center gap-1">
             <span className="text-[9px] font-mono uppercase text-slate-500 font-bold tracking-wider mb-1">Aceleración</span>
             <div 
                ref={throttleTrackRef}
                className={`w-7 h-24 bg-slate-950/60 rounded-full border relative flex justify-center shadow-inner transition-colors duration-200 ${
                   throttleAutoReturn ? 'border-blue-800/80 cursor-ns-resize' : 'border-amber-800/80 cursor-ns-resize'
                }`}
                onPointerDown={handleThrottleStart}
                onPointerMove={handleThrottleMove}
                onPointerUp={handleThrottleEnd}
                onPointerCancel={handleThrottleEnd}
             >
                {/* Línea divisoria central */}
                <div className="absolute top-1/2 left-0 w-full h-[1px] bg-slate-800"></div>
                
                {/* Relleno dinámico de color */}
                <div 
                   className={`absolute left-0 w-full rounded-full opacity-30 transition-colors duration-200 ${
                      throttleAutoReturn ? 'bg-blue-500' : 'bg-amber-500'
                   }`}
                   style={{
                      height: `${Math.abs(throttle) * 50}%`,
                      bottom: throttle >= 0 ? '50%' : 'auto',
                      top: throttle < 0 ? '50%' : 'auto'
                   }}
                ></div>

                {/* Palanca (Handle) */}
                <div 
                   className={`absolute w-7 h-7 rounded-full shadow-lg border-2 left-0 transition-all duration-75 flex items-center justify-center ${
                      draggingThrottle 
                         ? throttleAutoReturn 
                            ? 'bg-blue-500 border-blue-300 scale-95 shadow-[0_0_12px_rgba(59,130,246,0.4)]'
                            : 'bg-amber-500 border-amber-300 scale-95 shadow-[0_0_12px_rgba(245,158,11,0.4)]' 
                         : throttleAutoReturn
                            ? 'bg-blue-600 border-blue-500 shadow-[0_0_8px_rgba(59,130,246,0.3)] hover:border-blue-400'
                            : 'bg-amber-600 border-amber-500 shadow-[0_0_8px_rgba(245,158,11,0.3)] hover:border-amber-400'
                   }`}
                   style={{ 
                      bottom: `${(throttle + 1) * 50}%`,
                      transform: 'translateY(50%)'
                   }}
                >
                   <div className="w-3 h-[2px] bg-white/40 rounded-full"></div>
                </div>
             </div>
             <span className="text-[10px] font-mono text-slate-400 font-bold mt-1">{(throttle).toFixed(2)}</span>
          </div>

          {/* Etiquetas y botón STOP físico */}
          <div className="flex flex-col justify-between h-28 text-[9px] font-mono text-slate-500 py-1 pl-1">
             <span className="text-emerald-500 font-bold tracking-wide text-center">▲ FWD</span>
             
             <button 
                onClick={() => { 
                   setThrottle(0); 
                   updateMove(steering, 0); 
                }}
                className="px-2 py-1 bg-rose-500/10 hover:bg-rose-500/20 active:scale-95 border border-rose-500/30 text-rose-400 rounded-lg hover:text-rose-300 transition-all font-mono text-[9px] font-bold shadow-lg shadow-rose-950/20"
             >
                STOP
             </button>
             
             <span className="text-rose-500 font-bold tracking-wide text-center">▼ REV</span>
          </div>

          {/* Selector de modo Acelerador */}
          <div className="flex flex-col justify-center h-28 text-[9px] font-mono text-slate-500 py-1 pl-1">
             <span className="text-slate-500 font-bold tracking-wide text-center mb-1.5 uppercase">Modo</span>
             <button 
                onClick={toggleThrottleMode}
                className={`px-2 py-2 flex flex-col items-center justify-center gap-1 active:scale-95 border rounded-xl transition-all duration-200 font-mono text-[9px] font-bold shadow-lg w-14 ${
                   throttleAutoReturn 
                      ? 'bg-blue-500/10 border-blue-500/30 text-blue-400 hover:bg-blue-500/20' 
                      : 'bg-amber-500/10 border-amber-500/30 text-amber-400 hover:bg-amber-500/20 shadow-amber-950/20'
                }`}
                title={throttleAutoReturn ? "Modo Muelle: retorna a cero al soltar" : "Modo Fijo: mantiene la velocidad actual"}
             >
                {throttleAutoReturn ? (
                   <>
                      <svg className="w-4 h-4 text-blue-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                         <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2.5" d="M3 10h10a8 8 0 018 8v2M3 10l6 6m-6-6l6-6" />
                      </svg>
                      <span className="text-[8px] tracking-tight">MUELLE</span>
                   </>
                ) : (
                   <>
                      <svg className="w-4 h-4 text-amber-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                         <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2.5" d="M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z" />
                      </svg>
                      <span className="text-[8px] tracking-tight">FIJO</span>
                   </>
                )}
             </button>
          </div>
       </div>

    </div>
  );
}
