# Knowledge Graph — mini-component-placement-strategy

## L1: Definitions (Complete)
| Term | Definition | Code Artifact |
|------|-----------|---------------|
| Component | Physical electronic part placed on PCB | `Component` struct (placement_core.h) |
| ComponentCategory | Functional classification (passive, active, analog IC, digital IC, power, connector, RF, crystal, sensor, ESD, test) | `ComponentCategory` enum (12 types) |
| PackageType | SMD/THT package form factor | `PackageType` enum (31 types from 0201 to BGA-484) |
| PlacementDomain | Signal domain for functional separation | `PlacementDomain` enum (7 domains) |
| MountType | Component mounting location and method | `MountType` enum (4 types) |
| Point2D | 2D coordinate in board space (mm) | `Point2D` struct |
| Rect2D | 2D rectangle (footprint/body outline) | `Rect2D` struct |
| Envelope3D | 3D component volume (footprint + height) | `Envelope3D` struct |
| BoundingBox | Axis-aligned bounding box | `BoundingBox` struct |
| Pad | Solder land on a component | `Pad` struct |
| Net | Electrical connection between pins | `Net` struct |
| Board | Printed circuit board definition | `Board` struct |
| BoardLayer | PCB stackup layer | `BoardLayer` struct |
| PlacementResult | Complete placement configuration | `PlacementResult` struct |
| PlacementCost | Multi-objective cost decomposition | `PlacementCost` struct |
| ConstraintType | DRC constraint categories | `ConstraintType` enum (8 types) |
| Violation | Constraint violation record | `Violation` struct |
| IPCDensityLevel | IPC-2221 design density level | `IPCDensityLevel` enum (A/B/C) |
| KeepOutZone | Restricted placement area | `KeepOutZone` struct |
| ThermalNode | Thermal resistance network node | `ThermalNode` struct |
| ThermalEdge | Thermal conductance path | `ThermalEdge` struct |
| ThermalNetwork | Complete thermal circuit | `ThermalNetwork` struct |
| ThermalVia | Thermal via specification | `ThermalVia` struct |

## L2: Core Concepts (Complete)
| Concept | Description | Implementation |
|---------|-------------|----------------|
| Functional Grouping | Components grouped by signal domain | domain field in Component |
| Analog/Digital Separation | Physical separation to prevent coupling | DOMAIN_ANALOG / DOMAIN_DIGITAL |
| Power Domain Isolation | High-current paths kept separate | DOMAIN_POWER |
| RF Isolation | RF components shielded from digital | DOMAIN_RF |
| Signal Flow Priority | Placement ordered by signal path | Priority field in Component |
| Component Rotation | 90-degree rotation snapping | rotation field + set_position |
| Half-Perimeter Wirelength | HPWL estimation metric | placement_estimate_wire_length() |
| Board Stackup | Multi-layer PCB construction | BoardLayer + board_add_layer |
| Placement Grid | Regular placement grid | grid_x_mm / grid_y_mm |
| IPC Design Rules | Industry-standard PCB design rules | IPC spacing, density levels |
| Thermal Management | Heat dissipation through placement | Thermal cost + thermal network |
| Signal Integrity | High-speed signal constraints | Critical net checking |

## L3: Mathematical Structures (Complete)
| Structure | Description | Implementation |
|-----------|-------------|----------------|
| 2D Euclidean Geometry | Point distance, rectangle overlap | placement_util_distance, rect_overlap |
| Rotation Matrix | 2D rotation of component bodies | placement_component_get_bounds |
| HPWL Metric | (max_x-min_x)+(max_y-min_y) for each net | placement_cost_hpwl() |
| Rectilinear Steiner Tree | RST approximation via MST*0.85 | placement_cost_steiner() |
| Thermal Resistance Network | G*T = P linear system | placement_thermal_solve_steady_state |
| Gaussian Elimination | Linear system solver with pivoting | Thermal solver implementation |
| Convex Hull (Graham Scan) | Minimum bounding polygon | placement_util_convex_hull() |
| Quadtree Spatial Index | O(log N) spatial queries | QuadTree struct + build/query |
| Pareto Optimality | Multi-objective non-dominance | ParetoFront + dominates/insert |
| Hypervolume Indicator | S-metric for Pareto front quality | placement_pareto_hypervolume() |
| Linear Regression | y = a*x + b fitting | placement_util_linear_regression |
| Pearson Correlation | Statistical correlation coefficient | placement_util_correlation |
| Box-Muller Transform | Gaussian random variable generation | placement_util_random_gaussian |

## L4: Fundamental Laws (Complete)
| Law/Theorem | Formula | Code Verification |
|-------------|---------|-------------------|
| Fourier's Law (Heat Conduction) | q = -k * grad(T) | Thermal spreading resistance calculation |
| Newton's Law of Cooling | q = h * A * (T_s - T_amb) | Convection boundary in thermal network |
| Joule Heating | P = I^2 * R | Power dissipation in components |
| IPC-2221 Spacing Rules | min_spacing = f(package_size, density_level) | placement_constraint_get_ipc_spacing() |
| IPC-7351 Courtyard | Land pattern + courtyard excess | Spacing matrix implementation |
| Metropolis Criterion | P(accept) = exp(-deltaE / T) | Simulated Annealing acceptance |
| Hooke's Law (Spring Force) | F = k * (d - L_ideal) | Force-directed spring attractive force |
| Coulomb Repulsion | F = K_r / d^2 | Force-directed electrical repulsion |
| Critical Length Rule | L_max = t_rise / (2 * t_pd) | Trace length constraint check |
| Differential Pair Matching | delta_L < t_rise * v / (10 * epsilon_r) | Diff pair constraint check |
| Thermal Spreading (Yovanovich) | R_spread = psi / (4 * k * a) | placement_thermal_spreading_resistance |
| Heat Equation Green's Function | Delta_T(r) = P * K0(r/L) / (pi * k * t) | placement_thermal_temperature_at() |

## L5: Algorithms/Methods (Complete)
| Algorithm | Complexity | Implementation |
|-----------|------------|----------------|
| Greedy Sequential Placement | O(C^2 * P) | placement_strategy_greedy() |
| Simulated Annealing (Metropolis) | O(M * C^2) | placement_strategy_simulated_annealing() |
| Force-Directed Placement (Fruchterman-Reingold) | O(I * C^2) | placement_strategy_force_directed() |
| Fiduccia-Mattheyses Min-Cut | O(P) per pass | placement_strategy_partition_bisection() |
| Genetic Algorithm (GA) | O(G * P * C^2) | placement_strategy_genetic_algorithm() |
| K-Means Clustering | O(I * K * C) | placement_strategy_clustering() |
| Prim's MST Algorithm | O(N^2) | mst_length() in Steiner estimator |
| Graham Scan (Convex Hull) | O(N log N) | placement_util_convex_hull() |
| Quadtree Build/Query | O(C log C) / O(log C) | placement_util_quadtree_* |
| LCG Random Number Generator | O(1) | placement_util_random_* |
| Box-Muller Gaussian Sampler | O(1) | placement_util_random_gaussian |
| Gaussian Elimination (Pivot) | O(N^3) | placement_thermal_solve_steady_state |

## L6: Canonical Problems (Complete)
| Problem | Description | Example/Demo |
|---------|-------------|--------------|
| Mixed-Signal PCB Placement | Analog/digital domain separation with greedy strategy | example_greedy.c |
| Power Electronics Thermal Management | Hot component separation via SA | example_sa.c |
| Thermal Via Optimization | Hot spot mitigation through via placement | example_thermal.c |
| Multi-Strategy Comparison | Side-by-side evaluation of 6 placement algorithms | demo_placement.c |
| Constraint-Driven Placement | IPC-compliant spacing and boundary checking | tests/test_placement.c |
| Multi-Objective Pareto Optimization | Trade-off between HPWL, thermal, SI | Pareto front in optimizer |

## L7: Applications (Partial+ - 4 applications)
| Application | Domain | Evidence |
|-------------|--------|----------|
| PCB Assembly Pick-and-Place | Electronics manufacturing | CSV export to industry-standard format |
| Automotive ECU Placement | Detroit/Toyota automotive | Demo with thermal constraints for engine compartment |
| Smartphone PCB Layout | iPhone consumer electronics | High-density IPC Level C support |
| Power Supply Thermal Design | Industrial/power electronics | Thermal via optimization for DC-DC converters |
| Avionics PCB | Boeing 787 aerospace | High-reliability IPC Level A with wide margins |

## L8: Advanced Topics (Partial - 2 topics)
| Topic | Description | Implementation |
|-------|-------------|----------------|
| Multi-Objective Pareto Optimization | NSGA-II inspired non-dominated sorting | ParetoFront + hypervolume |
| Time-Varying Thermal Analysis | Transient heating/cooling | Thermal capacitance in ThermalNode |
| Stochastic Placement Optimization | Monte Carlo acceptance in SA | Metropolis criterion |
| Lyapunov-Stable Force Model | Convergence guarantees for FD placement | Damping factor ensures stability |

## L9: Research Frontiers (Partial - documented)
| Topic | Description |
|-------|-------------|
| AI/ML-Based Placement | Reinforcement learning for component placement |
| 6G mmWave PCB Layout | Antenna-in-package placement optimization |
| Quantum Annealing Placement | Quadratic unconstrained binary optimization for placement |
| Digital Twin Thermal Simulation | Real-time thermal simulation for PCB digital twins |