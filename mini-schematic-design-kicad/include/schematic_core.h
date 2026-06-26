/**
 * @file schematic_core.h
 * @brief Core data structures for electronic schematic representation
 *
 * Defines the foundational types for representing electronic schematics
 * in a CAD-agnostic intermediate format, supporting KiCad S-expression
 * import/export, netlist generation, and ERC analysis.
 *
 * Course Mapping:
 *   - MIT 6.002 Circuits — component/pin/connection model
 *   - Berkeley EE16A/B — schematic topology representation
 *   - Stanford EE272 — hierarchical design structure
 *   - Cambridge 3B6 — schematic capture fundamentals
 *
 * Reference: KiCad Eeschema Documentation v8.0
 */

#ifndef SCHEMATIC_CORE_H
#define SCHEMATIC_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ──────────── L1 Definitions: Core Types ──────────── */

/** Electrical pin type classification per IEC 60617 */
typedef enum {
    PIN_TYPE_INPUT        = 0,
    PIN_TYPE_OUTPUT       = 1,
    PIN_TYPE_BIDI         = 2,
    PIN_TYPE_TRISTATE     = 3,
    PIN_TYPE_PASSIVE      = 4,
    PIN_TYPE_UNSPECIFIED  = 5,
    PIN_TYPE_POWER_IN     = 6,
    PIN_TYPE_POWER_OUT    = 7,
    PIN_TYPE_OPEN_COLLECTOR = 8,
    PIN_TYPE_OPEN_EMITTER = 9,
    PIN_TYPE_NO_CONNECT   = 10
} pin_type_t;

/** Electrical pin shape / graphical style */
typedef enum {
    PIN_SHAPE_LINE        = 0,
    PIN_SHAPE_INVERTED    = 1,
    PIN_SHAPE_CLOCK       = 2,
    PIN_SHAPE_INVERTED_CLOCK = 3,
    PIN_SHAPE_INPUT_LOW   = 4,
    PIN_SHAPE_CLOCK_LOW   = 5,
    PIN_SHAPE_OUTPUT_LOW  = 6,
    PIN_SHAPE_EDGE_FALLING = 7,
    PIN_SHAPE_NON_LOGIC   = 8
} pin_shape_t;

/** Pin orientation for schematic placement */
typedef enum {
    PIN_ORIENT_RIGHT = 0,
    PIN_ORIENT_LEFT  = 1,
    PIN_ORIENT_UP    = 2,
    PIN_ORIENT_DOWN  = 3
} pin_orientation_t;

/**
 * @brief Single electrical pin in a component symbol
 *
 * A pin represents one electrical terminal of a component.
 * Each pin has a local side number, a name, an electrical type,
 * and a graphical shape for proper schematic rendering.
 */
typedef struct {
    char     name[32];
    char     number[8];
    pin_type_t   electrical_type;
    pin_shape_t  shape;
    pin_orientation_t orientation;
    double   length;
    int      pos_x;
    int      pos_y;
    bool     is_visible;
    bool     is_power;
} schematic_pin_t;

/**
 * @brief Component symbol definition (L1: symbol representation)
 *
 * Components in a schematic are instances of symbols that define
 * their electrical interface. Each symbol has a reference designator,
 * value string, footprint assignment, and a list of pins.
 */
typedef struct {
    char     reference[16];
    char     value[64];
    char     footprint[64];
    char     library_id[128];
    char     datasheet[256];
    double   pos_x;
    double   pos_y;
    double   rotation;
    bool     is_mirrored_x;
    bool     is_mirrored_y;
    bool     is_power_symbol;
    bool     exclude_from_bom;
    bool     exclude_from_board;
    int      unit;
    int      num_pins;
    schematic_pin_t *pins;
} schematic_component_t;

/**
 * @brief Net / wire connection between component pins
 *
 * A net is the electrical node connecting two or more component pins.
 * In KiCad parlance, this is the basic electrical connection.
 * Each net has a unique name (either user-assigned or auto-generated).
 */
typedef struct {
    char     name[128];
    int      net_code;
    int      num_connections;
    struct net_connection {
        char ref[16];
        char pin[8];
        int  pin_index;
    } *connections;
    bool     is_power_net;
    char     net_class[32];
} schematic_net_t;

/**
 * @brief Hierarchical sheet definition
 *
 * Multi-sheet schematics use hierarchical sheets to encapsulate
 * sub-circuits. Each sheet has border pins that connect to the
 * parent schematic via hierarchical labels.
 */
typedef struct {
    char     name[64];
    char     filename[256];
    int      num_pins;
    struct sheet_pin {
        char name[32];
        char shape[8];
        int  pos_x;
        int  pos_y;
    } *pins;
    double   pos_x;
    double   pos_y;
    double   width;
    double   height;
} schematic_sheet_t;

/**
 * @brief Schematic design — complete representation
 */
typedef struct {
    char     title[128];
    char     date[32];
    char     rev[16];
    char     company[64];
    char     comment1[128];
    char     comment2[128];
    char     comment3[128];
    int      num_components;
    schematic_component_t *components;
    int      num_nets;
    schematic_net_t *nets;
    int      num_sheets;
    schematic_sheet_t *sheets;
} schematic_design_t;

/* ──────────── Label/Wire/Junction Types ──────────── */

typedef enum {
    LABEL_LOCAL       = 0,
    LABEL_GLOBAL      = 1,
    LABEL_HIERARCHICAL = 2,
    LABEL_NETCLASS    = 3
} label_type_t;

typedef struct {
    char     text[256];
    label_type_t type;
    double   pos_x;
    double   pos_y;
    double   rotation;
    double   text_size;
    bool     is_italic;
    bool     is_bold;
} schematic_label_t;

typedef struct {
    double   x1, y1;
    double   x2, y2;
    int      width;
} schematic_wire_t;

typedef struct {
    double   pos_x;
    double   pos_y;
    double   diameter;
} schematic_junction_t;

typedef struct {
    double   pos_x;
    double   pos_y;
} schematic_noconnect_t;

/* ──────────── L3: Graph Theory Structures ──────────── */

/**
 * @brief Adjacency list representation of netlist connectivity
 *
 * Models the schematic as an undirected graph where vertices are
 * component pins and edges are net connections. This enables
 * graph-theoretic analysis: connected components, cycles, cuts.
 *
 * Uses Compressed Sparse Row (CSR) format for memory efficiency
 * and fast BFS/DFS traversals.
 */
typedef struct {
    int      num_vertices;
    int      num_edges;
    int     *adjacency_offsets;
    int     *adjacency_targets;
    char   **vertex_refs;
    char   **vertex_pins;
} netlist_graph_t;

/* ──────────── Layer Constants ──────────── */

typedef enum {
    LAYER_WIRE          = 0,
    LAYER_BUS           = 1,
    LAYER_JUNCTION      = 2,
    LAYER_NOCONNECT     = 3,
    LAYER_LOCAL_LABEL   = 4,
    LAYER_GLOBAL_LABEL  = 5,
    LAYER_HIER_LABEL    = 6,
    LAYER_TEXT           = 7,
    LAYER_NOTES          = 8,
    LAYER_SHEET          = 9,
    LAYER_SHEET_PIN      = 10,
    LAYER_SHEET_BORDER   = 11,
    LAYER_ERC_WARNING    = 12,
    LAYER_ERC_ERROR      = 13
} schematic_layer_t;

/* ──────────── Core API ──────────── */

/** Initialize an empty schematic design */
schematic_design_t* schematic_design_create(const char *title);
void schematic_design_free(schematic_design_t *sch);

/** Add component, returns component index or -1 on error */
int schematic_add_component(schematic_design_t *sch,
                            const char *ref, const char *value,
                            const char *footprint, const char *lib_id);

/** Add a pin to an existing component */
int schematic_component_add_pin(schematic_component_t *comp,
                                const char *name, const char *num,
                                pin_type_t etype);

/** Add a net connection between component pins */
int schematic_add_net(schematic_design_t *sch, const char *name, int net_code);
bool schematic_connect_pin(schematic_design_t *sch,
                           const char *ref, const char *pin_name,
                           const char *net_name);

int schematic_total_connections(const schematic_design_t *sch);
int schematic_total_pins(const schematic_design_t *sch);

/** Validate reference designators (no duplicates, valid format) */
int schematic_validate_references(const schematic_design_t *sch,
                                   char *errors, size_t err_size);

/** Validate that all power pins are connected */
int schematic_check_power_connectivity(const schematic_design_t *sch);

int schematic_find_component(const schematic_design_t *sch, const char *ref);
int schematic_find_net(const schematic_design_t *sch, const char *net_name);

/** Build adjacency graph from schematic netlist */
netlist_graph_t* schematic_build_graph(const schematic_design_t *sch);
void netlist_graph_free(netlist_graph_t *g);

/** DFS connected components on netlist graph */
int netlist_graph_connected_components(const netlist_graph_t *g,
                                        int *component_id);

/** BFS shortest path between two vertices */
int netlist_graph_shortest_path(const netlist_graph_t *g,
                                 int start_vertex, int end_vertex,
                                 int *path, int max_path_len);

bool schematic_has_floating_nets(const schematic_design_t *sch);

#endif /* SCHEMATIC_CORE_H */