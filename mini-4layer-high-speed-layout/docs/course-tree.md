# Course Tree - mini-4layer-high-speed-layout

## Prerequisites (What You Need Before This Module)

```
mini-4layer-high-speed-layout
|
+-- [PREREQ] mini-signal-system-theory
|   +-- Fourier analysis
|   +-- Convolution
|   +-- Bandwidth concepts
|
+-- [PREREQ] mini-circuit-analysis
|   +-- Ohm's Law
|   +-- Kirchhoff's Laws
|   +-- RLC circuits
|   +-- Impedance concept
|
+-- [PREREQ] mini-electromagnetic-wave
|   +-- Maxwell's equations
|   +-- Wave propagation
|   +-- Skin effect
|   +-- Transmission line theory (Telegrapher's equations)
|
+-- [PREREQ] mini-analog-electronics
    +-- Frequency response
    +-- Bode plots
    +-- Feedback and stability
```

## Knowledge Dependencies Within This Module

```
hs_stackup (L1-L2: materials, layers)
  |
  +-> hs_impedance (L3-L5: uses stackup geometry for Z0 calc)
  |     |
  |     +-> hs_transmission (L3-L5: uses Z0 for TL analysis)
  |     |     |
  |     |     +-> hs_crosstalk (L4-L6: uses Z0, TL params for coupling)
  |     |
  |     +-> hs_pdn (L3-L5: uses plane capacitance, spreading L)
  |
  +-> hs_via (L3-L6: uses barrel geometry, pad dimensions)
```

## Postrequisites (What Builds on This Module)

```
mini-4layer-high-speed-layout
|
+-- [POSTREQ] mini-emc-signal-integrity
|   +-- Full-board SI analysis
|   +-- EMI compliance testing
|   +-- Advanced crosstalk mitigation
|
+-- [POSTREQ] mini-wireless-mobile-comm
|   +-- RF PCB design for antennas
|   +-- mmWave interconnect design
|
+-- [POSTREQ] mini-electronic-mfg-test
    +-- DFM (Design for Manufacturing)
    +-- Impedance TDR testing
    +-- ICT/FCT test point placement
```
