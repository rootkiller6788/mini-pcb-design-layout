# Course Tree — mini-flex-rigid-flex-design

## Prerequisite Dependency Tree

```
mini-flex-rigid-flex-design/
│
├── Prerequisites (must know before)
│   ├── Mechanics of Materials (beam theory, stress/strain)
│   ├── EM Theory (Maxwell → TL theory)
│   ├── Heat Transfer (conduction, convection)
│   └── Materials Science (polymers, metals, composites)
│
├── Core Knowledge (this module provides)
│   ├── L1: Flex/Rigid-Flex Definitions
│   │   ├── Material types (PI, LCP, PET, Cu RA/ED)
│   │   ├── Geometry types (stackup, bend zone, transition)
│   │   └── Analysis result structures
│   │
│   ├── L2: Core Concepts
│   │   ├── Stackup design (symmetric, asymmetric)
│   │   ├── Bend mechanics (neutral axis, strain)
│   │   ├── DRC methodology (IPC rule checking)
│   │   └── Signal integrity (impedance, loss, crosstalk)
│   │
│   ├── L3: Mathematical Structures
│   │   ├── Beam theory → flexural rigidity
│   │   ├── TL theory → Wheeler, Cohn, Hammerstad
│   │   ├── Thermal series/parallel → composite conductivity
│   │   └── Fracture mechanics → K_IC, critical crack length
│   │
│   ├── L4: Fundamental Laws
│   │   ├── IPC-2223 (bend radius, annular ring, transition)
│   │   ├── Coffin-Manson (fatigue life)
│   │   ├── Suhir (bimaterial interface stress)
│   │   ├── Fourier (heat conduction)
│   │   └── Engelmaier-Wild (thermal fatigue)
│   │
│   ├── L5: Algorithms
│   │   ├── Bend analysis pipeline
│   │   ├── Impedance calculation suite
│   │   ├── Loss budget analysis
│   │   ├── DRC engine
│   │   └── Thermal analysis pipeline
│   │
│   └── L6: Canonical Problems
│       ├── Minimum bend radius for stackup
│       ├── Dynamic flex fatigue prediction
│       ├── 50/90/100Ω impedance design
│       ├── Rigid-flex transition stress
│       └── Thermal derating for automotive
│
├── Downstream Dependencies (needs this module)
│   ├── mini-pcb-design-layout (general PCB design)
│   ├── mini-emc-signal-integrity (SI/PI analysis)
│   ├── mini-electronic-mfg-test (flex manufacturing)
│   ├── mini-wireless-mobile-comm (antenna flex interconnects)
│   └── mini-iot-edge-computing (wearable flex circuits)
│
└── Research Frontiers (L9)
    ├── 6G sub-THz flexible interconnects
    ├── Additive manufacturing of flex circuits
    ├── Stretchable/bendable electronics
    └── AI-driven stackup optimization
```

## Learning Path (Recommended Order)
1. Start with L1: material types and definitions (flex_material.h)
2. Build a simple stackup (flex_stackup.h)
3. Analyze bend reliability (flex_bend.h)
4. Check design rules (flex_design_rule.h)
5. Compute impedance and losses (flex_signal_integrity.h)
6. Analyze transition zone (flex_rigid_transition.h)
7. Thermal analysis (flex_thermal.h)
8. Run end-to-end examples (examples/)
