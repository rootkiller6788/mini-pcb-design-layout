# Mini PCB Design & Layout

A collection of **from-scratch, zero-dependency C implementations** of PCB design theory, layout algorithms, and manufacturing analysis. Each module translates industry-standard design rules (IPC-2221/2222/2223/6012/6013/7351), transmission line theory, thermal physics, and schematic capture concepts into runnable C code, bridging the gap between EDA tools and first-principles understanding.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|------------|--------|-------------|
| [mini-4layer-high-speed-layout](mini-4layer-high-speed-layout/) | Crosstalk (NEXT/FEXT), characteristic/differential impedance, PDN design, 4-layer stackup, S-parameters, via modeling | MIT 6.776, Stanford EE273, Bogatin SI |
| [mini-component-placement-strategy](mini-component-placement-strategy/) | Multi-objective placement optimization, simulated annealing, force-directed placement, thermal-aware placement, DRC constraints | CMU 15-462, MIT 6.002 |
| [mini-flex-rigid-flex-design](mini-flex-rigid-flex-design/) | Bend mechanics, IPC-2223/6013 design rules, flex materials, rigid-flex transitions, flex signal integrity, flex stackup, flex thermal | IPC-2223, IPC-6013 |
| [mini-gerber-generation-dfm-review](mini-gerber-generation-dfm-review/) | Gerber RS-274X/X2 generation, Excellon drill format, DFM rule checking, PCB geometry primitives | Berkeley EE117, TU Munich HF Eng |
| [mini-pcb-design-for-manufacturing](mini-pcb-design-for-manufacturing/) | DFM design rules, cost modeling, panelization/2D bin packing, thermal DFM, yield models (Poisson/Murphy/Seeds) | IPC-2221, IPC-2222, IPC-6012, IPC-7351 |
| [mini-pcb-stackup-impedance-routing](mini-pcb-stackup-impedance-routing/) | PCB stackup design, Wheeler/Wadell impedance formulas, transmission line theory, signal integrity metrics, routing rules, via design | MIT 6.002, Bogatin SI, Wheeler 1965 |
| [mini-pcb-thermal-management-layout](mini-pcb-thermal-management-layout/) | Steady-state/transient thermal analysis, 2D FDM simulation, thermal via optimization, material database, copper pour sizing | MIT 6.630, Berkeley EE105, Stanford EE359 |
| [mini-schematic-design-kicad](mini-schematic-design-kicad/) | Schematic core data structures, S-expression parser, ERC, netlist extraction (SPICE/IPC-D-356/EDIF), connectivity graph, BOM generation | MIT 6.002, Berkeley EE16A/B, Stanford EE272 |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Standards-driven** — every module maps to IPC standards (IPC-2221/2222/2223/6012/6013/7351) and textbook theory
- **Practical demos** — impedance calculators, DFM checkers, placement optimizers, netlist exporters, and more

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-4layer-high-speed-layout
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-pcb-design-layout/
├── mini-4layer-high-speed-layout/       # Crosstalk, impedance, PDN, stackup, S-params, vias
├── mini-component-placement-strategy/   # Multi-objective placement optimization
├── mini-flex-rigid-flex-design/         # Flex & rigid-flex: bend, materials, transitions
├── mini-gerber-generation-dfm-review/   # Gerber RS-274X/X2, Excellon, DFM checking
├── mini-pcb-design-for-manufacturing/   # DFM rules, cost, panelization, yield models
├── mini-pcb-stackup-impedance-routing/  # Stackup, impedance, transmission lines, routing
├── mini-pcb-thermal-management-layout/  # Steady-state/transient thermal, FDM simulation
└── mini-schematic-design-kicad/         # Schematic capture, ERC, netlist, BOM generation
```

## License

MIT
