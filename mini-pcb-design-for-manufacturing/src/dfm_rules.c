/**
 * @file    dfm_rules.c
 * @brief   DFM Design Rules Implementation - L1-L5
 *
 * @details Implements all PCB design rule checks for manufacturing:
 *          - Minimum trace width by copper weight and IPC class
 *          - Trace spacing / clearance (IPC-2221 voltage-based)
 *          - Annular ring verification
 *          - Solder mask expansion and web/dam checks
 *          - Silkscreen legibility rules
 *          - Drill rules (diameter, aspect ratio, clearance)
 *          - Edge clearance rules
 *          - Thermal relief spoke design
 *          - IPC-2221 electrical clearance (voltage-spacing)
 *          - Etch compensation calculation
 *
 * Knowledge Mapping:
 *   L1 - Definitions: trace width, spacing, annular ring, solder mask
 *        expansion, silkscreen min line, drill-to-copper, edge clearance,
 *        thermal relief spoke, antipad
 *   L2 - Core Concepts: Etching factor, mask registration tolerance,
 *        creepage distance, thermal relief current capacity
 *   L3 - Mathematical Structures: Voltage-spacing piecewise functions,
 *        etch compensation formula, thermal relief geometry
 *   L4 - Fundamental Laws: IPC-2221 spacing formula, IPC-2152
 *        current-carrying capacity
 *   L5 - Algorithms: Design rule check engines, clearance computation
 *
 * Reference: IPC-2221 (Generic Standard on Printed Board Design)
 *            IPC-2222 (Sectional Design Standard for Rigid Organic)
 *            IPC-6012 (Qualification and Performance)
 *            IPC-2152 (Current-Carrying Capacity)
 */

#include "dfm_rules.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ================================================================
   L1 - Trace Width Rules
   ================================================================

   Minimum trace width depends on:
   1. Copper weight (thicker copper = wider minimum due to etching)
   2. IPC class (higher class = tighter requirements)
   3. Manufacturing process capability

   Trace Width Data (IPC-2221, IPC-6012):
   +---------------+-----------+-----------+-----------+
   | Copper Weight | Class 1   | Class 2   | Class 3   |
   |               | (um)      | (um)      | (um)      |
   +---------------+-----------+-----------+-----------+
   | 0.5 oz        | 75        | 75        | 75        |
   | 1.0 oz        | 100       | 100       | 100       |
   | 2.0 oz        | 150       | 150       | 150       |
   | 3.0 oz        | 200       | 200       | 200       |
   | 4.0 oz        | 250       | 250       | 250       |
   | 6.0 oz        | 400       | 400       | 400       |
   +---------------+-----------+-----------+-----------+

   Etch compensation is required because the chemical etching
   process is isotropic - it etches sideways as well as down.
   The amount of sideways etching is proportional to copper
   thickness. Typical etch factor = 1.0 (1 um sideways per
   1 um of copper thickness).

   Design width = Target width + 2 * Cu_thickness * etch_factor
   ================================================================ */

static const trace_width_rule_t trace_width_rules[] = {
    { CU_WEIGHT_0_5_OZ,  75,  75,  75, 17.5 },
    { CU_WEIGHT_1_0_OZ, 100, 100, 100, 35.0 },
    { CU_WEIGHT_2_0_OZ, 150, 150, 150, 70.0 },
    { CU_WEIGHT_3_0_OZ, 200, 200, 200, 105.0 },
    { CU_WEIGHT_4_0_OZ, 250, 250, 250, 140.0 },
    { CU_WEIGHT_6_0_OZ, 400, 400, 400, 210.0 },
};

static const int num_trace_rules =
    (int)(sizeof(trace_width_rules) / sizeof(trace_width_rules[0]));

double get_min_trace_width(copper_weight_t copper, ipc_class_t cls)
{
    for (int i = 0; i < num_trace_rules; i++) {
        if (trace_width_rules[i].copper == copper) {
            switch (cls) {
            case IPC_CLASS_1: return trace_width_rules[i].class1_min_width_um;
            case IPC_CLASS_2: return trace_width_rules[i].class2_min_width_um;
            case IPC_CLASS_3: return trace_width_rules[i].class3_min_width_um;
            default:          return 0.0;
            }
        }
    }
    return 0.0;
}

double get_etch_compensation(copper_weight_t copper)
{
    for (int i = 0; i < num_trace_rules; i++) {
        if (trace_width_rules[i].copper == copper) {
            return trace_width_rules[i].etch_factor_um;
        }
    }
    return 0.0;
}

/* ================================================================
   L1/L4 - Spacing / Clearance Rules (IPC-2221)
   ================================================================

   Electrical clearance between conductors prevents dielectric
   breakdown and maintains signal integrity. The required spacing
   increases with voltage.

   IPC-2221 voltage-spacing table:
   +-----------------+------------------+-------------------+
   | Voltage Range   | Internal Spacing | External Spacing  |
   | (V peak)        | (mm)             | (mm)              |
   +-----------------+------------------+-------------------+
   | 0-15            | 0.05             | 0.05              |
   | 15-30           | 0.05             | 0.10              |
   | 30-50           | 0.10             | 0.20              |
   | 50-100          | 0.15             | 0.40              |
   | 100-150         | 0.30             | 0.60              |
   | 150-170         | 0.30             | 1.00              |
   | 170-250         | 0.50             | 1.25              |
   | 250-300         | 0.80             | 1.50              |
   | 300-500         | 1.50             | 2.50              |
   | >500            | 0.005*V + 0.5    | 0.005*V + 0.5     |
   +-----------------+------------------+-------------------+

   Derating factors:
   - Conformal coating: multiply spacing by 0.5 (halves required)
   - High altitude (>3000m): multiply by 1.3 per 1000m above 3km
   - Internal layers are already derated (internal = external / 1.5)
   ================================================================ */

static double ipc2221_base_spacing_internal(double Vpeak)
{
    if (Vpeak <= 15.0)      return 0.05;
    else if (Vpeak <= 30.0) return 0.05;
    else if (Vpeak <= 50.0) return 0.10;
    else if (Vpeak <= 100.0) return 0.15;
    else if (Vpeak <= 150.0) return 0.30;
    else if (Vpeak <= 170.0) return 0.30;
    else if (Vpeak <= 250.0) return 0.50;
    else if (Vpeak <= 300.0) return 0.80;
    else if (Vpeak <= 500.0) return 1.50;
    else                     return 0.005 * Vpeak + 0.5;
}

static double ipc2221_base_spacing_external(double Vpeak)
{
    if (Vpeak <= 15.0)      return 0.05;
    else if (Vpeak <= 30.0) return 0.10;
    else if (Vpeak <= 50.0) return 0.20;
    else if (Vpeak <= 100.0) return 0.40;
    else if (Vpeak <= 150.0) return 0.60;
    else if (Vpeak <= 170.0) return 1.00;
    else if (Vpeak <= 250.0) return 1.25;
    else if (Vpeak <= 300.0) return 1.50;
    else if (Vpeak <= 500.0) return 2.50;
    else                     return 0.005 * Vpeak + 0.5;
}

double compute_ipc2221_clearance(double voltage_peak, bool is_external,
                                  bool is_coated, double altitude_m)
{
    /* Get base spacing in mm */
    double spacing_mm = is_external
        ? ipc2221_base_spacing_external(voltage_peak)
        : ipc2221_base_spacing_internal(voltage_peak);

    /* Convert internal->external ratio */
    if (!is_external) {
        spacing_mm *= 1.5; /* internal needs 1.5x of the base internal */
    }

    /* Conformal coating derating: coated PCBs can use 50% spacing */
    if (is_coated) {
        spacing_mm *= 0.5;
    }

    /* High altitude derating:
     * Above 3000m, spacing increases 30% per 1000m.
     * Reason: lower air pressure reduces dielectric strength
     * (Paschen's law - breakdown voltage depends on p*d product) */
    if (altitude_m > 3000.0) {
        double extra_km = (altitude_m - 3000.0) / 1000.0;
        spacing_mm *= (1.0 + 0.30 * extra_km);
    }

    /* Return in micrometers (industry standard unit for PCB) */
    return spacing_mm * 1000.0;
}

double compute_required_spacing(double voltage_peak, bool is_external,
                                bool is_coated, bool is_high_alt)
{
    double altitude = is_high_alt ? 5000.0 : 0.0;
    return compute_ipc2221_clearance(voltage_peak, is_external,
                                     is_coated, altitude);
}

/* ================================================================
   L2 - Annular Ring Rules
   ================================================================

   Annular ring is the copper pad that remains after drilling a hole.
   It provides the solderable surface and mechanical anchor for the
   plated through-hole.

   IPC Requirements:
   +----------+-------------+-------------+----------+
   | Class    | Ext Ring    | Int Ring    | Breakout |
   +----------+-------------+-------------+----------+
   | Class 1  | 50 um       | N/A         | Allowed  |
   | Class 2  | 50 um       | 25 um       | 90 deg   |
   | Class 3  | 50 um       | 25 um       | None     |
   +----------+-------------+-------------+----------+

   Breakout is measured as the angle from center where the drill
   hole extends beyond the pad. 90 deg breakout means up to 25%
   of the circumference can break out (acceptable for Class 2).
   ================================================================ */

static const annular_ring_rule_t annular_ring_rules[] = {
    { IPC_CLASS_1, 50.0, 0.0,  100.0 }, /* 100% breakout allowed */
    { IPC_CLASS_2, 50.0, 25.0, 25.0  }, /* 90 deg = 25% circumference */
    { IPC_CLASS_3, 50.0, 25.0, 0.0   }, /* zero breakout */
};

const annular_ring_rule_t* get_annular_ring_rule(ipc_class_t cls)
{
    for (int i = 0; i < 3; i++) {
        if (annular_ring_rules[i].ipc_class == cls)
            return &annular_ring_rules[i];
    }
    return NULL;
}

bool check_annular_ring(double pad_diameter_um, double drill_diameter_um,
                        bool is_external, ipc_class_t ipc_class,
                        double breakout_deg)
{
    const annular_ring_rule_t *rule = get_annular_ring_rule(ipc_class);
    if (!rule) return false;

    /* Compute actual annular ring */
    double ring_um = (pad_diameter_um - drill_diameter_um) / 2.0;

    /* Check minimum ring width */
    double min_ring = is_external
        ? rule->min_external_ring_um
        : rule->min_internal_ring_um;

    if (ring_um < (min_ring - 1e-9)) return false;

    /* Check breakout */
    double breakout_pct = (breakout_deg / 360.0) * 100.0;
    if (breakout_pct > rule->max_breakout_pct) return false;

    return true;
}

/* ================================================================
   L2 - Solder Mask Rules
   ================================================================

   Solder mask is the polymer coating that protects copper traces
   from oxidation and prevents solder bridges during assembly.

   Key parameters:
   - Mask expansion: distance from pad edge to mask opening edge.
     Positive expansion means mask opening is larger than pad.
     Typical: 50-75 um for Class 1-2, 50-100 um for Class 3.

   - Mask web: the solder mask bridge between two adjacent openings.
     Must be wide enough to adhere to laminate. Min web: 75-100 um.

   - Mask dam: the solder mask bridge between pad and via.
     Prevents solder wicking into via. Min dam: 100-150 um.

   - Registration tolerance: misalignment between mask film and
     copper pattern. Typically 50-75 um per side.

   Mask opening = pad_diameter + 2 * mask_expansion
   Mask web = center_dist - opening1/2 - opening2/2
   ================================================================ */

static const solder_mask_rule_t mask_rules[] = {
    /* Class 1 */
    { 50.0, 100.0, 75.0, 75.0, 100.0 },
    /* Class 2 */
    { 50.0, 100.0, 50.0, 100.0, 150.0 },
    /* Class 3 */
    { 50.0, 100.0, 50.0, 100.0, 150.0 },
};

const solder_mask_rule_t* get_solder_mask_rule(ipc_class_t cls)
{
    if (cls < 0 || cls > 2) return NULL;
    return &mask_rules[cls];
}

double compute_mask_opening(double pad_diameter_um, double expansion_um)
{
    if (pad_diameter_um < 0.0) return 0.0;
    return pad_diameter_um + 2.0 * expansion_um;
}

bool check_mask_web(double opening1_um, double opening2_um,
                    double center_dist_um, double min_web_um)
{
    if (center_dist_um <= 0.0) return false;

    /* Web = distance between edges of mask openings */
    double half_sum = (opening1_um + opening2_um) / 2.0;
    double web = center_dist_um - half_sum;

    return (web >= min_web_um);
}

/* ================================================================
   L1 - Silkscreen Rules
   ================================================================

   Silkscreen (also called legend or nomenclature) provides text
   and symbols on the PCB surface for component identification,
   orientation, and board labeling.

   IPC requirements for legibility:
   - Min line width: 100-150 um (Class 3 needs finer lines)
   - Min text height: 800-1500 um (smaller for high-density)
   - Clearance to pads: 100-200 um (prevent silkscreen on solderable)
   - Clearance to solder mask edge: 50-100 um
   - Clearance to board edge: 200-300 um (prevent peeling)

   Silkscreen inks: epoxy-based (white most common), UV-curable.
   Application: screen printing (low cost) or inkjet (high precision).
   ================================================================ */

static const silkscreen_rule_t silkscreen_rules[] = {
    /* Class 1 */
    { 150.0, 1500.0, 150.0, 200.0, 100.0, 300.0 },
    /* Class 2 */
    { 120.0, 1200.0, 120.0, 150.0, 75.0, 250.0 },
    /* Class 3 */
    { 100.0, 1000.0, 100.0, 100.0, 50.0, 200.0 },
};

const silkscreen_rule_t* get_silkscreen_rule(ipc_class_t cls)
{
    if (cls < 0 || cls > 2) return NULL;
    return &silkscreen_rules[cls];
}

/* ================================================================
   L2 - Drill Rules
   ================================================================

   PCB drilling uses two main technologies:
   1. Mechanical drilling: carbide bits, 0.1-6.35mm diameter range,
      typical speed 80-160k RPM, feed rate 0.5-3 m/min.
   2. Laser drilling: CO2 or UV laser, 0.025-0.15mm diameter,
      used for microvias in HDI (High Density Interconnect) boards.

   Key constraints:
   - Min drill diameter: limited by bit manufacturing and breakage
   - Max aspect ratio: limits board thickness for a given hole size
   - Drill-to-copper clearance: prevents drill from damaging nearby traces
   - Hole-to-edge: prevents breakout at board edge
   - Hole-to-hole: prevents web fracturing between adjacent holes

   Standard mechanical drill (non-advanced process):
   - Min diameter: 0.20 mm (0.15 mm available at premium)
   - Max aspect ratio: 8:1
   - Drill-to-copper: 200-400 um depending on class
   - Hole-to-hole: 0.50 mm (center-to-center)

   Advanced process (HDI):
   - Laser via max depth: 100 um (1 dielectric layer)
   - Laser via max aspect ratio: 1:1
   ================================================================ */

static const drill_rule_t drill_rule_standard = {
    0.20, 6.35, 300.0, 0.80, 8.0, 0.50, 0.50,
    100.0, 1.0
};

static const drill_rule_t drill_rule_advanced = {
    0.10, 6.35, 200.0, 0.50, 10.0, 0.40, 0.40,
    100.0, 1.0
};

const drill_rule_t* get_drill_rule(bool is_advanced_process)
{
    return is_advanced_process ? &drill_rule_advanced : &drill_rule_standard;
}

/* ================================================================
   L2 - Edge Clearance Rules
   ================================================================

   Edge clearance is the distance between any conductive feature
   and the board outline. Required for:
   1. Routing tolerance during depaneling
   2. Preventing copper exposure at board edge (corrosion risk)
   3. Maintaining structural integrity of edge features

   Internal plane pullback: additional clearance for internal power/
   ground planes, typically 400-600 um. This prevents copper exposure
   at the board edge after routing, which could cause short circuits
   or corrosion.
   ================================================================ */

static const edge_clearance_rule_t edge_rules[] = {
    /* Class 1 */
    { 250.0, 400.0, 200.0, 250.0, 400.0 },
    /* Class 2 */
    { 300.0, 500.0, 250.0, 300.0, 500.0 },
    /* Class 3 */
    { 400.0, 600.0, 300.0, 400.0, 600.0 },
};

const edge_clearance_rule_t* get_edge_clearance_rule(ipc_class_t cls)
{
    if (cls < 0 || cls > 2) return NULL;
    return &edge_rules[cls];
}

/* ================================================================
   L2 - Thermal Relief Rules
   ================================================================

   Thermal relief pads (also called "thermal ties" or "wagon wheels")
   connect pads to copper planes through narrow spokes instead of
   a solid connection. This serves two purposes:

   1. Thermal management during soldering:
      Solid plane connections act as heat sinks, making it difficult
      to heat the pad to soldering temperature. Thermal relief
      restricts heat flow, enabling proper solder joint formation.

   2. Electrical connection:
      Spoke cross-section must be sufficient for the required current.
      Too thin = excessive voltage drop and heating.
      Too thick = soldering difficulty (heat sinking).

   Typical spoke design:
   - 4 spokes at 90 deg (standard) or 2 spokes (high density)
   - Spoke width: 200-500 um
   - Clearance gap (air gap between pad and plane): 200-400 um
   - Antipad: the clearance area in the plane around the pad

   Thermal connection area:
     A_thermal = N_spokes * spoke_width * copper_thickness

   Current capacity check using IPC-2152:
     Ensure A_thermal >= required cross-section for current
   ================================================================ */

static const thermal_relief_rule_t thermal_rules[] = {
    /* Class 1 */
    { 200.0, 600.0, 200.0, 100.0, 4, 300.0 },
    /* Class 2 */
    { 250.0, 500.0, 250.0, 100.0, 4, 350.0 },
    /* Class 3 */
    { 300.0, 450.0, 300.0, 100.0, 4, 400.0 },
};

const thermal_relief_rule_t* get_thermal_relief_rule(ipc_class_t cls)
{
    if (cls < 0 || cls > 2) return NULL;
    return &thermal_rules[cls];
}

double compute_thermal_relief_area(double pad_diameter_um,
                                    double antipad_diameter_um,
                                    double spoke_width_um,
                                    int num_spokes,
                                    double copper_thickness_um)
{
    /* Validate inputs */
    if (pad_diameter_um <= 0.0 || spoke_width_um <= 0.0 ||
        num_spokes <= 0 || copper_thickness_um <= 0.0) {
        return 0.0;
    }

    /* The spoke connects from pad edge to antipad edge.
       Spoke length = (antipad_diameter - pad_diameter) / 2 */
    double spoke_length_um = (antipad_diameter_um - pad_diameter_um) / 2.0;
    if (spoke_length_um <= 0.0) return 0.0;

    /* Area of one spoke = width * length (rectangular approximation) */
    double one_spoke_area_um2 = spoke_width_um * spoke_length_um;

    /* Total thermal connection cross-sectional area */
    double total_area_um2 = (double)num_spokes * one_spoke_area_um2;

    /* Convert to mm^2 for current capacity calculations */
    return total_area_um2 * 1e-6;
}

/* ================================================================
   L3 - Etching Compensation
   ================================================================

   Chemical etching is isotropic - it removes copper in all directions
   at approximately the same rate. This means that as the etchant
   removes copper vertically (thinning), it also removes copper
   horizontally (narrowing traces).

   The etch factor quantifies this anisotropy:
     F_etch = lateral_etch / vertical_etch

   Typical etch factors:
     - Standard process: F = 1.0 (equal lateral and vertical)
     - Advanced process: F = 0.5 (better anisotropy)
     - Plasma etching: F = 0.1 (highly anisotropic)

   Compensation formula:
     W_design = W_target + 2 * F * t_copper

   where:
     W_design = width drawn in CAD (design width)
     W_target = desired finished width after etching
     F = etch factor
     t_copper = copper foil thickness

   The factor of 2 accounts for etching from both sides of the trace.
   ================================================================ */

double compute_etch_compensation(double target_width_um,
                                  double copper_thickness_um,
                                  double etch_factor)
{
    if (target_width_um <= 0.0 || copper_thickness_um <= 0.0 ||
        etch_factor < 0.0) {
        return 0.0;
    }

    /* W_designed = W_target + 2 * etch_factor * copper_thickness */
    double lateral_etch = 2.0 * etch_factor * copper_thickness_um;
    return target_width_um + lateral_etch;
}
