# Knowledge Graph - mini-4layer-high-speed-layout

## L1: Definitions (Complete)
- Characteristic impedance Z0, differential impedance Z_diff, odd-mode Z_odd, even-mode Z_even
- S-parameters: S11, S21, S12, S22  
- Propagation constant gamma = alpha + j*beta
- RLGC per-unit-length parameters (R, L, G, C)
- NEXT (Near-End Crosstalk), FEXT (Far-End Crosstalk)
- Mutual capacitance C_m, mutual inductance L_m
- PDN target impedance Z_target, ESR, ESL
- Plane pair capacitance, spreading inductance
- Via types: through-hole, blind, buried, microvia
- Via barrel, pad, antipad, stub
- Skin depth delta
- Stackup configuration types, layer types, material types

## L2: Core Concepts (Complete)
- Impedance matching and termination strategies
- Microstrip vs stripline vs CPW geometry
- Single-ended vs differential routing
- 3W rule for crosstalk spacing
- Guard traces and ground fill
- Decoupling hierarchy: VRM->bulk->ceramic->on-package
- Reference plane assignment in 4-layer stackup
- Prepreg vs Core dielectric
- Via transition as impedance discontinuity
- Return path disruption at vias
- Ground stitching vias for EMI control
- Signal rise time bandwidth relationship

## L3: Mathematical Structures (Complete)
- Telegrapher equations: dV/dz = -(R+jwL)*I, dI/dz = -(G+jwC)*V
- ABCD (chain) matrix for cascaded 2-port networks
- S-parameter to ABCD matrix conversion
- Coupled transmission line equations (2N-conductor TL)
- Partial inductance of cylindrical conductor
- Coaxial capacitance model for via barrel
- RLC resonator model for planes and decaps
- Plane pair impedance matrix / eigenmode analysis
- Skin depth: delta = sqrt(rho/(pi*f*mu0))
- Effective dielectric constant (Wheeler formula)
- Conformal mapping for microstrip analysis
- Fourier analysis of digital signals

## L4: Fundamental Laws (Complete)
- Wheeler microstrip impedance formula
- IPC-2141 impedance equations
- Hammerstad-Jensen microstrip model
- Cohn stripline equations
- Telegrapher equations -> wave equation
- Characteristic impedance: Z0 = sqrt((R+jwL)/(G+jwC))
- S-parameter reciprocity: S12 = S21 (passive, reciprocal)
- S-parameter losslessness: |S11|^2 + |S21|^2 = 1
- NEXT saturation theorem: NEXT -> K_b*V for long lines
- FEXT proportionality: FEXT ~ length * frequency
- Stripline K_f = 0 (TEM mode cancellation)
- Microstrip K_f != 0 (non-TEM mode)
- Target impedance: Z_target = deltaV / I_transient
- Plane capacitance: C = eps0*er*A/d (Gauss law)
- Via inductance: L_via = (mu0*h/(2*pi))*ln(2h/r)
- Stub resonance: f_res = c0/(4*L_stub*sqrt(er))
- Skin effect depth formula
- Shannon-Hartley channel capacity relationship

## L5: Algorithms/Methods (Complete)
- Newton-Raphson trace width solver
- Binary search antipad optimization
- Frequency sweep PDN impedance analysis
- Decap selection optimization algorithm
- Cascade ABCD matrix multiplication
- S-parameter extraction from ABCD
- Coupled-line coupling parameter extraction
- Eye diagram analysis from time-domain waveform
- TDR impedance profile via inverse FFT
- Power-sum crosstalk (coherent + RMS)
- Stackup validation algorithm (7 checks)
- Back-drill depth decision algorithm
- Stitching via count calculation

## L6: Canonical Problems (Complete)
- DDR4-3200 4-layer PCB impedance design (50/100 Ohm)
- PCIe Gen4 16 GT/s crosstalk budget analysis
- DDR4 VDD/VTT PDN design with decap selection
- Step response of lossy transmission line
- Eye diagram opening analysis
- Differential via model for 10G+ signaling
- Multi-aggressor crosstalk power-sum analysis

## L7: Applications (Partial+)
- DDR4 SODIMM stackup design
- PCIe Gen4 link budget verification
- 5G NR FR2 (28 GHz) PCB design considerations

## L8: Advanced Topics (Partial)
- Roughness correction for mmWave frequencies (Hammerstad-Bekkadal)
- Frequency-dependent dielectric dispersion (Kirschning-Jansen)
- Advanced decoupling: 3-terminal and reverse-geometry capacitors

## L9: Research Frontiers (Partial - documented only)
- 6G sub-THz PCB materials (PTFE, Alumina, LCP)
- AI-driven PDN optimization
- Additive manufacturing for PCB stackups
