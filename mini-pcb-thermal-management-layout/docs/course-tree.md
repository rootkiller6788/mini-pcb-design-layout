# Course Tree - PCB Thermal Management Layout

## Prerequisites (What This Module Depends On)

```
mini-signal-system-theory
  L1: Fourier series, Laplace transform
  L2: Linear systems, transfer functions
  L3: Convolution, correlation
  --> Needed for: transient thermal analysis (L3), Foster/Cauer models (L3)

mini-circuit-analysis
  L1: Voltage/current/power definitions
  L2: Kirchhoff laws, series/parallel
  L4: Ohm's Law
  --> Needed for: thermal resistance networks (L2, L4), electrical-thermal analogy

mini-analog-electronics
  L1: Semiconductor packages, power dissipation
  L2: LDO regulators, power amplifiers
  L6: Thermal design for analog circuits
  --> Needed for: LDO cooling example (L6), power device thermal design (L7)

mini-digital-electronics
  L1: IC packaging, power consumption
  L2: CMOS power dissipation (static + dynamic)
  --> Needed for: package thermal resistance models (L1), multi-component thermal analysis
```

## Dependents (What Depends On This Module)

```
mini-power-electronics
  Uses: MOSFET heatsink design (L6), thermal runaway check (L8)
  Uses: Arrhenius lifetime estimation (L7)
  Uses: Via array optimization for power modules (L6)

mini-wireless-mobile-comm
  Uses: RF PA copper pour and heatsink design (L6)
  Uses: Natural/forced convection calculations (L2)
  Uses: Mutual heating between RF components (L2)

mini-radar-remote-sensing
  Uses: GaN power amplifier thermal management (L7)
  Uses: Heat sink selection for outdoor enclosures (L6)
  Uses: Arrhenius reliability for 24/7 operation (L7)

mini-emc-signal-integrity
  Uses: PCB stack-up thermal modeling (L5)
  Uses: Copper plane effective conductivity (L2)
  Uses: Thermal vias for heat + ground (L5)

mini-electronic-mfg-test
  Uses: Material database for PCB fabrication (L1)
  Uses: CTE compatibility checking (L2)
  Uses: Copper weight vs thermal performance (L2)

mini-iot-edge-computing
  Uses: Enclosed electronics thermal design (L6)
  Uses: Passive cooling (copper pour + natural convection) (L6)
```

## Internal Dependency Tree

```
L1 (Definitions) 
  --> L2 (Core Concepts) 
    --> L3 (Math Structures) 
      --> L4 (Fundamental Laws)
        --> L5 (Algorithms)
          --> L6 (Canonical Problems)
            --> L7 (Applications)
              --> L8 (Advanced Topics)
                --> L9 (Research Frontiers)

Key: Each level builds on all previous levels.
All dependencies satisfied within this module.
