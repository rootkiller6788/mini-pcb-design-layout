/**
 * @file placement_core.h
 * @brief Core data structures for PCB component placement strategy
 *
 * Defines the fundamental types and operations used throughout the
 * component placement system. Every component, board, net, and
 * placement result is represented here.
 *
 * Knowledge Mapping:
 *   L1 (Definitions): Component types, package types, placement grid,
 *                     footpoint dimensions, bounding box, centroid
 *   L2 (Core Concepts): Functional grouping, signal flow priority,
 *                       analog/digital/power domain separation
 *
 * Course Alignment:
 *   - MIT 6.003: Signal integrity concepts in placement
 *   - Berkeley EE105: Analog circuit layout considerations
 *   - Georgia Tech ECE 6350: EM-aware placement
 *
 * References:
 *   - IPC-7351: Generic Requirements for Surface Mount Design and Land Pattern Standard
 *   - IPC-2221: Generic Standard on Printed Board Design
 */

#ifndef PLACEMENT_CORE_H
#define PLACEMENT_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1 Definitions: Component Types & Classifications
 * ============================================================================ */

/** Component functional category for domain-aware placement */
typedef enum {
    COMP_CAT_PASSIVE      = 0,  /* Resistor, capacitor, inductor */
    COMP_CAT_ACTIVE       = 1,  /* Transistor, diode */
    COMP_CAT_ANALOG_IC    = 2,  /* Op-amp, voltage regulator, ADC/DAC */
    COMP_CAT_DIGITAL_IC   = 3,  /* MCU, FPGA, logic gates, memory */
    COMP_CAT_POWER        = 4,  /* Power MOSFET, LDO, DC-DC converter */
    COMP_CAT_CONNECTOR    = 5,  /* Header, USB, RJ45, edge connector */
    COMP_CAT_ELECTROMECH  = 6,  /* Relay, switch, transformer, buzzer */
    COMP_CAT_RF           = 7,  /* Antenna, balun, SAW filter, PA, LNA */
    COMP_CAT_CRYSTAL_OSC  = 8,  /* Crystal oscillator, TCXO, OCXO */
    COMP_CAT_SENSOR       = 9,  /* Temperature, IMU, light, hall effect */
    COMP_CAT_ESD_PROTECT  = 10, /* TVS diode, varistor, ESD array */
    COMP_CAT_DEBUG_TEST   = 11, /* Test point, debug header, JTAG */
    COMP_CAT_COUNT        = 12
} ComponentCategory;

/** Package/SMD type classification affecting placement strategy */
typedef enum {
    PKG_SMD_0201 = 0,   /* 0.6 x 0.3 mm  — ultra-miniature */
    PKG_SMD_0402 = 1,   /* 1.0 x 0.5 mm  — miniature */
    PKG_SMD_0603 = 2,   /* 1.6 x 0.8 mm  — common passive */
    PKG_SMD_0805 = 3,   /* 2.0 x 1.25 mm — common passive */
    PKG_SMD_1206 = 4,   /* 3.2 x 1.6 mm  — power passive */
    PKG_SMD_1210 = 5,   /* 3.2 x 2.5 mm  — larger passive */
    PKG_SMD_1812 = 6,   /* 4.5 x 3.2 mm  — power passive */
    PKG_SMD_2512 = 7,   /* 6.3 x 3.2 mm  — high power */
    PKG_SOT_23   = 8,   /* Small outline transistor, 3-pin */
    PKG_SOT_223  = 9,   /* SOT with tab, power transistor */
    PKG_SOIC_8   = 10,  /* 1.27mm pitch, 8-pin */
    PKG_SOIC_16  = 11,  /* 1.27mm pitch, 16-pin */
    PKG_TSSOP_16 = 12,  /* 0.65mm pitch, thin shrink */
    PKG_TSSOP_20 = 13,  /* 0.65mm pitch */
    PKG_QFP_32   = 14,  /* Quad flat pack, 0.8mm pitch */
    PKG_QFP_64   = 15,  /* Quad flat pack */
    PKG_QFP_100  = 16,  /* Quad flat pack */
    PKG_QFN_16   = 17,  /* Quad flat no-lead, exposed pad */
    PKG_QFN_32   = 18,  /* Quad flat no-lead */
    PKG_QFN_48   = 19,  /* Quad flat no-lead */
    PKG_BGA_64   = 20,  /* Ball grid array, 8x8 */
    PKG_BGA_100  = 21,  /* Ball grid array, 10x10 */
    PKG_BGA_256  = 22,  /* Ball grid array, 16x16 */
    PKG_BGA_484  = 23,  /* Ball grid array, 22x22 */
    PKG_TO_220   = 24,  /* Through-hole power transistor */
    PKG_TO_247   = 25,  /* Larger through-hole power */
    PKG_DIP_8    = 26,  /* Dual inline package */
    PKG_DIP_16   = 27,  /* Dual inline package */
    PKG_SMA      = 28,  /* SMA diode package */
    PKG_SMB      = 29,  /* SMB diode package */
    PKG_SC_70    = 30,  /* SC-70, small SMD transistor */
    PKG_COUNT    = 31
} PackageType;

/** Placement domain for functional separation */
typedef enum {
    DOMAIN_DIGITAL         = 0,
    DOMAIN_ANALOG          = 1,
    DOMAIN_POWER           = 2,
    DOMAIN_RF              = 3,
    DOMAIN_MIXED_SIGNAL    = 4,
    DOMAIN_HIGH_VOLTAGE    = 5,
    DOMAIN_LOW_NOISE       = 6,
    DOMAIN_COUNT           = 7
} PlacementDomain;

/** Mounting technology */
typedef enum {
    MOUNT_SMD_TOP     = 0,  /* Surface-mount, top layer */
    MOUNT_SMD_BOTTOM  = 1,  /* Surface-mount, bottom layer */
    MOUNT_THT_TOP     = 2,  /* Through-hole, inserted from top */
    MOUNT_THT_BOTTOM  = 3   /* Through-hole, inserted from bottom */
} MountType;

/* ============================================================================
 * L1 Definitions: Core Geometric Structures
 * ============================================================================ */

/** 2D point in placement space (millimeters) */
typedef struct {
    double x;  /* X-coordinate in mm, origin at board bottom-left */
    double y;  /* Y-coordinate in mm */
} Point2D;

/** 2D rectangle representing component body outline */
typedef struct {
    Point2D origin;  /* Bottom-left corner */
    double width;    /* Width in mm (X-direction) */
    double height;   /* Height in mm (Y-direction) */
} Rect2D;

/** 3D envelope for height-aware placement (in mm) */
typedef struct {
    Rect2D footprint;  /* 2D footprint on PCB surface */
    double z_min;      /* Minimum Z above board (standoff) */
    double z_max;      /* Maximum Z above board (component height) */
} Envelope3D;

/** Bounding box for groups of components */
typedef struct {
    double x_min, y_min;
    double x_max, y_max;
} BoundingBox;

/* ============================================================================
 * L1 Definitions: Pad, Pin, and Net Structures
 * ============================================================================ */

/** Single pad/solder-land on a component */
typedef struct {
    uint32_t pin_number;    /* Pin number (1-indexed per IPC convention) */
    char     pin_name[16];  /* Signal name (e.g., "VCC", "GND", "GPIO1") */
    Point2D  offset;        /* Offset from component origin to pad center */
    double   pad_width;     /* Pad width in mm */
    double   pad_height;    /* Pad height in mm */
    bool     is_thermal_pad; /* True if this pad is a thermal pad */
} Pad;

/** Single electrical net connecting multiple component pins */
typedef struct {
    uint32_t net_id;           /* Unique net identifier */
    char     net_name[32];     /* Net name from schematic */
    uint32_t pin_count;        /* Number of pins on this net */
    bool     is_critical;      /* Critical net (impedance-controlled, differential) */
    bool     is_power_net;     /* Power distribution net */
    double   target_impedance; /* Target characteristic impedance (0 = not controlled) */
    double   max_current_A;    /* Maximum expected current in amperes */
} Net;

/* ============================================================================
 * L1 Definitions: Component Structure
 * ============================================================================ */

/** Maximum pads per component */
#define PLACEMENT_MAX_PADS      256
#define PLACEMENT_MAX_NETS      1024
#define PLACEMENT_MAX_NAME_LEN  64

/** Complete component definition */
typedef struct {
    /* Identification */
    uint32_t          comp_id;           /* Unique component identifier */
    char              designator[16];    /* Reference designator (R1, C5, U3, etc.) */
    char              part_number[32];   /* Manufacturer part number */
    char              description[PLACEMENT_MAX_NAME_LEN]; /* Human description */

    /* Physical properties */
    ComponentCategory category;          /* Functional category */
    PackageType       package;           /* SMD/THT package type */
    MountType         mount;             /* Mounting location */
    PlacementDomain   domain;            /* Signal domain */
    Rect2D            body;              /* Component body outline (origin at body center) */
    Envelope3D        envelope;          /* 3D volume */
    double            mass_grams;        /* Mass in grams (for mechanical/vibration) */

    /* Electrical properties */
    double            power_dissipation_W;  /* Power dissipation in watts */
    double            max_junction_temp_C;  /* Maximum junction temperature (°C) */
    double            theta_JA_C_per_W;     /* Junction-to-ambient thermal resistance */
    double            theta_JC_C_per_W;     /* Junction-to-case thermal resistance */
    double            supply_voltage_V;     /* Nominal supply voltage */

    /* Pads and connectivity */
    uint32_t          pad_count;
    Pad               pads[PLACEMENT_MAX_PADS];
    uint32_t          net_ids[PLACEMENT_MAX_PADS]; /* Net ID for each pad */

    /* Placement state */
    Point2D           position;    /* Current placement position (component origin) */
    double            rotation;    /* Rotation in degrees (0, 90, 180, 270) */
    bool              is_placed;   /* True if component has been placed */
    bool              is_fixed;    /* True if position is locked (connectors, etc.) */
    int32_t           priority;    /* Placement priority (0 = highest) */

    /* Manufacturing */
    bool              requires_thermal_relief;
    bool              requires_glue_dot;
    double            min_spacing_mm;  /* Minimum spacing to other components */
} Component;

/* ============================================================================
 * L1 Definitions: Board Structure
 * ============================================================================ */

/** PCB stackup layer definition */
typedef struct {
    uint32_t layer_id;
    char     layer_name[32];   /* e.g., "Top", "GND", "PWR", "Bottom" */
    bool     is_signal;        /* True for routing layers */
    bool     is_plane;         /* True for power/ground planes */
    double   copper_weight_oz; /* Copper weight in ounces */
    double   thickness_mm;     /* Layer thickness */
} BoardLayer;

/** Printed circuit board definition */
typedef struct {
    char        board_name[64];
    Rect2D      outline;         /* Board outline dimensions */
    double      thickness_mm;    /* Total board thickness */
    uint32_t    layer_count;
    BoardLayer  layers[16];      /* Stackup layers (max 16) */
    double      min_trace_width_mm;
    double      min_trace_spacing_mm;
    double      min_via_drill_mm;
    double      min_via_annular_ring_mm;

    /* Placement grid */
    double      grid_x_mm;       /* Placement grid spacing X */
    double      grid_y_mm;       /* Placement grid spacing Y */
} Board;

/* ============================================================================
 * L1 Definitions: Placement Result
 * ============================================================================ */

/** Cost breakdown for a placement configuration */
typedef struct {
    double total_cost;            /* Total weighted cost */
    double wire_length_cost;      /* Half-perimeter wire length estimate */
    double thermal_cost;          /* Thermal violation penalty */
    double signal_integrity_cost; /* SI constraint violation penalty */
    double overlap_cost;          /* Component overlap penalty */
    double density_cost;          /* Local density violation penalty */
    double manufacturability_cost;/* DFM rule violation penalty */
} PlacementCost;

/** Complete placement result */
typedef struct {
    Board      board;
    uint32_t   component_count;
    Component* components;       /* Dynamically allocated array */
    uint32_t   net_count;
    Net*       nets;             /* Dynamically allocated array */
    PlacementCost cost;
    double     total_wire_length_mm; /* Estimated total wire length */
    double     placement_density;    /* Components / board area */
    bool       is_valid;             /* All constraints satisfied */
    uint32_t   iterations;           /* Iterations used in optimization */
    double     compute_time_ms;      /* Computation time */
} PlacementResult;

/* ============================================================================
 * Core API: Component Management
 * ============================================================================ */

/**
 * Initialize a component with default values.
 *
 * @param comp       Pointer to component to initialize
 * @param designator Reference designator (e.g., "R1", "U3")
 * @param category   Functional category
 * @param package    Package type
 */
void placement_component_init(Component* comp, const char* designator,
                              ComponentCategory category, PackageType package);

/**
 * Add a pad definition to a component.
 *
 * Assumes center-origin coordinate system for the component body.
 * Pad offsets are relative to the component body center.
 *
 * @param comp        Component to add pad to
 * @param pin_number  Pin number (1-indexed)
 * @param pin_name    Signal name
 * @param offset_x    X offset from component center (mm)
 * @param offset_y    Y offset from component center (mm)
 * @param pad_w       Pad width (mm)
 * @param pad_h       Pad height (mm)
 * @return true on success, false if max pads exceeded
 */
bool placement_component_add_pad(Component* comp, uint32_t pin_number,
                                 const char* pin_name,
                                 double offset_x, double offset_y,
                                 double pad_w, double pad_h);

/**
 * Set the component position and mark as placed.
 *
 * @param comp      Component to position
 * @param x         X position on board (mm)
 * @param y         Y position on board (mm)
 * @param rotation  Rotation in degrees (snapped to 0/90/180/270)
 */
void placement_component_set_position(Component* comp, double x, double y,
                                      double rotation);

/**
 * Compute the bounding box of a component in board coordinates,
 * accounting for rotation.
 *
 * @param comp  Component to compute bounds for
 * @return      Bounding box in board coordinates
 */
BoundingBox placement_component_get_bounds(const Component* comp);

/* ============================================================================
 * Core API: Board Management
 * ============================================================================ */

/**
 * Initialize a board with given dimensions.
 *
 * @param board    Board to initialize
 * @param name     Board name
 * @param width    Board width in mm
 * @param height   Board height in mm
 * @param layers   Number of layers
 */
void placement_board_init(Board* board, const char* name,
                          double width, double height, uint32_t layers);

/**
 * Add a layer definition to the board stackup.
 *
 * @param board      Board to modify
 * @param layer_id   Layer number
 * @param name       Layer name
 * @param is_signal  True if routing layer
 * @param is_plane   True if power/ground plane
 * @param cu_weight  Copper weight in oz
 * @param thickness  Layer thickness in mm
 * @return true on success
 */
bool placement_board_add_layer(Board* board, uint32_t layer_id,
                               const char* name, bool is_signal, bool is_plane,
                               double cu_weight, double thickness);

/* ============================================================================
 * Core API: Net Management
 * ============================================================================ */

/**
 * Initialize a net with a given name.
 *
 * @param net   Net to initialize
 * @param id    Unique net ID
 * @param name  Net name
 */
void placement_net_init(Net* net, uint32_t id, const char* name);

/* ============================================================================
 * Core API: Placement Result Management
 * ============================================================================ */

/**
 * Initialize a placement result, allocating memory for components and nets.
 *
 * @param result          Placement result to initialize
 * @param board           Board definition
 * @param max_components  Maximum number of components
 * @param max_nets        Maximum number of nets
 * @return true on success
 */
bool placement_result_init(PlacementResult* result, const Board* board,
                           uint32_t max_components, uint32_t max_nets);

/**
 * Free memory associated with a placement result.
 *
 * @param result  Placement result to free
 */
void placement_result_free(PlacementResult* result);

/**
 * Compute the centroid (center of mass) of all placed components.
 *
 * Useful for evaluating board balance and mechanical considerations.
 *
 * @param result  Placement result to analyze
 * @return        Centroid point
 */
Point2D placement_compute_centroid(const PlacementResult* result);

/**
 * Estimate total wire length using half-perimeter wirelength (HPWL) metric.
 *
 * HPWL = sum over all nets of (max_x - min_x + max_y - min_y) for pins on that net.
 * This is a commonly used estimate in VLSI and PCB placement.
 *
 * Complexity: O(N * P) where N = number of nets, P = average pins per net.
 * Reference: Shahookar & Mazumder, "VLSI Cell Placement Techniques", ACM Computing Surveys, 1991.
 *
 * @param result  Placement result (components must be placed)
 * @return        Estimated total wire length in mm
 */
double placement_estimate_wire_length(const PlacementResult* result);

/**
 * Check if a component can be legally placed at the given position.
 * Checks board boundary, overlap with other components, and minimum spacing.
 *
 * @param result    Current placement result
 * @param comp      Component to check
 * @param x         Proposed X position
 * @param y         Proposed Y position
 * @return true if position is legal
 */
bool placement_is_position_legal(const PlacementResult* result,
                                 const Component* comp, double x, double y);

#ifdef __cplusplus
}
#endif

#endif /* PLACEMENT_CORE_H */
