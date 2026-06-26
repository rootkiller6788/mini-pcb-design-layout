# Course Prerequisite Tree — mini-schematic-design-kicad

## Dependency Graph

```
schematic_core.c (L1-L2: Component/Pin/Net definitions)
├── schematic_netlist.c (L3-L5: Netlist extraction, SPICE, MNA)
│   └── Depends on: schematic_core.h
├── schematic_bom.c (L5-L7: BOM generation, export, supply chain)
│   └── Depends on: schematic_core.h
├── schematic_erc.c (L2-L6: Electrical Rules Check)
│   └── Depends on: schematic_core.h
├── schematic_connectivity.c (L3-L8: Graph theory, connectivity)
│   └── Depends on: schematic_core.h
└── schematic_sexpr.c (L5: S-expression parser, KiCad I/O)
    └── Depends on: schematic_core.h
```

## Prerequisite Knowledge Map

1. **schematic_core** → prerequisite: C struct/pointer basics, IEC 60617 pin types
2. **schematic_netlist** → prerequisite: schematic_core, SPICE format, KCL/KVL
3. **schematic_bom** → prerequisite: schematic_core, IPC standards, supply chain concepts
4. **schematic_erc** → prerequisite: schematic_core, IEEE 1164, design rule concepts
5. **schematic_connectivity** → prerequisite: schematic_core, graph theory (DFS/BFS), CSR format
6. **schematic_sexpr** → prerequisite: schematic_core, recursive descent parsing, Lisp S-expressions

## Required Background

| Topic | Course Equivalent |
|-------|------------------|
| C structs, pointers, dynamic memory | Any intro CS course |
| Graph theory (DFS, BFS, bridges) | MIT 6.042, CMU 15-251 |
| Circuit theory (KCL, KVL, nodal analysis) | MIT 6.002, Berkeley EE16A |
| SPICE simulation | Berkeley EE105 |
| BOM/PLM/ERP basics | Stanford EE272 |
| Parsing/compilers | MIT 6.035 |
| EMC/EMI fundamentals | ETH 227-0455 |
