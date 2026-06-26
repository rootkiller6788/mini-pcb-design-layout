# Prerequisite Dependency Tree

## Knowledge Prerequisites for PCB Stackup, Impedance & Routing

```
Electromagnetic Waves (Maxwell's Equations)
  |
  +-- Transmission Line Theory (Telegrapher's Equations)
  |     |
  |     +-- RLGC Parameters
  |     +-- Characteristic Impedance Z0
  |     +-- Propagation Constant gamma
  |     +-- Reflection Coefficient / VSWR
  |
  +-- Material Science
  |     |
  |     +-- Dielectric Properties (er, tan_d)
  |     +-- Conductor Properties (sigma, roughness)
  |     +-- Skin Depth
  |
  +-- Signal Processing
  |     |
  |     +-- Fourier Analysis
  |     +-- S-parameters
  |     +-- Eye Diagrams
  |
  +-- PCB Manufacturing
        |
        +-- Layer Stackup
        +-- Via Drilling/Plating
        +-- IPC Standards (2221, 2152, 6012)
```

## Internal Module Dependencies

```
pcb_material.h  (foundation: dielectric/conductor properties)
  |
  +-- pcb_transmission_line.h  (RLGC, Z0, gamma, S-params)
  |     |
  |     +-- pcb_impedance.h  (microstrip, stripline, CPW, diff pairs)
  |     +-- pcb_signal_integrity.h  (eye diagram, jitter, BER)
  |
  +-- pcb_via.h  (via parasitics, electrical model)
  |
  +-- pcb_stackup.h  (layer definition, design rules)
  |     |
  |     +-- pcb_routing.h  (trace routing, length matching, crosstalk)
```

## Dependencies on Other Mini Modules

| Prerequisite Module | Concepts Used |
|--------------------|---------------|
| 0. mini-signal-system-theory | Fourier transforms, bandwidth |
| 1. mini-circuit-analysis | RLC networks, impedance |
| 7. mini-electromagnetic-wave | Maxwell's equations, wave propagation |
| 18. mini-emc-signal-integrity | Crosstalk, EMI, SI fundamentals |
