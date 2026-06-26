# Knowledge Graph: PCB Stackup, Impedance & Routing

## L1: Definitions
- `PcbStackup` — multi-layer PCB physical structure
- `LayerDefinition` — single layer material/geometry
- `ImpedanceResult` — single-ended Z0 with derived params
- `DiffImpedanceResult` — differential pair Z0 analysis
- `TransmissionLine` — characteristic properties (Z0, gamma, vp)
- `RlgcParams` — per-unit-length RLGC parameters
- `ViaDimensions` / `ViaElectricalModel` — via physical and electrical parameters
- `DielectricMaterial` / `ConductorMaterial` — PCB material properties
- `EyeDiagram` — signal integrity eye metrics
- `JitterDecomposition` — total/deterministic/random jitter breakdown

## L2: Core Concepts
- **Stackup construction**: layer definition, conductor/dielectric alternation
- **Impedance formulas**: IPC-2141A, Hammerstad-Jensen, Wheeler (microstrip); Cohn (stripline); Wen (CPW)
- **Transmission line synthesis**: RLGC extraction from geometry, TL parameter computation
- **Via parasitic extraction**: DC/AC resistance, inductance (Goldfarb & Pucel), capacitance
- **Material database**: 14 standard commercial laminates with frequency-dependent properties
- **SI measurements**: rise/fall time, peak-to-peak, RMS noise

## L3: Mathematical Structures
- **Odd/even mode analysis**: edge-coupled and broadside-coupled differential pairs
- **Mixed-mode S-parameters**: SDD11/SDD21/SCC11/SCD21 mode conversion
- **Telegrapher's equations**: V(x) and I(x) solutions with standing wave patterns
- **RLGC coupling matrix**: NxN cross-coupling for multi-layer systems
- **Via TDR**: time-domain reflectometry of via impedance discontinuities
- **Eye diagram generation**: UI-overlap with sampling point analysis

## L4: Fundamental Laws
- **IPC-2221**: minimum trace width, spacing (voltage-dependent clearance)
- **IPC-2152**: conductor current capacity I=k*dT^b1*A^b2
- **Reflection coefficient**: Gamma=(ZL-Z0)/(ZL+Z0)
- **VSWR/Return Loss/Mismatch Loss**: impedance matching quality metrics
- **Kramers-Kronig causality**: physical realizability check
- **Via aspect ratio**: plating reliability constraint (AR <= 8/12/1)
- **Coffin-Manson fatigue**: via thermal cycle life estimation
- **Skin depth**: delta=sqrt(2/(omega*mu*sigma))

## L5: Algorithms/Methods
- **Inverse impedance design**: bisection on Hammerstad-Jensen for w_from_Z0
- **Impedance sweep**: Z0(w) curves for design-space exploration
- **Sensitivity analysis**: dZ/dw, dZ/dh, dZ/der, dZ/dt via finite differences
- **Via antipad optimization**: bisection for Z_via=Z_trace matching
- **Layer assignment automaton**: optimal S/G/P patterns for N-layer boards
- **PRBS generation**: LFSR for PRBS7/9/11/15/23/31
- **DFE/FFE tap computation**: zero-forcing equalizer design
- **DFE 3-tap FFE**: pre/main/post cursor cancellation
- **Channel capacity**: Shannon capacity from SNR(f) profile

## L6: Canonical Problems
- **Standard 4/6/8/10-layer stackups**: proven industrial designs
- **50 Ohm microstrip on FR-4**: standard controlled-impedance design
- **100 Ohm differential pair on FR-4**: Ethernet/USB/HDMI standard
- **USB 90 Ohm differential pair**: USB 2.0/3.x impedance spec
- **Quarter-wave transformer**: single-frequency Z-matching (Z_T=sqrt(Z0*ZL))
- **Single-stub matching**: shunt stub conjugate matching
- **Multi-section transformer**: binomial/Chebyshev broadband matching
- **NRZ ISI analysis**: eye closure from channel dispersion
- **PAM4 eye analysis**: three-eye (upper/middle/lower) characterization

## L7: Applications
- **Automotive ISO 26262 stackup**: high-Tg, halogen-free, CAF-resistant (Isola 370HR/ITEQ IT-180A)
- **Aerospace IPC-6012 Class 3/A**: high-reliability, thermal cycling (-55 to +125C)
- **Smartphone HDI**: any-layer microvias, 0.8mm total, thin core
- **JLCPCB 4-layer standard**: JLC2313 compatible material stack (ITEQ IT-180A)
- **PCIe Gen3/4/5 trace validation**: insertion loss, return loss, impedance tolerance
- **USB 3.x trace validation**: Z_diff=90ohm, gen-dependent loss budgets
- **DDR4/DDR5 flight time and skew**: JEDEC timing budget compliance
- **HDMI 2.1 routing**: 4 TMDS pairs + clock at 48 Gbps
- **Ethernet 10GBASE-T**: 4-pair routing validation
- **100 Ohm Ethernet diff pair**: 1000BASE-T/10GBASE-T standard

## L8: Advanced Topics
- **112G PAM4 stackup**: Nyquist 28 GHz, Megtron 7 class, backdrilling
- **Rigid-flex transition**: impedance discontinuity analysis at flex interface
- **Embedded capacitance**: thin high-Dk dielectric as distributed bypass
- **Surface roughness Z0 shift**: Hammerstad correction impact on impedance
- **Glass-weave impedance effect**: periodic er variation and intra-pair skew
- **Coaxial via**: signal via + GND ring for continuous return path
- **Via stub damping**: critical damping resistor for resonance suppression
- **Fiber-weave skew reduction**: 10-15 degree angled routing mitigation
- **T-coil inductance**: bandwidth extension for capacitive load compensation
- **Via-to-via crosstalk**: NEXT/FEXT between adjacent signal vias
- **Statistical eye analysis**: peak distortion analysis from single-bit response
- **PSIJ (Power Supply Induced Jitter)**: VCO sensitivity to supply noise
- **SSN (Simultaneous Switching Noise)**: L*dI/dt ground bounce
- **CIJ (Crosstalk-Induced Jitter)**: crosstalk amplitude to jitter conversion

## L9: Research Frontiers
- Advanced dielectric materials (liquid crystal polymer, ceramic-filled PTFE)
- AI/ML-based stackup optimization for complex multi-rail PDN
- Sub-THz PCB design (beyond 100 GHz for 6G)
- Quantum-compatible cryogenic PCB materials
