# Course Tree - PCB Design for Manufacturing

## Prerequisites
- Mathematics: Statistics (Cp/Cpk), Calculus (optimization), Algebra (geometry)
- Materials Science: Thermal conductivity, CTE, Young's modulus
- Circuit Theory: Voltage, current, power dissipation
- Programming: C data structures, memory management

## Module Dependencies

### Upstream (needed by this module)
1. Basic statistics: mean, variance, normal distribution
2. Calculus: derivatives, integrals for yield models
3. Materials science: k, CTE, E for substrates
4. Circuit theory: I, V, P for thermal/current calculations

### Downstream (depends on this module)
1. PCB Layout: Applies DFM rules during routing
2. Signal Integrity: Uses material Dk, Df
3. Power Integrity: Uses copper weight, thermal
4. Thermal Simulation: Uses R_thermal models
5. Manufacturing Planning: Uses cost and yield models
6. Quality Engineering: Uses process capability metrics

## Learning Path

### Phase 1: Foundations (L1-L2)
- IPC classification, Substrate materials, Surface finishes
- Design rules: What they are and why they exist

### Phase 2: Theory (L3-L4)
- Statistical process control: Cp, Cpk
- Yield models: Poisson through Negative Binomial
- Thermal analysis: Conduction, resistance networks
- Cost models: Learning curves, tolerance allocation

### Phase 3: Application (L5-L6)
- Running DRC checks programmatically
- Optimizing panel utilization
- Estimating manufacturing cost
- Predicting yield from design parameters

### Phase 4: Integration (L7-L9)
- Full DFM workflow for real designs
- Advanced topics: HDI, RF materials, additive manufacturing
- Research frontiers: AI-driven DFM, new materials