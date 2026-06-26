/**
 * @file placement_thermal.c
 * @brief Implementation of thermal-aware PCB component placement
 */

#include "placement_thermal.h"
#include "placement_constraint.h"
#include "placement_util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Thermal Network Construction
 * ============================================================================ */

bool placement_thermal_build_network(ThermalNetwork* network,
                                      const PlacementResult* result,
                                      double ambient_C)
{
    if (!network || !result) return false;

    uint32_t n_heaters = 0;
    /* Count heat-dissipating components */
    for (uint32_t i = 0; i < result->component_count; i++) {
        if (result->components[i].is_placed
            && result->components[i].power_dissipation_W > 0.0) {
            n_heaters++;
        }
    }

    if (n_heaters == 0) {
        network->node_count = 0;
        network->edge_count = 0;
        network->nodes = NULL;
        network->edges = NULL;
        return true;
    }

    /* Nodes: one per heater plus one ambient node (index 0) */
    network->node_count = n_heaters + 1;
    network->nodes = (ThermalNode*)calloc(network->node_count, sizeof(ThermalNode));
    if (!network->nodes) return false;

    /* Ambient node */
    network->nodes[0].node_id       = 0;
    network->nodes[0].comp_id       = 0;
    network->nodes[0].temperature_C = ambient_C;
    network->nodes[0].power_W       = 0.0;

    /* Heater nodes */
    uint32_t node_idx = 1;
    for (uint32_t i = 0; i < result->component_count; i++) {
        if (result->components[i].is_placed
            && result->components[i].power_dissipation_W > 0.0) {
            network->nodes[node_idx].node_id       = node_idx;
            network->nodes[node_idx].comp_id       = result->components[i].comp_id;
            network->nodes[node_idx].temperature_C = ambient_C;
            network->nodes[node_idx].power_W       = result->components[i].power_dissipation_W;
            node_idx++;
        }
    }

    /* Edges: N*(N-1)/2 between heaters + N edges to ambient */
    uint32_t max_edges = n_heaters * (n_heaters - 1) / 2 + n_heaters;
    network->edges = (ThermalEdge*)calloc(max_edges, sizeof(ThermalEdge));
    if (!network->edges) {
        free(network->nodes);
        network->nodes = NULL;
        return false;
    }

    /* Ambient-to-heater edges (convection + conduction through board) */
    for (uint32_t n = 1; n <= n_heaters; n++) {
        ThermalEdge* edge = &network->edges[network->edge_count];
        edge->from_node = n;
        edge->to_node   = 0; /* Ambient */
        /* Effective θ_JA from component datasheet */
        uint32_t comp_idx = 0;
        /* Find the component for this node */
        for (uint32_t i = 0; i < result->component_count; i++) {
            if (result->components[i].comp_id == network->nodes[n].comp_id) {
                comp_idx = i;
                break;
            }
        }
        edge->resistance_CpW = result->components[comp_idx].theta_JA_C_per_W;
        strncpy(edge->path_type, "ambient", sizeof(edge->path_type) - 1);
        network->edge_count++;
    }

    /* Heater-to-heater edges (lateral conduction through PCB) */
    for (uint32_t i = 1; i <= n_heaters; i++) {
        for (uint32_t j = i + 1; j <= n_heaters; j++) {
            /* Find corresponding components */
            Component* ci = NULL, *cj = NULL;
            for (uint32_t k = 0; k < result->component_count; k++) {
                if (result->components[k].comp_id == network->nodes[i].comp_id)
                    ci = &result->components[k];
                if (result->components[k].comp_id == network->nodes[j].comp_id)
                    cj = &result->components[k];
            }
            if (!ci || !cj) continue;

            ThermalEdge* edge = &network->edges[network->edge_count];
            edge->from_node = i;
            edge->to_node   = j;
            edge->resistance_CpW = placement_constraint_thermal_coupling(
                ci, cj, result->board.thickness_mm);
            /* Convert coupling coefficient to resistance: θ = 1/coupling if coupling > 0 */
            if (edge->resistance_CpW < 1e-9) {
                edge->resistance_CpW = 1e9; /* Very large = negligible coupling */
            } else {
                edge->resistance_CpW = 1.0 / edge->resistance_CpW;
            }
            strncpy(edge->path_type, "board_lateral", sizeof(edge->path_type) - 1);
            network->edge_count++;
        }
    }

    network->ambient_temperature_C = ambient_C;
    network->board_k_wpmk          = 0.3; /* FR4 */
    network->board_thickness_mm    = result->board.thickness_mm;

    return true;
}

void placement_thermal_network_free(ThermalNetwork* network)
{
    if (!network) return;
    free(network->nodes);
    free(network->edges);
    network->nodes = NULL;
    network->edges = NULL;
    network->node_count = 0;
    network->edge_count = 0;
}

/* ============================================================================
 * Steady-State Thermal Solver
 * ============================================================================ */

bool placement_thermal_solve_steady_state(ThermalNetwork* network)
{
    if (!network || network->node_count < 2) return false;

    uint32_t N = network->node_count;

    /* Build conductance matrix G (N x N) and power vector P (N) */
    double* G = (double*)calloc(N * N, sizeof(double));
    double* P = (double*)calloc(N, sizeof(double));
    if (!G || !P) {
        free(G); free(P);
        return false;
    }

    /* Fill conductance matrix: G_ij = -1/R_ij for i≠j, G_ii = Σ 1/R_ij */
    for (uint32_t e = 0; e < network->edge_count; e++) {
        uint32_t u = network->edges[e].from_node;
        uint32_t v = network->edges[e].to_node;
        double R = network->edges[e].resistance_CpW;

        if (R < 1e-12) R = 1e-12; /* Avoid division by zero */
        double g = 1.0 / R;

        G[u * N + u] += g;
        G[v * N + v] += g;
        G[u * N + v] -= g;
        G[v * N + u] -= g;
    }

    /* Power vector: P_i = power dissipated at node i */
    for (uint32_t i = 0; i < N; i++) {
        P[i] = network->nodes[i].power_W;
    }

    /* Fix ambient node (node 0) temperature */
    /* Modify row 0: [1, 0, 0, ..., 0], RHS = T_amb */
    for (uint32_t j = 0; j < N; j++) {
        G[0 * N + j] = 0.0;
    }
    G[0 * N + 0] = 1.0;
    P[0] = network->ambient_temperature_C;

    /* Gaussian elimination with partial pivoting */
    for (uint32_t k = 0; k < N; k++) {
        /* Find pivot */
        uint32_t p = k;
        double max_val = fabs(G[k * N + k]);
        for (uint32_t i = k + 1; i < N; i++) {
            if (fabs(G[i * N + k]) > max_val) {
                max_val = fabs(G[i * N + k]);
                p = i;
            }
        }

        if (max_val < 1e-12) {
            free(G); free(P);
            return false; /* Singular matrix */
        }

        /* Swap rows */
        if (p != k) {
            for (uint32_t j = 0; j < N; j++) {
                double tmp = G[k * N + j];
                G[k * N + j] = G[p * N + j];
                G[p * N + j] = tmp;
            }
            double tmp = P[k];
            P[k] = P[p];
            P[p] = tmp;
        }

        /* Eliminate below */
        for (uint32_t i = k + 1; i < N; i++) {
            double factor = G[i * N + k] / G[k * N + k];
            for (uint32_t j = k; j < N; j++) {
                G[i * N + j] -= factor * G[k * N + j];
            }
            P[i] -= factor * P[k];
        }
    }

    /* Back substitution */
    double* T = (double*)malloc(N * sizeof(double));
    if (!T) {
        free(G); free(P);
        return false;
    }

    for (int32_t i = (int32_t)N - 1; i >= 0; i--) {
        double sum = 0.0;
        for (uint32_t j = i + 1; j < N; j++) {
            sum += G[i * N + j] * T[j];
        }
        if (fabs(G[i * N + i]) < 1e-12) {
            T[i] = network->ambient_temperature_C;
        } else {
            T[i] = (P[i] - sum) / G[i * N + i];
        }
    }

    /* Update network node temperatures */
    for (uint32_t i = 0; i < N; i++) {
        network->nodes[i].temperature_C = T[i];
    }

    free(T);
    free(G);
    free(P);
    return true;
}

double placement_thermal_junction_temp(const Component* comp,
                                        const ThermalNetwork* network)
{
    if (!comp || !network) return network ? network->ambient_temperature_C : 25.0;

    /* Find the component's node in the network */
    for (uint32_t i = 0; i < network->node_count; i++) {
        if (network->nodes[i].comp_id == comp->comp_id) {
            /* Board temperature at component location + self-heating J-C drop */
            return network->nodes[i].temperature_C
                   + comp->power_dissipation_W * comp->theta_JC_C_per_W;
        }
    }

    /* Component not in network (no power dissipation) */
    return network->ambient_temperature_C;
}

/* ============================================================================
 * Heat Spreading Analysis
 * ============================================================================ */

double placement_thermal_spreading_resistance(const Component* comp,
                                               double board_k_wpmk,
                                               double board_thickness_mm)
{
    if (!comp || board_k_wpmk <= 0.0) return 0.0;

    /* Equivalent circular source radius:
     * a = sqrt(body_width * body_height / π) */
    double area = comp->body.width * comp->body.height;
    if (area <= 0.0) return 0.0;
    double a = sqrt(area / M_PI);

    /* Board area: assume large enough for spreading */
    double board_area = 10000.0; /* 100x100mm board */
    double b = sqrt(board_area / M_PI);

    /* Thickness ratio */
    double tau = board_thickness_mm / a;

    /* Dimensionless constriction resistance for a circular source
     * on a finite-thickness flux tube.
     * Reference: Yovanovich, "Thermal Spreading and Contact Resistances",
     *            Heat Transfer Handbook, 2003. */
    double epsilon = a / b; /* Source-to-board ratio */
    double psi;

    if (tau < 1e-6) {
        /* Isothermal base (very thin board): R → 0 (perfect sink) */
        psi = 1.0;
    } else {
        /* Series solution for finite thickness:
         * ψ = 4/π * Σ [J1(λ_n * a) / (n * J0(λ_n * b)^2) * tanh(λ_n * τ)]
         * Approximated for engineering use: */
        psi = 1.0 - 1.4098 * epsilon + 0.3441 * epsilon * epsilon * epsilon
              + 0.0435 * epsilon * epsilon * epsilon * epsilon * epsilon;

        /* Thickness correction */
        double tanh_tau = tanh(M_PI * tau);
        psi *= tanh_tau / tau;
    }

    /* Spreading resistance:
     * R_spread = ψ / (4 * k * a) */
    double R_spread = psi / (4.0 * board_k_wpmk * (a * 1e-3)); /* Convert mm to m */

    return R_spread;
}

double placement_thermal_temperature_at(double x_mm, double y_mm,
                                         double source_x, double source_y,
                                         double power_W,
                                         double board_k_wpmk,
                                         double board_thickness_mm,
                                         double h_conv_Wpm2K,
                                         double ambient_C)
{
    double dx = (x_mm - source_x) * 1e-3; /* mm → m */
    double dy = (y_mm - source_y) * 1e-3;
    double r  = sqrt(dx * dx + dy * dy);

    if (r < 1e-6) r = 1e-6;

    double k = board_k_wpmk;
    double t = board_thickness_mm * 1e-3;
    double h = h_conv_Wpm2K;

    /* Characteristic length for lateral spreading */
    double L_char = sqrt(k * t / h);

    /* Temperature rise from point source on plate with convection:
     * ΔT(r) = P * K₀(r / L_char) / (π * k * t) */
    double arg = r / L_char;
    double K0;

    /* Approximation of modified Bessel function K₀ */
    if (arg < 0.1) {
        K0 = -log(arg / 2.0) - 0.5772156649; /* Euler-Mascheroni constant */
    } else if (arg < 5.0) {
        /* Polynomial approximation for 0.1 ≤ x ≤ 5 */
        double x = arg;
        K0 = exp(-x) * (1.25331414 - 0.15666418 / x
             + 0.08811128 / (x * x) - 0.09139095 / (x * x * x));
    } else {
        /* Asymptotic form for large x */
        K0 = exp(-arg) * sqrt(M_PI / (2.0 * arg));
    }

    double delta_T = power_W * K0 / (M_PI * k * t);

    return ambient_C + delta_T;
}

/* ============================================================================
 * Thermal Via Optimization
 * ============================================================================ */

double placement_thermal_via_resistance(const ThermalVia* vias,
                                         double board_thickness_mm)
{
    if (!vias || vias->via_count == 0) return 1e12; /* Very high = no conduction */

    /* Single via thermal resistance:
     * R_single = t / (k_Cu * A_cross_section)
     * where A_cross_section = π * (D_outer² - (D_outer - 2*t_plating)²) / 4
     */

    /* Copper thermal conductivity: 385 W/(m·K) */
    double k_Cu = 385.0;

    double d_outer = vias->outer_diameter_mm * 1e-3; /* mm → m */
    double t_plate = vias->plating_thickness_um * 1e-6; /* µm → m */
    double d_inner = d_outer - 2.0 * t_plate;

    if (d_inner <= 0.0) d_inner = 0.0;

    /* Cross-sectional area of the copper barrel */
    double A_cu = M_PI * (d_outer * d_outer - d_inner * d_inner) / 4.0;

    if (A_cu <= 0.0) return 1e12;

    double t = board_thickness_mm * 1e-3;

    double R_single = t / (k_Cu * A_cu);

    /* Parallel vias */
    double R_array = R_single / (double)vias->via_count;

    return R_array;
}

int32_t placement_thermal_vias_required(const Component* comp,
                                         double max_temp_C,
                                         double ambient_C,
                                         double board_k_wpmk,
                                         double board_thickness_mm,
                                         Rect2D available_area,
                                         double via_drill_mm)
{
    if (!comp || comp->power_dissipation_W <= 0.0) return 0;

    /* Temperature rise to dissipate */
    double power_W = comp->power_dissipation_W;
    double allowed_rise = max_temp_C - ambient_C;
    if (allowed_rise <= 0.0) return -1; /* Impossible: ambient already exceeds limit */

    /* Required total thermal resistance */
    double R_required = allowed_rise / power_W;

    /* Board spreading resistance (without vias) */
    double R_spread = placement_thermal_spreading_resistance(
        comp, board_k_wpmk, board_thickness_mm);

    /* If spreading already sufficient, no vias needed */
    if (R_spread <= R_required) return 0;

    /* Additional parallel resistance needed from vias:
     * 1/R_total = 1/R_spread + 1/R_vias
     * → R_vias = 1 / (1/R_required - 1/R_spread) */
    double inv_R_req = 1.0 / R_required;
    double inv_R_spread = 1.0 / R_spread;
    double inv_R_vias = inv_R_req - inv_R_spread;

    if (inv_R_vias <= 0.0) return 0;

    double R_vias_target = 1.0 / inv_R_vias;

    /* Single via resistance */
    ThermalVia tv;
    tv.drill_diameter_mm       = via_drill_mm;
    tv.outer_diameter_mm       = via_drill_mm + 0.1; /* 0.1mm annular ring */
    tv.plating_thickness_um    = 25.0; /* 1 oz copper = 35µm, typical via = 25µm */
    tv.via_count               = 1;

    double R_single = placement_thermal_via_resistance(&tv, board_thickness_mm);

    if (R_single > 1e11) return -1;

    /* Number of vias = R_single / R_vias_target (parallel) */
    int32_t n_vias = (int32_t)ceil(R_single / R_vias_target);

    /* Check if vias fit in available area */
    double via_pitch = via_drill_mm * 2.5; /* Minimum pitch: 2.5 × drill diameter */
    double max_vias_x = floor(available_area.width / via_pitch);
    double max_vias_y = floor(available_area.height / via_pitch);
    int32_t max_fit = (int32_t)(max_vias_x * max_vias_y);

    if (n_vias > max_fit) {
        /* Return negated required count to indicate it doesn't fit */
        return -n_vias;
    }

    return n_vias;
}

/* ============================================================================
 * Hot Spot Detection
 * ============================================================================ */

uint32_t placement_thermal_detect_hotspots(const PlacementResult* result,
                                            const ThermalNetwork* network,
                                            double grid_mm,
                                            Point2D* positions,
                                            uint32_t max_spots)
{
    (void)grid_mm; /* Reserved for future grid-based scanning implementation */
    if (!result || !network || max_spots == 0 || !positions) return 0;

    /* Compute mean and stddev of all node temperatures */
    uint32_t n_nodes = network->node_count;
    if (n_nodes < 2) return 0;

    double* temps = (double*)malloc(n_nodes * sizeof(double));
    if (!temps) return 0;

    /* Collect temperatures (skip ambient node 0) */
    uint32_t count = 0;
    for (uint32_t i = 1; i < n_nodes; i++) {
        temps[count++] = network->nodes[i].temperature_C;
    }

    if (count < 2) {
        free(temps);
        return 0;
    }

    double mean = placement_util_mean(temps, count);
    double std  = placement_util_stddev(temps, count);

    /* Threshold: mean + 3σ (statistical outlier) */
    double threshold = mean + 3.0 * std;

    /* Find nodes exceeding threshold */
    uint32_t hotspots = 0;
    for (uint32_t i = 1; i < n_nodes && hotspots < max_spots; i++) {
        if (network->nodes[i].temperature_C > threshold) {
            /* Find component position */
            for (uint32_t c = 0; c < result->component_count; c++) {
                if (result->components[c].comp_id == network->nodes[i].comp_id) {
                    positions[hotspots] = result->components[c].position;
                    hotspots++;
                    break;
                }
            }
        }
    }

    free(temps);
    return hotspots;
}

double placement_thermal_max_gradient(const ThermalNetwork* network)
{
    if (!network || network->node_count < 3) return 0.0;

    double max_grad = 0.0;

    for (uint32_t i = 1; i < network->node_count; i++) {
        for (uint32_t j = i + 1; j < network->node_count; j++) {
            /* Temperature difference */
            double dT = fabs(network->nodes[i].temperature_C
                           - network->nodes[j].temperature_C);

            /* Find edge between these nodes */
            double dist = 0.0;
            for (uint32_t e = 0; e < network->edge_count; e++) {
                if ((network->edges[e].from_node == i && network->edges[e].to_node == j) ||
                    (network->edges[e].from_node == j && network->edges[e].to_node == i)) {
                    /* Distance from thermal resistance: d = 1/(R * 2πk * t) approx */
                    double R = network->edges[e].resistance_CpW;
                    if (R > 0.0 && R < 1e9) {
                        double k = network->board_k_wpmk;
                        double t = network->board_thickness_mm * 1e-3;
                        dist = 1.0 / (R * 2.0 * M_PI * k * t);
                    }
                    break;
                }
            }

            if (dist > 1e-12) {
                double grad = dT / dist; /* °C/m */
                /* Convert to °C/mm */
                grad *= 1e-3;
                if (grad > max_grad) max_grad = grad;
            }
        }
    }

    return max_grad;
}
