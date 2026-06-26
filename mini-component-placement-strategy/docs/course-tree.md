# Course Tree — mini-component-placement-strategy

## Prerequisite Dependency Graph

```
mini-component-placement-strategy
│
├── [PREREQ] mini-circuit-analysis (1)
│   ├── Kirchhoff's laws → thermal network nodal analysis
│   └── Circuit matrix solution → Gaussian elimination for thermal
│
├── [PREREQ] mini-analog-electronics (2)
│   ├── Op-amp layout considerations → analog domain placement
│   └── Noise coupling → analog/digital separation
│
├── [PREREQ] mini-digital-electronics (3)
│   ├── High-speed digital → critical net constraints
│   └── FPGA/BGA package knowledge → package type definitions
│
├── [PREREQ] mini-electromagnetic-wave (7)
│   ├── EM coupling → keep-out zones, shielding
│   └── Transmission line theory → impedance-controlled nets
│
├── [PREREQ] mini-digital-signal-process (6)
│   ├── Signal flow → connectivity-based placement order
│   └── DSP system architecture → mixed-signal partitioning
│
├── [PREREQ] mini-wireless-mobile-comm (11)
│   └── RF front-end → RF domain placement constraints
│
├── [PREREQ] mini-emc-signal-integrity (18)
│   ├── Signal integrity analysis → critical length checks
│   ├── Differential signaling → diff pair matching
│   └── EMC design rules → spacing, keep-out
│
├── [PREREQ] mini-electronic-mfg-test (19)
│   ├── DFM rules → wave solder orientation, shadowing
│   ├── IPC standards → spacing rules, density levels
│   └── Pick-and-place → CSV export format
│
└── [PREREQ] mini-power-electronics (10)
    ├── Thermal management → thermal network, via optimization
    └── Power dissipation → junction temperature estimation
```

## Knowledge Dependencies

### Must Know (Hard Prerequisites)
1. **Basic circuit theory**: Voltage, current, resistance → thermal-electrical analogy
2. **Coordinate geometry**: 2D points, rectangles, rotation → placement geometry
3. **Linear algebra**: Matrix operations → thermal network solve
4. **Basic thermodynamics**: Heat, temperature, conduction → thermal analysis

### Should Know (Soft Prerequisites)
1. **IPC standards awareness**: IPC-2221, IPC-7351 → design rule context
2. **Optimization theory**: Cost functions, gradient descent → placement strategies
3. **Graph theory**: Connectivity, min-cut → partition-based placement
4. **Statistics**: Mean, variance, correlation → thermal gradient, cost evaluation

### Nice to Know (Enrichment)
1. **VLSI CAD algorithms**: Cell placement → adapted to PCB context
2. **Computational geometry**: Quadtree, convex hull → spatial indexing
3. **Heat transfer theory**: Spreading resistance → advanced thermal analysis
4. **Multi-objective optimization**: Pareto optimality → strategy comparison