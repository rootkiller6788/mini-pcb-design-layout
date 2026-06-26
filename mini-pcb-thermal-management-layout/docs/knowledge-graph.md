# Knowledge Graph - PCB Thermal Management Layout

## L1: Definitions (Complete)

20 core type definitions covering: heat transfer modes, PCB materials (10 types), IC packages (12 types), cooling solutions (10 types), PCB layers, copper weights, 3D coordinates, thermal resistance networks, PCB stack-ups, thermal via geometry, heat sources, copper pours, heat sinks, TIM properties, ambient conditions, temperature fields, error codes, convection correlations, and material properties (22 canonical materials).

## L2: Core Concepts (Complete)

14 core concepts: Fourier conduction, Newton convection, Stefan-Boltzmann radiation, Kennedy spreading model, series/parallel thermal resistance, junction temperature network solver, mutual heating, natural/forced convection Nusselt correlations, heat sink fin efficiency analysis, material property database, PCB stack effective conductivity, copper layer effective k, and convection estimation tables.

## L3: Mathematical Structures (Complete)

10 mathematical structures: 1D heat equation (Fourier), 2D Poisson equation (FDM 5-point stencil), Foster RC thermal network, Cauer RC ladder (physical 1D model), lumped capacitance model, Biot number criterion, Fourier number (transient penetration), characteristic spreading length, thermal impedance Zth(t), and thermal time constant.

## L4: Fundamental Laws (Complete)

8 fundamental laws verified by implementation and testing:
- Fourier's Law: q = -k dT/dx
- Newton's Law of Cooling: q = h A (Ts - Tinf)
- Stefan-Boltzmann Radiation: q = epsilon sigma (Ts^4 - Tinf^4)
- Conservation of Energy: div(k grad T) + qdot = 0
- Series Thermal Resistance: R = R1 + R2
- Parallel Thermal Resistance: 1/R = 1/R1 + 1/R2
- Arrhenius Lifetime Equation: AF = exp((Ea/k)(1/Tu - 1/Ts))
- Kirchhoff-type Nodal Thermal Analysis: G*T = Q

## L5: Algorithms/Methods (Complete)

12 algorithms: Gauss-Seidel iteration (2D FDM), SOR acceleration, optimal omega calculation, network Gauss-Seidel solver, forward Euler integration (Cauer), binary search pour sizing, exhaustive grid search via optimization, iterative via count, constrained material selection, natural convection Nusselt, forced convection Nusselt, and fin efficiency method.

## L6: Canonical Problems (Complete)

6 canonical problems: LDO copper pour design, MOSFET heatsink selection, thermal via array optimization, 2D FDM simulation with multiple heat sources, complete cooling solution design flow, and junction temperature network analysis.

## L7: Applications (Complete - 3+ applications)

Applications include: GPS receiver LDO thermal design, DC motor drive MOSFET cooling, GaN power transistor via design, Arrhenius reliability estimation (MIL-HDBK-217F), and thermal runaway detection for parallel power devices.

## L8: Advanced Topics (Complete - 2 topics)

Cauer RC ladder transient analysis (1D physical heat diffusion model), and thermal runaway stability analysis for parallel MOSFETs with imbalance detection.

## L9: Research Frontiers (Partial)

GaN/SiC wide-bandgap semiconductor thermal management documented in material database (AlN, BeO) and examples. Thermal-aware PCB design automation flow covered.

---

*Status: L1-L6 Complete, L7 Complete (5 applications), L8 Complete (2 topics), L9 Partial*
