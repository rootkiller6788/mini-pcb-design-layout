# mini-flex-rigid-flex-design

> Flex and Rigid-Flex PCB Design — Mechanics, Materials, Signal Integrity, and Thermal Analysis

## Module Status: COMPLETE ✅

- **L1 (Definitions)**: Complete — 29 core definitions (11 enums, 18 structs)
- **L2 (Core Concepts)**: Complete — 14 concepts implemented across 7 modules
- **L3 (Math Structures)**: Complete — 12 mathematical structures fully typed
- **L4 (Fundamental Laws)**: Complete — 15 laws with C implementation + Lean formalization
- **L5 (Algorithms)**: Complete — 56 algorithms implementing IPC standards and physics models
- **L6 (Canonical Problems)**: Complete — 8 problems with end-to-end solutions
- **L7 (Applications)**: Complete — 3 real-world applications
- **L8 (Advanced Topics)**: Partial+ — 3 advanced topics (LEFM, DS model, Suhir theory)
- **L9 (Research Frontiers)**: Partial — 4 topics documented

**Score: 16/18** (L1-L6: Complete, L7: Complete, L8: Partial, L9: Partial)

---

## Core Definitions (L1)

### Material Types
- **Dielectric**: Polyimide (Kapton), LCP, PET, PEN, PTFE, MPI, Flexible Epoxy
- **Copper**: Rolled Annealed (RA), Electrodeposited (ED), Low-Profile variants
- **Adhesive**: Acrylic, Epoxy, PI Adhesive, PSA, Adhesiveless
- **Stiffener**: FR-4, Polyimide, Aluminum, Stainless Steel, Copper
- **Coverlay**: PI Film, LPI (Liquid Photoimageable), PIC

### Key Structs
| Struct | Fields | Purpose |
|--------|--------|---------|
| `flex_dielectric_electrical_t` | 9 | εr, tanδ, DI strength across frequency |
| `flex_dielectric_mechanical_t` | 10 | E, CTE, Tg, k_thermal |
| `flex_copper_foil_t` | 9 | ρ, tensile, roughness, fatigue |
| `flex_layer_t` | 16 | Single layer in stackup |
| `flex_stackup_t` | 16 | Complete rigid-flex stackup |
| `flex_bend_params_t` | 12 | Bend analysis inputs |
| `flex_bend_result_t` | 11 | Bend reliability output |
| `flex_tl_params_t` | 16 | TL geometry + materials |
| `flex_tl_result_t` | 14 | Z0, loss, delay, crosstalk |

---

## Core Theorems & Laws (L4)

| # | Law | Formula | Reference |
|---|-----|---------|-----------|
| 1 | IPC-2223 Min Bend Radius | R_min = k·t (k=6,12,20,25) | IPC-2223C §5.2.4 |
| 2 | Bernoulli-Euler Beam Bending | σ = E·y/R | Timoshenko |
| 3 | Hooke's Law | σ = E·ε | Hooke 1678 |
| 4 | Coffin-Manson Fatigue | N_f = (ε_f/ε_a)^c | Coffin 1954 |
| 5 | Fourier Heat Conduction | q = -k·A·dT/dx | Fourier 1822 |
| 6 | Newton Cooling (Convection) | q = h·A·ΔT | Newton 1701 |
| 7 | Suhir Bimaterial Stress | τ_max = (E·Δα·ΔT·βL/2)/sinh(βL/2) | Suhir 1986 |
| 8 | Griffith LEFM | K_I = σ·√(π·a) ≥ K_IC | Griffith 1921 |
| 9 | Engelmaier-Wild Fatigue | N_f = 0.5·(Δγ/2ε_f)^(1/c) | Engelmaier 1983 |
| 10 | Arrhenius Acceleration | Life(T) = Life(25°C)·exp((Ea/k)·(1/T-1/298)) | Arrhenius 1889 |
| 11 | IPC-2152 Ampacity | I = k·ΔT^b1·A^b2 | IPC-2152 |
| 12 | Timoshenko Bi-Metal Warpage | w = (Δα·ΔT·L²)/(8·h) | Timoshenko 1925 |
| 13 | Djordjevic-Sarkar DK(f) | εr(f) = εr'-(εr'-ε∞)·tanh(α·log(f/f0)) | IEEE T-EMC 2001 |
| 14 | Hammerstad Roughness | Kr = 1+(2/π)·atan(1.4·(Rrms/δ)²) | ELAB 1975 |
| 15 | Wheeler Microstrip Z0 | Z0 = (60/√εeff)·ln(8h/W+W/(4h)) | IEEE MTT 1965 |

---

## Core Algorithms (L5)

### Bend Mechanics
- `flex_min_bend_radius_ipc2223()` — IPC k-factor method
- `flex_min_bend_radius_beam_theory()` — First-principles beam theory
- `flex_copper_strain_percent()` — Maximum strain at outer fiber
- `flex_strain_profile()` — Strain distribution through thickness
- `flex_cycles_to_failure_coffin_manson()` — Strain-life fatigue
- `flex_cycles_ipc_tm650()` — IPC-TM-650 empirical model
- `flex_cycles_temperature_derate()` — Arrhenius temperature derating
- `flex_springback_angle()` — Elastic recovery after forming
- `flex_bend_analyze()` — Complete bend analysis pipeline

### Signal Integrity
- `flex_microstrip_z0_wheeler()` — Wheeler model (1965)
- `flex_microstrip_embedded_z0()` — Coverlay correction
- `flex_stripline_z0()` — Cohn symmetric stripline
- `flex_stripline_asymmetric_z0()` — Offset stripline
- `flex_diff_microstrip_z0()` — Differential pair impedance
- `flex_conductor_loss_db_per_mm()` — Skin effect loss
- `flex_dielectric_loss_db_per_mm()` — Dielectric absorption loss
- `flex_next_coefficient()` — Near-end crosstalk
- `flex_fext_db()` — Far-end crosstalk
- `flex_tl_analyze()` — Unified TL analysis

### Thermal
- `flex_trace_ampacity_ipc2152()` — Safe current capacity
- `flex_thermal_conduction_1d()` — Fourier conduction
- `flex_convection_coefficient()` — Natural/forced convection
- `flex_junction_temperature()` — Component Tj estimation
- `flex_thermal_analyze()` — Complete thermal analysis

---

## Classic Problems Solved (L6)

1. **Minimum bend radius for arbitrary stackup** → `example_bend_analysis.c`
2. **Dynamic flex fatigue life (phone hinge, 200k cycles)** → `example_bend_analysis.c`
3. **50/90/100Ω impedance design on flex** → `example_signal_integrity.c`
4. **Loss budget for 10 Gbps USB 3.2 on LCP** → `example_signal_integrity.c`
5. **6-layer automotive rigid-flex transition** → `example_rigid_flex_design.c`
6. **Thermal derating for under-hood (-40 to +125°C)** → `example_rigid_flex_design.c`
7. **Stackup warpage from CTE mismatch** → `flex_stackup_warpage_estimate()`
8. **DRC violation detection and reporting** → `flex_drc_run_full()`

---

## Nine-School Course Mapping

| School | Relevant Courses | Mapped Content |
|--------|-----------------|----------------|
| **MIT** | 6.003, 6.630, 2.001 | TL theory, EM waves, beam mechanics |
| **Stanford** | EE359, EE247, ME80 | mmWave interconnects, composite beams |
| **Berkeley** | EE117, EE105, MSE113 | TL impedance, interconnects, fracture mechanics |
| **Illinois** | ECE310, ECE451, ME330 | SI, microwave, engineering materials |
| **Michigan** | EECS411, ME382, AUTO566 | HF PCB, fatigue, automotive electronics |
| **Georgia Tech** | ECE4270, ECE6350, ME6201 | DSP, EM, fracture & fatigue |
| **TU Munich** | SigProc, HF Eng, MatSci | TL theory, mmWave, polymers |
| **ETH Zurich** | 227-0427, 227-0455, 151-0306 | SI, microwave IC, mechanics |
| **Tsinghua** | 信号与系统, 通信原理, 电磁场, 工程力学 | Wave prop, interconnects, EM, mechanics |

---

## Build & Test

```bash
make          # Build libflexrigidflex.a
make test     # Build and run test suite (78 tests)
make examples # Build all example programs
make clean    # Remove build artifacts
```

### Test Results
```
=== Results: 78 passed, 0 failed ===
```
All tests pass under `gcc -std=c11 -Wall -Wextra -pedantic -O2`.

---

## Project Structure

```
mini-flex-rigid-flex-design/
├── Makefile                          # Build system
├── README.md                         # This file
├── include/                          # 7 header files (2,553 lines)
│   ├── flex_material.h               # Material types and properties
│   ├── flex_stackup.h                # Layer stackup definitions
│   ├── flex_bend.h                   # Bend mechanics
│   ├── flex_design_rule.h            # IPC DRC rules
│   ├── flex_signal_integrity.h       # SI definitions
│   ├── flex_rigid_transition.h       # Transition zone
│   └── flex_thermal.h               # Thermal analysis
├── src/                              # 8 source files (4,487 lines)
│   ├── flex_material.c               # Material property database
│   ├── flex_stackup.c                # Stackup operations
│   ├── flex_bend.c                   # Bend analysis
│   ├── flex_design_rule.c            # DRC engine
│   ├── flex_impedance.c              # TL impedance calculations
│   ├── flex_rigid_transition.c       # Transition analysis
│   ├── flex_thermal.c               # Thermal analysis
│   └── flex_formal.lean              # Lean 4 formalization
├── tests/
│   └── test_flex.c                   # 78-test comprehensive suite
├── examples/                         # 3 end-to-end examples
│   ├── example_bend_analysis.c       # Phone hinge flex
│   ├── example_signal_integrity.c    # USB 3.2 on LCP
│   └── example_rigid_flex_design.c   # Automotive ECU
├── docs/                             # Knowledge documentation
│   ├── knowledge-graph.md            # L1-L9 coverage map
│   ├── coverage-report.md            # Status assessment
│   ├── gap-report.md                 # Remaining gaps
│   ├── course-alignment.md           # 9-school curriculum
│   └── course-tree.md               # Prerequisite dependency tree
├── benches/                          # Performance benchmarks
├── demos/                            # Visualization demos
└── .gitignore
```

### Line Counts (include/ + src/)
```
include/ : 2,553 lines (7 files)
src/     : 4,487 lines (7 .c files + 1 .lean file)
-----------------------------------------
TOTAL    : 7,040 lines ≥ 3,000 ✓
```

---

## Reference Standards

| Standard | Title | Module Coverage |
|----------|-------|----------------|
| IPC-2223C | Sectional Design Standard for Flex/Rigid-Flex | Bend radius, DRC rules |
| IPC-6013 | Qualification & Performance of Flex/Rigid-Flex | Via, pad, plating rules |
| IPC-2152 | Current-Carrying Capacity in Printed Boards | Trace ampacity |
| IPC-4202 | Flexible Base Dielectrics | Material property tables |
| IPC-4203 | Adhesive Coated Dielectric Films | Adhesive specifications |
| IPC-4204 | Flexible Metal-Clad Dielectrics | Copper-clad materials |
| IPC-4562 | Metal Foil for Printed Boards | Copper foil properties |
| IPC-TM-650 | Test Methods Manual | Flexural fatigue testing |

---

## Key Textbooks

| Topic | Textbook | Author(s) |
|-------|----------|-----------|
| Mechanics of Materials | Mechanics of Materials | Beer & Johnston |
| Elasticity | Theory of Elasticity | Timoshenko & Goodier |
| Heat Transfer | Fundamentals of Heat and Mass Transfer | Incropera & DeWitt |
| Microwave Engineering | Microwave Engineering, 4th Ed. | Pozar |
| Signal Integrity | High-Speed Digital Design | Johnson & Graham |
| Materials Science | Electronic Materials Handbook | ASM International |
| Fracture Mechanics | Fracture Mechanics | Anderson |
