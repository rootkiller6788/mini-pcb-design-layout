# Course Alignment — mini-schematic-design-kicad

| University | Course | Topic | Implementation |
|------------|--------|-------|----------------|
| MIT | 6.002 Circuits and Electronics | Component/pin/connection model, KCL, KVL | `schematic_core.h/.c` |
| MIT | 6.003 Signal Processing | Circuit topology, nodal analysis | `schematic_netlist.c` (MNA) |
| MIT | 6.004 Computation Structures | Digital pin conflict detection | `schematic_erc.c` (IEEE 1164) |
| MIT | 6.035 Compiler Design | Recursive descent parsing | `schematic_sexpr.c` |
| MIT | 6.042 Mathematics for CS | Graph theory, DFS/BFS, bridges | `schematic_connectivity.c` |
| Stanford | EE272 Design for Manufacturing | BOM management, procurement | `schematic_bom.c` |
| Stanford | EE313 Digital Systems | Netlist connectivity | `schematic_connectivity.c` |
| Berkeley | EE16A/B Designing Information Devices | Schematic topology | `schematic_core.c` |
| Berkeley | EE105 Microelectronic Devices | SPICE netlist | `schematic_netlist.c` |
| Berkeley | EE141 Digital Integrated Circuits | ERC in design flow | `schematic_erc.c` |
| Berkeley | EE219 Circuit Theory | Topology analysis | `schematic_connectivity.c` |
| CMU | 15-251 Great Ideas in Theoretical CS | Graph algorithms | `schematic_connectivity.c` |
| ETH | 227-0427 Signal Processing | Circuit analysis | `schematic_netlist.c` |
| ETH | 227-0455 EMC | EMI-aware design | `schematic_connectivity.c` (antenna loops) |
| TU Munich | High-Frequency Engineering | RF circuit connectivity | `schematic_connectivity.c` |
| TU Munich | Automotive Electronics | ERC standards, ISO 26262 | `schematic_erc.c` |
| Georgia Tech | ECE 6350 EM Applications | System integration | `schematic_bom.c` |
| Michigan | EECS 411 Microwave Circuits | Component selection | `schematic_bom.c` |
| Cambridge | 3B6 Electronic Design | Schematic capture fundamentals | `schematic_core.c` |
| Tsinghua | Signal and System | Network analysis | `schematic_netlist.c` (MNA) |
