// Rover Project Script
// Shared data for Rover components (Oliver's Subsystem: Control, Comms, Power)

window.wiringData = {
    // 1. CEREBRO Y SENSORES (ESP32 de Oliver)
    robot: [
        { pin: "VIN (5V)", dest: "Salida del Step-Down MP1584", type: "power" },
        { pin: "3V3 (3.3V)", dest: "Salida hacia lógica VDD del TMC2209", type: "power" },
        { pin: "GND", dest: "Punto de Masa Común (Star-Grounding)", type: "ground" },
        { pin: "D34 (ADC)", dest: "Divisor de Voltaje (Medidor de Batería 3S)", type: "sensor" },
        
        // Motores Tracción (PWM/DIR a DRV8871H)
        { pin: "D32 / D33", dest: "Driver DRV8871H #4 (Motor Back-Left / W3)", type: "motor" },
        { pin: "D25 / D26", dest: "Driver DRV8871H #5 (Motor Mid-Left / W2)", type: "motor" },
        { pin: "D27 / D14", dest: "Driver DRV8871H #6 (Motor Front-Left / W1)", type: "motor" },
        { pin: "D5 / D18",  dest: "Driver DRV8871H #1 (Motor Back-Right / W6)", type: "motor" },
        { pin: "D2 / D4",   dest: "Driver DRV8871H #2 (Motor Mid-Right / W5)", type: "motor" },
        { pin: "D16 / D17", dest: "Driver DRV8871H #3 (Motor Front-Right / W4)", type: "motor" },
        
        // Servos de Dirección e Inclinación (Directos)
        { pin: "D12", dest: "Servo W3 (Steering Back-Left)", type: "signal" },
        { pin: "D13", dest: "Servo W1 (Steering Front-Left)", type: "signal" },
        { pin: "D19", dest: "Servo W6 (Steering Back-Right)", type: "signal" },
        { pin: "D21", dest: "Servo W4 (Steering Front-Right)", type: "signal" },
        { pin: "D15", dest: "Servo Cam Tilt (Inclinación Cámara)", type: "signal" },

        // Stepper Pan (TMC2209)
        { pin: "D23", dest: "TMC2209 (STEP) - Giro Cámara Pan", type: "signal" },
        { pin: "D22", dest: "TMC2209 (DIR)  - Giro Cámara Pan", type: "signal" },

        // Puertos Serie (UART)
        { pin: "TX0 / RX0", dest: "Consola / USB (Debug)", type: "logic" }
    ],
    
    // 2. ENERGÍA (BMS, STEP-DOWNS, CAPACITORES)
    power: [
        { pin: "Batería LiPo 3S", dest: "11.1V a todos los nodos principales", type: "power" },
        { pin: "MP1584 (5V)", dest: "Alimenta ESP32 (VIN)", type: "power" },
        { pin: "XL4015 #1 (6V)", dest: "Potencia para Servos Izquierdos + Cap 1000uF", type: "power" },
        { pin: "XL4015 #2 (6V)", dest: "Potencia para Servos Derechos/Cam + Cap 1000uF", type: "power" },
        { pin: "Divisor Voltaje", dest: "R1 10k a V_BAT, R2 3.3k a GND. Centro a ESP32 D34", type: "sensor" }
    ],

    // 3. MÓDULOS DE DRIVERS (DRV8871H y TMC2209)
    drivers: [
        { pin: "DRV8871 VM", dest: "Alimentación V_BAT directa + Cap 100uF (x6)", type: "power" },
        { pin: "DRV8871 GND", dest: "Al Punto Estrella GND", type: "ground" },
        { pin: "DRV8871 OUT1/2", dest: "A los terminales de motores N20 + caps cerámicos 0.1uF", type: "motor" },
        { pin: "TMC2209 VMOT", dest: "Alimentación V_BAT directa + Cap 100uF", type: "power" },
        { pin: "TMC2209 VDD", dest: "Lógica a 3.3V desde el pin 3V3 del ESP32", type: "power" },
        { pin: "TMC2209 OUT A/B", dest: "A bobinas del Stepper NEMA 17", type: "motor" },
        { pin: "TMC2209 STEP", dest: "Señal de paso desde el pin D23 del ESP32", type: "signal" },
        { pin: "TMC2209 DIR", dest: "Señal de dirección desde el pin D22 del ESP32", type: "signal" },
        { pin: "TMC2209 EN", dest: "Habilitación del driver (conectado a GND)", type: "signal" }
    ]
};
