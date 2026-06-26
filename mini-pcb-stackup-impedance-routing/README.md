# mini-pcb-stackup-impedance-routing

PCB Stackup Design, Impedance Calculation, and High-Speed Routing Analysis Library.

## Module Status: COMPLETE ✅

- **L1-L6**: Complete
- **L7**: Complete (7+ applications)
- **L8**: Complete (10+ advanced topics)
- **L9**: Partial (documented, not implemented)

## Knowledge Coverage Summary

| Level | Name | Status |
|-------|------|--------|
| L1 | Definitions | ✅ Complete — 10+ struct types |
| L2 | Core Concepts | ✅ Complete — 7 implementation files |
| L3 | Math Structures | ✅ Complete — RLGC matrix, Telegrapher, mode conversion |
| L4 | Fundamental Laws | ✅ Complete — IPC-2221/2152, VSWR, skin depth |
| L5 | Algorithms/Methods | ✅ Complete — Bisection, sensitivity, PRBS, DFE |
| L6 | Canonical Problems | ✅ Complete — Standard stackups, 50/100 Ohm designs |
| L7 | Applications | ✅ Complete — Automotive, Aerospace, PCIe, DDR5, USB |
| L8 | Advanced Topics | ✅ Complete — 112G, rigid-flex, glass-weave, PSIJ |
| L9 | Research Frontiers | ⚠️ Partial — Documented only |

## Core Definitions

- **PcbStackup**: Multi-layer PCB physical structure (up to 32 layers)
- **ImpedanceResult**: Single-ended Z0 with er_eff, delay, C/L per unit length
- **DiffImpedanceResult**: Differential pair Z_diff, Z_odd, Z_even, coupling coefficient
- **TransmissionLine**: Z0, gamma(alpha+j*beta), vp, wavelength, delay
- **ViaElectricalModel**: Via R, L, C, stub capacitance, resonant frequency
- **EyeDiagram**: Eye height, width, Q-factor, RMS jitter, SNR
- **JitterDecomposition**: TJ, DJ, RJ, PJ, DCD, ISI jitter components

## Core Theorems & Formulas

1. **IPC-2141A Microstrip** (Z0 for w/h < 1 and w/h >= 1)
2. **Hammerstad-Jensen** (most accurate closed-form, +/-1%)
3. **Wheeler** (1977 empirical microstrip)
4. **Cohn Stripline** (1954 symmetric stripline)
5. **Wen CPW** (1969 coplanar waveguide with elliptic integrals)
6. **Telegrapher's Equations**: V(x)=V+exp(-gamma*x)+V-exp(+gamma*x)
7. **Reflection Coefficient**: Gamma=(ZL-Z0)/(ZL+Z0)
8. **IPC-2152 Current Capacity**: I=k*dT^beta1*A^beta2
9. **Goldfarb-Pucel Via Inductance**: L=5.08*h*[ln(2h/r)+1] nH
10. **Coffin-Manson Fatigue**: N_f=C*(delta_epsilon)^(-n)
11. **Shannon Capacity**: C=sum(df*log2(1+SNR))

## Core Algorithms

1. **Bisection inverse design**: w_from_Z0, s_from_Zdiff (30-60 iterations, 0.01µm precision)
2. **Impedance sweep**: Z0(w) over w_min to w_max design space
3. **Sensitivity analysis**: dZ/dw, dZ/dh, dZ/der, dZ/dt finite differences
4. **Via antipad optimization**: D_antipad from Z0 matching
5. **Layer assignment automaton**: S/G/P patterns for N-layer boards
6. **PRBS generation**: LFSR for PRBS7 through PRBS31
7. **DFE/FFE equalizer tap computation**: zero-forcing criterion
8. **Eye diagram analysis**: UI overlap, bathtub curve, BER estimation

## Classic Problems

1. Standard 4/6/8/10-layer stackup design (JLCPCB compatible)
2. 50 Ohm microstrip on FR-4 (controlled impedance)
3. 100 Ohm differential pair on FR-4 (Ethernet standard)
4. Quarter-wave impedance transformer
5. Single-stub shunt matching (Smith chart analytical solution)
6. DDR4 fly-by bus timing verification
7. PCIe lane-to-lane skew budget analysis

## Course Alignment

| School | Course | Topics |
|--------|--------|--------|
| MIT | 6.630 | TL theory, impedance matching |
| Stanford | EE247 | High-speed interconnects, SI |
| Berkeley | EE117 | Microstrip/stripline analysis |
| UIUC | ECE 451 | Via modeling, discontinuities |
| Michigan | EECS 411 | Microwave impedance matching |
| Georgia Tech | ECE 6350 | Multi-layer EM structures |
| TU Munich | HF Engineering | CPW, microstrip design |
| ETH | 227-0455 | Advanced TL, S-parameters |
| Tsinghua | EM Fields | PCB stackup, impedance control |

## Build & Test

```
make          # compile library
make test     # build and run all tests
make examples # build and run all examples
make clean    # remove build artifacts
```

## File Structure

```
├── Makefile           # GNU Make build system
├── README.md          # This file
├── include/           # 7 header files
│   ├── pcb_impedance.h
│   ├── pcb_material.h
│   ├── pcb_routing.h
│   ├── pcb_signal_integrity.h
│   ├── pcb_stackup.h
│   ├── pcb_transmission_line.h
│   └── pcb_via.h
├── src/               # 7 C implementation files
│   ├── pcb_impedance.c
│   ├── pcb_material.c
│   ├── pcb_routing.c
│   ├── pcb_signal_integrity.c
│   ├── pcb_stackup.c
│   ├── pcb_transmission_line.c
│   └── pcb_via.c
├── tests/             # Test suite
│   └── test_stackup_impedance.c
├── examples/          # End-to-end examples
│   ├── example_4layer_stackup.c
│   ├── example_50ohm_design.c
│   └── example_ddr_routing.c
├── docs/              # Knowledge documentation
│   ├── knowledge-graph.md
│   ├── coverage-report.md
│   ├── gap-report.md
│   ├── course-alignment.md
│   └── course-tree.md
├── benches/           # Performance benchmarks
└── demos/             # Visualization/demos
```
