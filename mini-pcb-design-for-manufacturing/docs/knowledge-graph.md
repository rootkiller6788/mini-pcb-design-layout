# Knowledge Graph - PCB Design for Manufacturing

## L1: Definitions
- IPC Class 1/2/3, Substrate materials (8 types), Surface finishes (8 types)
- Copper weight conversion, Via types (6), Gerber layers (14), Fiducial marks (3)
- Design rules: trace width, spacing, annular ring, solder mask, silkscreen, drill, edge, thermal relief
- Panelization: V-score, tab-routing, mouse-bites, tooling holes, copper thieving

## L2: Core Concepts
- Process capability (Cp, Cpk, DPMO), OEE (A*P*Q)
- DRC violation management, Thermal resistance network
- Copper balance for warpage prevention, Etching factor
- Solder mask registration, Creepage vs clearance

## L3: Mathematical Structures
- Normal CDF approximation, Poisson process, compound distributions
- Monte Carlo simulation (LCG), Tolerance-cost power law
- Wright learning curve, Fourier heat conduction, Rule of mixtures

## L4: Fundamental Laws
- Poisson yield: Y=exp(-A*D)
- Murphy yield: Y=((1-exp(-AD))/(AD))^2
- Seeds yield: Y=exp(-sqrt(AD))
- Negative Binomial: Y=(1+AD/alpha)^(-alpha)
- IPC-2221 voltage-spacing, IPC-2152 current capacity
- Timoshenko bimetal warpage model

## L5: Algorithms
- DRC (9 categories), Tolerance allocation (3 methods)
- 2D bin packing panel optimization, Thermal via design
- Monte Carlo yield (6 models), Layer count optimization
- Manufacturing complexity index

## L6: Canonical Problems
- Panel utilization maximization, PCB cost estimation
- Yield prediction for multi-up panels, Thermal management design

## L7: Applications
- IoT Gateway (Class 2, consumer), Automotive ECU (Class 3), Aerospace (Class 3)

## L8: Advanced Topics
- HDI microvia design, High-frequency/RF DFM, Statistical process control

## L9: Research Frontiers
- Additive PCB manufacturing, AI-driven DFM, Advanced materials