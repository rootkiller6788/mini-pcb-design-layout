# Knowledge Graph — mini-schematic-design-kicad

## L1: Definitions
- `pin_type_t` (11 variants: input, output, bidi, tristate, passive, unspecified, power_in, power_out, open_collector, open_emitter, no_connect)
- `pin_shape_t` (9 variants: line, inverted, clock, inverted_clock, input_low, clock_low, output_low, edge_falling, non_logic)
- `pin_orientation_t` (4 directions)
- `schematic_pin_t` (electrical pin with name, number, type, shape, position)
- `schematic_component_t` (symbol instance with reference, value, footprint, pins)
- `schematic_net_t` (electrical node with connections, power net flag)
- `schematic_sheet_t` (hierarchical sheet with border pins)
- `schematic_design_t` (complete schematic with components, nets, sheets)
- `schematic_label_t`, `schematic_wire_t`, `schematic_junction_t`, `schematic_noconnect_t` (graphical primitives)
- `schematic_layer_t` (14 layers)
- `bom_line_item_t`, `bom_report_t` (BOM data per IPC-2581)
- `bom_grouping_strategy_t` (6 strategies)
- `bom_output_format_t` (6 formats)
- `component_category_t` (20 EIA categories)
- `erc_violation_code_t` (16 violation types)
- `erc_severity_t` (4 levels)
- `erc_violation_t`, `erc_report_t` (ERC data structures)
- `netlist_net_t`, `netlist_data_t` (netlist interchange)
- `netlist_format_t` (6 formats)
- `spice_component_type_t`, `spice_analysis_type_t`, `spice_simulation_t` (SPICE types)
- `sexpr_token_type_t` (6 token types)
- `sexpr_token_t`, `sexpr_node_t`, `sexpr_tokenizer_t` (S-expression types)
- `incidence_matrix_t`, `adjacency_matrix_t` (graph theory matrices)
- `connected_component_t`, `connectivity_path_t`, `netlist_cycle_t`, `connectivity_metrics_t` (connectivity types)

## L2: Core Concepts
- Netlist as bipartite graph (components <-> nets)
- IEEE 1164 pin conflict resolution (12x12 compatibility matrix)
- BOM aggregation by equivalence relations
- SPICE circuit description language
- S-expression grammar for KiCad files
- Graph connectivity (connected components, paths, cycles)
- ERC rule taxonomy (16 violation types)
- Pin drive/receive classification
- PCB layer model (14 schematic layers)

## L3: Mathematical Structures
- CSR (Compressed Sparse Row) graph representation
- Incidence matrix B in {0,1}^(P x N): B[p][n] = 1 iff pin p on net n
- Adjacency matrix A = B x B^T: pin-pin connectivity
- MNA matrix: [G B; C D][v; w] = [i; e]
- Gaussian elimination with partial pivoting
- Equivalence relations for BOM grouping
- Pin-type compatibility algebra (12x12 matrix)

## L4: Fundamental Laws
- Kirchhoff Current Law (KCL): Sum(I_j) = 0 at each node
- Kirchhoff Voltage Law (KVL): Sum(V_loop) = 0
- IPC-2221 assembly cost: C = N_smd x c_smd + N_tht x c_tht + C_setup
- Graph connectivity theorem: DFS labels connected components
- Bridge theorem (Tarjan 1974): edge e is bridge iff low[v] > disc[u]

## L5: Algorithms
- Iterative DFS for connected components (O(V+E))
- BFS for shortest path (O(V+E))
- DFS back-edge cycle detection (O(V+E))
- Bridge-finding via Tarjan's algorithm (O(V+E))
- Greedy supplier optimization (O(I x S))
- Recursive descent S-expression parser (O(n))
- Multi-format BOM export (CSV, HTML, JSON, MD, XML, TSV)
- 6 netlist format writers (SPICE, IPC-D-356, KiCad, PADS, Allegro, EDIF)
- SPICE component line tokenizer
- MNA matrix construction
- Gaussian elimination with partial pivoting (O(n^3))

## L6: Canonical Problems
- Single-pin net detection (floating nodes)
- Driver conflict resolution (IEEE 1164)
- Unconnected input/power pin detection
- Duplicate reference detection
- Antenna loop identification (EMI)
- Star ground verification
- Critical net identification (bridge analysis)
- Manhattan routability check (planarity)
- Hierarchical pin connectivity verification
- Shorted power net detection
- Bus width consistency check

## L7: Applications
1. Automotive supply chain risk (Toyota lean, ISO 9001, Fukushima resilience)
2. Assembly cost estimation (IPC-2221, Detroit manufacturing, NASA J-STD-001)
3. EMI loop detection (CISPR 25, ISO 7637 automotive EMC)

## L8: Advanced Topics
1. Manhattan routability (planarity heuristic) — implemented
2. Monte Carlo tolerance analysis — not implemented
3. Bayesian reliability prediction — not implemented
4. Lyapunov power stability — not implemented
5. Time-varying netlist simulation — not implemented

## L9: Research Frontiers
- 6G RIS PCB design (documented)
- Quantum compilation for PCB routing (noted)
- Semantic schematic-to-layout ML (research direction)
