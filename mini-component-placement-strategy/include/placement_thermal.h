/**
 * @file placement_thermal.h
 * @brief Thermal-aware PCB component placement
 *
 * Implements thermal modeling and analysis for PCB component placement,
 * enabling temperature-aware optimization to prevent hot spots and
 * improve reliability.
 *
 * Knowledge Mapping:
 *   L2 (Core Concepts): Thermal management in PCB layout, heat spreading,
 *                       thermal vias, copper pour for heat dissipation
 *   L3 (Math Structures): Thermal resistance network, heat equation
 *                         (steady-state conduction), superposition
 *   L4 (Fundamental Laws): Fourier's law of heat conduction,
 *                         Newton's law of cooling, Joule heating
 *
 * Course Alignment:
 *   - MIT 6.630: EM and thermal analysis of PCB structures
 *   - Michigan EECS 411: Microwave circuit thermal management
 *   - TU Munich: High-frequency engineering thermal design
 *
 * References:
 *   - Incropera et al., "Fundamentals of Heat and Mass Transfer", 2007
 *   - Erickson & Maksimovic, "Fundamentals of Power Electronics", 2001, Ch. 19
 *   - IPC-2152: Standard for Determining Current Carrying Capacity in PCB Design
 */

#ifndef PLACEMENT_THERMAL_H
#define PLACEMENT_THERMAL_H

#include "placement_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1 Definitions: Thermal Model Structures
 * ============================================================================ */

/** Thermal resistance network node */
typedef struct {
    uint32_t node_id;
    uint32_t comp_id;          /* 0 = ambient/environment node */
    double   temperature_C;    /* Node temperature */
    double   power_W;          /* Heat dissipated at this node (source) */
    double   capacitance_JpK;  /* Thermal capacitance (for transient) */
} ThermalNode;

/** Thermal resistance edge between two nodes */
typedef struct {
    uint32_t from_node;
    uint32_t to_node;
    double   resistance_CpW;   /* Thermal resistance in °C/W */
    char     path_type[32];    /* "copper", "FR4", "via", "convection", etc. */
} ThermalEdge;

/** Complete thermal resistance network */
typedef struct {
    uint32_t     node_count;
    ThermalNode* nodes;
    uint32_t     edge_count;
    ThermalEdge* edges;
    double       ambient_temperature_C;
    double       board_k_wpmk;     /* Board thermal conductivity (W/(m·K)) */
    double       board_thickness_mm;
} ThermalNetwork;

/** Thermal via definition */
typedef struct {
    Point2D position;
    double  drill_diameter_mm;
    double  outer_diameter_mm;
    double  plating_thickness_um;
    uint32_t via_count;          /* Number of vias in this array */
    double  effective_resistance_CpW; /* Computed thermal resistance */
} ThermalVia;

/* ============================================================================
 * Thermal Network Construction
 * ============================================================================ */

/**
 * Build a thermal resistance network from a component placement.
 *
 * Creates nodes for each heat-dissipating component and edges representing:
 *   - Conduction through PCB substrate (Fourier's law)
 *   - Conduction through copper planes (spreading resistance)
 *   - Convection to ambient (Newton's law of cooling)
 *
 * @param network    Output thermal network (caller must pre-allocate or use init)
 * @param result     Component placement
 * @param ambient_C  Ambient temperature
 * @return           True on success
 */
bool placement_thermal_build_network(ThermalNetwork* network,
                                      const PlacementResult* result,
                                      double ambient_C);

/**
 * Free memory associated with a thermal network.
 */
void placement_thermal_network_free(ThermalNetwork* network);

/* ============================================================================
 * Steady-State Thermal Analysis
 * ============================================================================ */

/**
 * Solve steady-state thermal network using nodal analysis.
 *
 * Solves the linear system G * T = P where:
 *   G is the thermal conductance matrix (G_ij = 1/R_ij),
 *   T is the vector of node temperatures (unknowns),
 *   P is the vector of power dissipations.
 *
 * Uses Gaussian elimination with partial pivoting.
 *
 * Complexity: O(N^3) for N nodes.
 * Reference: Incropera, "Fundamentals of Heat and Mass Transfer", Ch. 3.
 *
 * @param network  Thermal network (edges must be populated)
 * @return         True if solution converged
 */
bool placement_thermal_solve_steady_state(ThermalNetwork* network);

/**
 * Compute the junction temperature of a specific component
 * given its placement and thermal network.
 *
 * T_j = T_amb + P * θ_JA(effective)
 * where θ_JA(effective) = θ_JC + θ_CA (case-to-ambient via board).
 *
 * @param comp       Component to evaluate
 * @param network    Solved thermal network
 * @return           Junction temperature in °C
 */
double placement_thermal_junction_temp(const Component* comp,
                                        const ThermalNetwork* network);

/* ============================================================================
 * Heat Spreading Analysis
 * ============================================================================ */

/**
 * Compute heat spreading resistance for a component on a PCB.
 *
 * For a rectangular heat source on a board of finite thickness:
 *   R_spread = (1 / (π * k * a)) * f(a/b, t/a)
 * where a = sqrt(area_source/π), b = sqrt(area_board),
 *       k = board thermal conductivity, t = board thickness.
 *
 * Uses the analytic solution from:
 *   Yovanovich et al., "General Solution of Constriction Resistance
 *   within a Compound Disk", AIAA, 1976.
 *
 * @param comp               Component (source)
 * @param board_k_wpmk        Board thermal conductivity W/(m·K)
 * @param board_thickness_mm  Board thickness
 * @return                    Spreading resistance in °C/W
 */
double placement_thermal_spreading_resistance(const Component* comp,
                                               double board_k_wpmk,
                                               double board_thickness_mm);

/**
 * Compute temperature rise at position (x,y) due to a point heat source.
 *
 * Uses the method of images for a finite-thickness plate with
 * convection boundary conditions at top and bottom surfaces.
 *
 * T(x,y) - T_amb = (P / (2π * k * t)) * K_0(r * sqrt(h / (k*t)))
 * where K_0 is the modified Bessel function, r = distance from source,
 * h = convection coefficient, k = thermal conductivity, t = thickness.
 *
 * Reference: Carslaw & Jaeger, "Conduction of Heat in Solids", 1959, Ch. 10.
 *
 * @param x_mm, y_mm          Evaluation position
 * @param source_x, source_y  Heat source position
 * @param power_W             Source power in watts
 * @param board_k_wpmk        Board thermal conductivity
 * @param board_thickness_mm  Board thickness
 * @param h_conv_Wpm2K        Convection coefficient
 * @param ambient_C           Ambient temperature
 * @return                    Temperature at (x,y) in °C
 */
double placement_thermal_temperature_at(double x_mm, double y_mm,
                                         double source_x, double source_y,
                                         double power_W,
                                         double board_k_wpmk,
                                         double board_thickness_mm,
                                         double h_conv_Wpm2K,
                                         double ambient_C);

/* ============================================================================
 * Thermal Via Optimization
 * ============================================================================ */

/**
 * Compute the effective thermal resistance of a via array.
 *
 * R_via_array = (R_single_via) / N for N identical vias in parallel.
 * R_single_via = (4 * t) / (π * k_Cu * (D_outer² - D_inner²))
 *
 * Reference: Li et al., "Thermal Via Design for PCB Thermal Management",
 *            IEEE Trans. CPMT, 2018.
 *
 * @param vias             Via array definition
 * @param board_thickness_mm Board thickness
 * @return                 Effective thermal resistance in °C/W
 */
double placement_thermal_via_resistance(const ThermalVia* vias,
                                         double board_thickness_mm);

/**
 * Optimize thermal via placement for a hot component.
 *
 * Determines the number and pattern of thermal vias needed to keep
 * T_j below T_j_max, subject to routing area constraints.
 *
 * @param comp             Hot component
 * @param max_temp_C       Maximum allowed junction temperature
 * @param ambient_C        Ambient temperature
 * @param board_k_wpmk     Board thermal conductivity
 * @param board_thickness_mm Board thickness
 * @param available_area   Rectangle available for via placement
 * @param via_drill_mm     Available via drill size
 * @return                 Number of vias required (-1 if impossible)
 */
int32_t placement_thermal_vias_required(const Component* comp,
                                         double max_temp_C,
                                         double ambient_C,
                                         double board_k_wpmk,
                                         double board_thickness_mm,
                                         Rect2D available_area,
                                         double via_drill_mm);

/* ============================================================================
 * Hot Spot Detection
 * ============================================================================ */

/**
 * Detect thermal hot spots in a placement.
 *
 * A hot spot is defined as any region where local temperature
 * exceeds mean + 3 * standard_deviation (statistical outlier criterion).
 *
 * Scans the board on a grid and flags cells exceeding threshold.
 *
 * @param result     Placement result
 * @param network    Solved thermal network
 * @param grid_mm    Detection grid spacing
 * @param positions  Output array of hot spot positions (caller allocates)
 * @param max_spots  Maximum number of hot spots to detect
 * @return           Number of hot spots detected
 */
uint32_t placement_thermal_detect_hotspots(const PlacementResult* result,
                                            const ThermalNetwork* network,
                                            double grid_mm,
                                            Point2D* positions,
                                            uint32_t max_spots);

/**
 * Compute the maximum temperature gradient on the board.
 *
 * High gradients (>10°C/cm) can cause mechanical stress and
 * reliability issues due to CTE mismatch.
 *
 * @param network  Solved thermal network
 * @return         Maximum temperature gradient in °C/mm
 */
double placement_thermal_max_gradient(const ThermalNetwork* network);

#ifdef __cplusplus
}
#endif

#endif /* PLACEMENT_THERMAL_H */
