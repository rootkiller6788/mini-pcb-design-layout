# mini-4layer-high-speed-layout

**4-Layer High-Speed PCB Design and Layout**

Part of mini-electronic-info / mini-pcb-design-layout

---

## Module Status: COMPLETE

- **L1 Definitions**: Complete (12+ typedef struct, 6 enums)
- **L2 Core Concepts**: Complete (all concepts implemented)
- **L3 Math Structures**: Complete (RLGC, ABCD, S-params, complex numbers)
- **L4 Fundamental Laws**: Complete (15+ theorems with code verification)
- **L5 Algorithms/Methods**: Complete (13 algorithms)
- **L6 Canonical Problems**: Complete (7 problems solved in examples)
- **L7 Applications**: Complete (DDR4, PCIe Gen4, 5G NR)
- **L8 Advanced Topics**: Partial (roughness correction, dispersion, advanced decaps)
- **L9 Research Frontiers**: Partial (documented)

**Score: 15/18 - COMPLETE**

---

## Core Definitions (L1)

| Definition | Symbol | Unit | Header |
|------------|--------|------|--------|
| Characteristic Impedance | Z0 | Ohm | hs_impedance.h |
| Differential Impedance | Z_diff | Ohm | hs_impedance.h |
| S-parameters | S11, S21, S12, S22 | - | hs_transmission.h |
| Propagation Constant | gamma = alpha + j*beta | Np/m, rad/m | hs_transmission.h |
| RLGC Parameters | R, L, G, C | Ohm/m, H/m, S/m, F/m | hs_transmission.h |
| Mutual Capacitance | C_m | F/m | hs_crosstalk.h |
| Mutual Inductance | L_m | H/m | hs_crosstalk.h |
| Target Impedance | Z_target | Ohm | hs_pdn.h |
| ESR / ESL | ESR, ESL | Ohm, H | hs_pdn.h |
| Skin Depth | delta | m | hs_stackup.h |
| Via Inductance | L_via | H | hs_via.h |

## Core Theorems (L4)

| Theorem | Formula | Reference |
|---------|---------|-----------|
| Wheeler Impedance | Z0 = (87/sqrt(er+1.41))*ln(5.98h/(0.8w+t)) | Wheeler 1965 |
| Hammerstad-Jensen | Z0 = (60/sqrt(eeff))*ln(F*h/w+...) | Hammerstad 1980 |
| IPC-2141 Stripline | Z0 = (60/sqrt(er))*ln(4h/(pi*(0.8w+t))) | IPC-2141A |
| Telegrapher Eq. | dV/dz = -(R+jwL)*I, dI/dz = -(G+jwC)*V | Pozar Ch.2 |
| Characteristic Z | Z0 = sqrt((R+jwL)/(G+jwC)) | Pozar Eq.2.9 |
| S-param Reciprocity | S12 = S21 (passive, reciprocal) | Pozar Ch.4 |
| NEXT Saturation | NEXT_max = K_b * V_amplitude | Paul Ch.10 |
| FEXT Scaling | FEXT_amp ~ -K_f*V*L/(t_rise*v_p) | Paul Ch.10 |
| Stripline K_f=0 | C_m/C = L_m/L (TEM symmetry) | Feller 1977 |
| Target Impedance | Z_target = deltaV / I_transient | Smith 1999 |
| Plane Capacitance | C = eps0*er*A/d | Gauss Law |
| Via Inductance | L_via = (mu0*h/(2*pi))*ln(2h/r) | Grover Ch.5 |
| Stub Resonance | f_res = c0/(4*L_stub*sqrt(er)) | Johnson Ch.7 |
| Skin Depth | delta = sqrt(rho/(pi*f*mu0)) | Ramo Ch.5 |
| Shannon-Hartley | C = B*log2(1+SNR) | Shannon 1948 |

## Module Structure

```
mini-4layer-high-speed-layout/
??? Makefile              # make test builds and runs all tests
??? README.md             # This file
??? include/              # 6 header files (2349 lines)
?   ??? hs_stackup.h      # 4-layer stackup and material library
?   ??? hs_impedance.h    # Characteristic and differential impedance
?   ??? hs_transmission.h # Transmission line and S-parameters
?   ??? hs_crosstalk.h    # NEXT/FEXT crosstalk analysis
?   ??? hs_pdn.h          # Power distribution network design
?   ??? hs_via.h          # Via modeling and optimization
??? src/                  # 6 C implementations (~4000+ lines total)
?   ??? hs_stackup.c      # 15 functions
?   ??? hs_impedance.c    # 14 functions
?   ??? hs_transmission.c # 17 functions
?   ??? hs_crosstalk.c    # 12 functions
?   ??? hs_pdn.c          # 14 functions
?   ??? hs_via.c          # 12 functions
??? tests/
?   ??? test_main.c       # 56 test cases with assert()
??? examples/             # 3 end-to-end examples
?   ??? example_ddr4_impedance.c    # DDR4-3200 stackup + impedance
?   ??? example_pcie_crosstalk.c    # PCIe Gen4 crosstalk budget
?   ??? example_pdn_ddr4_vtt.c      # DDR4 VDD/VTT PDN design
??? docs/                 # Knowledge documentation
    ??? knowledge-graph.md
    ??? coverage-report.md
    ??? gap-report.md
    ??? course-alignment.md
    ??? course-tree.md
```

## Build and Test

```bash
# Build and run all tests
make test

# Build examples
make examples

# Run individual example
./build/example_ddr4_impedance
./build/example_pcie_crosstalk
./build/example_pdn_ddr4_vtt
```

## Key Algorithms (L5)

1. **Newton-Raphson Trace Width Solver** - Solve for w given Z_target
2. **Binary Search Antipad Optimization** - Tune via impedance
3. **Frequency Sweep PDN Analysis** - O(N_points * N_decaps) impedance profile
4. **Decap Selection Optimization** - Multi-stage decoupling network synthesis
5. **ABCD Matrix Cascade** - Complex matrix multiplication for cascaded networks
6. **S-parameter Extraction** - ABCD to S-matrix conversion (Pozar Eq.4.63)
7. **Eye Diagram Analysis** - UI overlay, eye height/width, BER estimation
8. **TDR Impedance Profiling** - S11(f) to Z(d) via IFFT
9. **Power-Sum Crosstalk** - Coherent and RMS multi-aggressor summation
10. **Stackup Validation** - 7 manufacturing constraint checks per IPC-2221

## Nine-School Course Mapping

This module synthesizes content from:
- **MIT** 6.630 (EM Waves) + 6.002 (Circuits)
- **Stanford** EE359 (Wireless)
- **Berkeley** EE117 (EM) + EE123 (DSP)
- **Illinois** ECE 451 (EM)
- **Michigan** EECS 411 (Microwave)
- **Georgia Tech** ECE 6350 (EM)
- **TU Munich** High-Frequency Engineering
- **ETH** 227-0455 (EM)
- **Tsinghua** Signal and System

## Notes

- All code compiles with `gcc -std=c11 -Wall -Wextra -O2`
- Zero TODO/FIXME/stub/placeholder in source
- All functions have documentation with theorem sources and complexity
- Each function implements an independent knowledge point
