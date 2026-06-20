function MapPanel({ position, heading, target, onSetTarget, gpsState }) {
    const mapRef = React.useRef(null);
    const mapInstanceRef = React.useRef(null);
    const markersRef = React.useRef({ rover: null, target: null });
    const tileLayerRef = React.useRef(null);
    const lineRef = React.useRef(null);
    const prevGpsStateRef = React.useRef(null);
    const [isSatellite, setIsSatellite] = React.useState(true);

    React.useEffect(() => {
        if (!mapRef.current) return;

        // Initialize Map
        if (!mapInstanceRef.current) {
            // Initialize Map with 'tap: false' to avoid deprecated event warnings in modern browsers
            const map = L.map(mapRef.current, {
                tap: false,
                zoomControl: false // We will use our own or default at top-left
            }).setView(position, 18);

            // Add default Zoom control back but in a better place if needed
            L.control.zoom({ position: 'topright' }).addTo(map);

            // Click handler
            map.on('click', (e) => {
                if (onSetTarget) onSetTarget([e.latlng.lat, e.latlng.lng]);
            });

            mapInstanceRef.current = map;

            // Define Custom Icons
            const roverIcon = L.divIcon({
                className: 'rover-marker-arrow bg-transparent border-0',
                html: `<div class="rover-arrow-inner" style="transition: transform 0.2s linear; transform-origin: center;">
                    <svg width="32" height="32" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg" style="filter: drop-shadow(0 2px 4px rgba(0,0,0,0.5));">
                        <path d="M12 2L19 21l-7-3-7 3z" fill="#f97316" stroke="white" stroke-width="2" stroke-linejoin="round"/>
                    </svg>
                   </div>`,
                iconSize: [32, 32],
                iconAnchor: [16, 16] // Center of the 32x32 icon
            });

            // Initialize Markers
            markersRef.current.rover = L.marker(position, { icon: roverIcon }).addTo(map);

            // Fix tile loading issues on resize/init by invalidating size after short delay
            setTimeout(() => map.invalidateSize(), 500);

            // Resize Observer to handle flexbox resizing
            const resizeObserver = new ResizeObserver(() => {
                map.invalidateSize();
            });
            if (mapRef.current) resizeObserver.observe(mapRef.current);
            // Store observer cleanup in the ref or closure? Closure is fine for useEffect cleanup.
            mapInstanceRef.current._resizeObserver = resizeObserver;
        }

        // Cleanup on unmount
        return () => {
            if (mapInstanceRef.current) {
                if (mapInstanceRef.current._resizeObserver) {
                    mapInstanceRef.current._resizeObserver.disconnect();
                }
                mapInstanceRef.current.remove();
                mapInstanceRef.current = null;
            }
        };
    }, []);

    // Handle Map Mode Change (Satellite vs Dark)
    React.useEffect(() => {
        if (!mapInstanceRef.current) return;

        if (tileLayerRef.current) {
            mapInstanceRef.current.removeLayer(tileLayerRef.current);
        }

        if (isSatellite) {
            tileLayerRef.current = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', {
                attribution: 'Tiles &copy; Esri World Imagery',
                maxZoom: 19
            });
        } else {
            tileLayerRef.current = L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
                attribution: '&copy; OpenStreetMap contributors &copy; CARTO',
                subdomains: 'abcd',
                maxZoom: 20
            });
        }

        tileLayerRef.current.addTo(mapInstanceRef.current);
    }, [isSatellite]);


    // Auto-pan map when GPS gets a fix
    React.useEffect(() => {
        if (!mapInstanceRef.current) return;
        if (gpsState === 'FIX' && prevGpsStateRef.current !== 'FIX') {
            mapInstanceRef.current.setView(position, 18);
        }
        prevGpsStateRef.current = gpsState;
    }, [gpsState]);
    // Update Rover Position & Heading
    React.useEffect(() => {
        if (mapInstanceRef.current && markersRef.current.rover) {
            markersRef.current.rover.setLatLng(position);

            // Rotate the arrow icon
            const iconElement = markersRef.current.rover.getElement();
            if (iconElement) {
                const arrowInner = iconElement.querySelector('.rover-arrow-inner');
                if (arrowInner) {
                    arrowInner.style.transform = `rotate(${heading}deg)`;
                }
            }
        }
    }, [position, heading]);

    // Update Target Marker & Line
    React.useEffect(() => {
        if (!mapInstanceRef.current) return;

        if (target) {
            if (!markersRef.current.target) {
                const targetIcon = L.divIcon({
                    className: 'target-marker-custom',
                    html: '<div class="w-5 h-5 bg-red-500 rounded-full border-[3px] border-white shadow-[0_0_10px_rgba(239,68,68,1)]"></div>',
                    iconSize: [20, 20],
                    iconAnchor: [10, 10]
                });
                markersRef.current.target = L.marker(target, { icon: targetIcon }).addTo(mapInstanceRef.current);
            } else {
                markersRef.current.target.setLatLng(target);
            }

            if (!lineRef.current) {
                lineRef.current = L.polyline([position, target], {
                    color: '#ef4444',
                    dashArray: '5, 10',
                    weight: 3,
                    opacity: 0.8
                }).addTo(mapInstanceRef.current);
            } else {
                lineRef.current.setLatLngs([position, target]);
            }
        } else {
            if (markersRef.current.target) {
                mapInstanceRef.current.removeLayer(markersRef.current.target);
                markersRef.current.target = null;
            }
            if (lineRef.current) {
                mapInstanceRef.current.removeLayer(lineRef.current);
                lineRef.current = null;
            }
        }
    }, [target, position]);

    return (
        <div className="w-full h-full flex flex-col">
            {/* Map Container */}
            <div ref={mapRef} className="flex-1 w-full h-full z-10 bg-slate-900" />

            {/* Overlay Controls & Info */}
            <div className="absolute bottom-4 left-4 z-[400] flex flex-col gap-2 items-start">
                {/* Center Button */}
                <button
                    onClick={() => mapInstanceRef.current && mapInstanceRef.current.setView(position, 18)}
                    className="w-10 h-10 bg-slate-900/90 text-white rounded-lg border border-slate-700 shadow-xl flex items-center justify-center hover:bg-orange-600 transition-colors"
                    title="Centrar en Rover"
                >
                    <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><circle cx="12" cy="12" r="10"></circle><line x1="22" y1="12" x2="18" y2="12"></line><line x1="6" y1="12" x2="2" y2="12"></line><line x1="12" y1="6" x2="12" y2="2"></line><line x1="12" y1="22" x2="12" y2="18"></line></svg>
                </button>

                {/* Toggle Layer Button */}
                <button
                    onClick={() => setIsSatellite(!isSatellite)}
                    className={`w-10 h-10 rounded-lg border border-slate-700 shadow-xl flex items-center justify-center transition-colors ${isSatellite ? 'bg-orange-600 text-white' : 'bg-slate-900/90 text-slate-300 hover:bg-slate-800'}`}
                    title={isSatellite ? "Ver Mapa Oscuro" : "Ver Satélite"}
                >
                    <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"></path><polyline points="3.27 6.96 12 12.01 20.73 6.96"></polyline><line x1="12" y1="22.08" x2="12" y2="12"></line></svg>
                </button>

                {/* Info Box */}
                <div className="bg-slate-900/80 backdrop-blur px-3 py-2 rounded-lg border border-slate-700 text-xs font-mono text-slate-300">
                    <div className="flex items-center gap-2">
                        <span className="w-2 h-2 rounded-full bg-orange-500"></span>
                        <span>ROVER: {position[0].toFixed(5)}, {position[1].toFixed(5)}</span>
                    </div>
                    {target && (
                        <div className="flex items-center gap-2 mt-1 text-red-400">
                            <span className="w-2 h-2 rounded-full bg-red-500"></span>
                            <span>TARGET: {target[0].toFixed(5)}, {target[1].toFixed(5)}</span>
                        </div>
                    )}
                </div>
            </div>


        </div>
    );
}
