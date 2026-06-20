const IconWrapper = ({ children, size = 24, className = "" }) => (
  <svg
    xmlns="http://www.w3.org/2000/svg"
    width={size}
    height={size}
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    strokeWidth="2"
    strokeLinecap="round"
    strokeLinejoin="round"
    className={className}
  >
    {children}
  </svg>
);

const ArrowLeft = (props) => (
  <IconWrapper {...props}>
    <line x1="19" y1="12" x2="5" y2="12" />
    <polyline points="12 19 5 12 12 5" />
  </IconWrapper>
);

const ChevronDown = (props) => (
  <IconWrapper {...props}>
    <polyline points="6 9 12 15 18 9" />
  </IconWrapper>
);

const Menu = (props) => (
  <IconWrapper {...props}>
    <line x1="4" y1="12" x2="20" y2="12" />
    <line x1="4" y1="6" x2="20" y2="6" />
    <line x1="4" y1="18" x2="20" y2="18" />
  </IconWrapper>
);

const X = (props) => (
  <IconWrapper {...props}>
    <line x1="18" y1="6" x2="6" y2="18" />
    <line x1="6" y1="6" x2="18" y2="18" />
  </IconWrapper>
);

const MEGA_SECTIONS = [
  {
    label: "Hardware",
    items: [
      { label: "Tracción 6WD", href: "hardware/traccion.html" },
      { label: "Electrónica de Control", href: "hardware/electronica.html" },
      { label: "Sistema de Alimentación", href: "hardware/energia.html" },
      { label: "Conexiones y Pines", href: "hardware/conexiones.html" },
    ],
  },
  {
    label: "Software",
    items: [
      { label: "Firmware ESP32", href: "software/firmware.html" },
      { label: "MQTT & Mensajería", href: "software/mqtt.html" },
      { label: "Infraestructura de Red", href: "software/infraestructura.html" },
      { label: "Tecnologías Usadas", href: "software/tecnologias.html" },
    ],
  },
  {
    label: "Sensores",
    items: [
      { label: "GPS & Navegación", href: "sensores/gps.html" },
      { label: "Cámara & Vídeo", href: "sensores/camara.html" },
      { label: "LiDAR", href: "sensores/lidar.html" },
    ],
  },
  {
    label: "Documentación",
    items: [
      { label: "Manual de Uso", href: "docs/guia.html" },
    ],
  },
];

const NavBar = ({ basePath = "", isDashboard = false }) => {
  const [isMegaMenuOpen, setIsMegaMenuOpen] = React.useState(false);
  const [isMobileMenuOpen, setIsMobileMenuOpen] = React.useState(false);
  const [openMobileSection, setOpenMobileSection] = React.useState(null);

  const gl = (path) => `${basePath}${path}`;
  const homeHref = gl("index.html");

  const closeMega = () => setIsMegaMenuOpen(false);
  const toggleMobileSection = (label) =>
    setOpenMobileSection((prev) => (prev === label ? null : label));

  return (
    <nav
      className="bg-slate-900 border-b border-slate-800 sticky top-0 z-50 backdrop-blur-md bg-opacity-95"
      onMouseLeave={closeMega}
    >
      {/* ── Main bar ── */}
      <div
        className={
          isDashboard
            ? "w-full px-4"
            : "max-w-7xl mx-auto px-4 sm:px-6 lg:px-8"
        }
      >
        <div className="flex items-center justify-between h-16">
          {/* Home */}
          <a
            href={homeHref}
            className="flex items-center gap-2 text-slate-400 hover:text-white transition-colors shrink-0"
          >
            <ArrowLeft size={16} />
            <span className="hidden sm:inline text-sm">Inicio</span>
          </a>

          {/* Desktop trigger */}
          <div className="hidden md:flex flex-1 justify-center ml-8">
            <button
              className={`px-5 py-2 text-sm font-semibold rounded transition-all duration-200 flex items-center gap-1.5 ${
                isMegaMenuOpen
                  ? "bg-slate-800 text-white"
                  : "text-orange-400 hover:text-white hover:bg-slate-800"
              }`}
              onMouseEnter={() => setIsMegaMenuOpen(true)}
              onClick={() => setIsMegaMenuOpen((v) => !v)}
              aria-expanded={isMegaMenuOpen}
            >
              Menú Principal
              <ChevronDown
                size={14}
                className={`transition-transform duration-200 ${
                  isMegaMenuOpen ? "rotate-180" : ""
                }`}
              />
            </button>
          </div>

          {/* Right side */}
          <div className="hidden lg:flex items-center gap-3 text-xs font-medium text-slate-400 bg-slate-800/50 border border-slate-700/50 rounded px-4 py-1.5 backdrop-blur-sm shadow-sm shrink-0 ml-4">
            <a
              href={gl("control.html")}
              className="px-3 py-1 bg-orange-600 hover:bg-orange-500 text-white rounded transition-colors flex items-center gap-2 shrink-0"
            >
              CENTRO DE MANDO
            </a>
            <a
              href={gl("test.html")}
              className="px-3 py-1 bg-amber-600 hover:bg-amber-500 text-white rounded transition-colors flex items-center gap-2 shrink-0"
            >
              TEST DECK
            </a>
            <a
              href={gl("calibracion.html")}
              className="px-3 py-1 bg-violet-700 hover:bg-violet-600 text-white rounded transition-colors flex items-center gap-2 shrink-0"
            >
              CALIBRACIÓN
            </a>
            <span className="text-slate-300 pl-2 border-l border-slate-700">
              2DO SMR
            </span>
            <span className="w-1 h-1 rounded-full bg-slate-600" />
            <span className="text-slate-300">IES CARDENAL CISNEROS</span>
          </div>

          {/* Mobile hamburger */}
          <div className="md:hidden flex items-center">
            <button
              onClick={() => setIsMobileMenuOpen((v) => !v)}
              className="text-slate-400 hover:text-white p-2"
              aria-label="Abrir menú"
            >
              {isMobileMenuOpen ? <X /> : <Menu />}
            </button>
          </div>
        </div>
      </div>

      {/* ── Mega-menu panel ── */}
      {isMegaMenuOpen && (
        <div
          className="absolute left-0 right-0 top-16 bg-slate-900 border-b border-slate-700 shadow-2xl z-40"
          onMouseEnter={() => setIsMegaMenuOpen(true)}
        >
          <div
            className={
              isDashboard
                ? "w-full px-4 py-6"
                : "max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-6"
            }
          >
            <div className="grid grid-cols-4 gap-0 divide-x divide-slate-800">
              {MEGA_SECTIONS.map((section, i) => (
                <div
                  key={section.label}
                  className={`${i === 0 ? "pr-6" : i === 3 ? "pl-6" : "px-6"}`}
                >
                  <h4 className="text-orange-400 text-[10px] font-bold uppercase tracking-widest mb-3 pb-2 border-b border-slate-800">
                    {section.label}
                  </h4>
                  <div className="space-y-0.5">
                    {section.items.map((item) => (
                      <a
                        key={item.label}
                        href={gl(item.href)}
                        onClick={closeMega}
                        className="block px-2 py-1.5 text-sm text-slate-400 hover:text-orange-400 hover:bg-slate-800/60 rounded transition-colors border-l-2 border-transparent hover:border-orange-500"
                      >
                        {item.label}
                      </a>
                    ))}
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* ── Mobile menu panel ── */}
      {isMobileMenuOpen && (
        <div className="md:hidden bg-slate-900 border-t border-slate-800 animate-fadeInDown">
          <div className="px-4 pt-4 pb-6 space-y-4 max-h-[calc(100vh-4rem)] overflow-y-auto">
            {/* Command buttons */}
            <div className="flex gap-2">
              <a
                href={gl("control.html")}
                onClick={() => setIsMobileMenuOpen(false)}
                className="flex-1 flex items-center justify-center py-3 bg-gradient-to-r from-orange-600 to-orange-500 rounded text-white font-bold tracking-wider shadow-lg shadow-orange-900/40 hover:from-orange-500 hover:to-orange-400 transition-all active:scale-95 text-sm"
              >
                CENTRO DE MANDO
              </a>
              <a
                href={gl("test.html")}
                onClick={() => setIsMobileMenuOpen(false)}
                className="flex-1 flex items-center justify-center py-3 bg-gradient-to-r from-amber-600 to-amber-500 rounded text-white font-bold tracking-wider shadow-lg shadow-amber-900/40 hover:from-amber-500 hover:to-amber-400 transition-all active:scale-95 text-sm"
              >
                TEST DECK
              </a>
            </div>
            <a
              href={gl("calibracion.html")}
              onClick={() => setIsMobileMenuOpen(false)}
              className="flex items-center justify-center py-3 bg-gradient-to-r from-violet-700 to-violet-600 rounded text-white font-bold tracking-wider shadow-lg shadow-violet-900/40 hover:from-violet-600 hover:to-violet-500 transition-all active:scale-95 text-sm"
            >
              CALIBRACIÓN BRÚJULA
            </a>

            {/* Thematic accordion */}
            {MEGA_SECTIONS.map((section) => (
              <div
                key={section.label}
                className="bg-slate-800/30 rounded border border-slate-700/50"
              >
                <button
                  className="w-full flex items-center justify-between px-4 py-3 text-sm font-semibold text-orange-400 uppercase tracking-wider"
                  onClick={() => toggleMobileSection(section.label)}
                >
                  {section.label}
                  <ChevronDown
                    size={14}
                    className={`transition-transform duration-200 ${
                      openMobileSection === section.label ? "rotate-180" : ""
                    }`}
                  />
                </button>
                {openMobileSection === section.label && (
                  <div className="px-4 pb-3 grid grid-cols-2 gap-1 border-t border-slate-700/50 pt-2">
                    {section.items.map((item) => (
                      <a
                        key={item.label}
                        href={gl(item.href)}
                        onClick={() => setIsMobileMenuOpen(false)}
                        className="block px-2 py-1.5 text-xs text-slate-400 hover:text-orange-400 hover:bg-slate-800 rounded transition-colors"
                      >
                        {item.label}
                      </a>
                    ))}
                  </div>
                )}
              </div>
            ))}

            {/* Footer info */}
            <div className="border-t border-slate-800 pt-4 flex justify-between text-[10px] font-medium text-slate-500 px-2">
              <span>2DO SMR</span>
              <span>IES CARDENAL CISNEROS</span>
            </div>
          </div>
        </div>
      )}
    </nav>
  );
};

window.NavBar = NavBar;
