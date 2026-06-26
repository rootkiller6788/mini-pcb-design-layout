/**
 * @file placement_strategy.c
 * @brief Implementation of PCB component placement strategy algorithms
 *
 * Contains implementation of 6 placement algorithms:
 *   1. Greedy sequential
 *   2. Simulated annealing
 *   3. Force-directed
 *   4. Partition-based min-cut bisection
 *   5. Genetic algorithm
 *   6. Clustering-based
 */

#include "placement_strategy.h"
#include "placement_optimizer.h"
#include "placement_util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Strategy Configuration
 * ============================================================================ */

void placement_strategy_config_init(StrategyConfig* config, StrategyType type)
{
    if (!config) return;
    memset(config, 0, sizeof(StrategyConfig));
    config->type = type;

    /* Default cost weights */
    config->weight_wire_length      = 0.50;
    config->weight_thermal          = 0.15;
    config->weight_signal_integrity = 0.15;
    config->weight_overlap          = 0.10;
    config->weight_density          = 0.10;

    switch (type) {
    case STRATEGY_SIMULATED_ANNEALING:
        config->config.sa.initial_temperature   = 1000.0;
        config->config.sa.final_temperature     = 0.01;
        config->config.sa.schedule              = COOLING_EXPONENTIAL;
        config->config.sa.alpha                 = 0.95;
        config->config.sa.moves_per_temperature = 100;
        config->config.sa.max_iterations        = 10000;
        config->config.sa.random_seed           = 42;
        config->config.sa.swap_probability      = 0.3;
        config->config.sa.max_move_distance     = 20.0;
        break;

    case STRATEGY_FORCE_DIRECTED:
        config->config.fd.spring_stiffness       = 0.5;
        config->config.fd.electrical_repulsion   = 5000.0;
        config->config.fd.ideal_edge_length_mm   = 15.0;
        config->config.fd.damping_factor         = 0.85;
        config->config.fd.convergence_threshold  = 0.1;
        config->config.fd.max_iterations         = 500;
        break;

    case STRATEGY_GENETIC:
        config->config.ga.population_size   = 100;
        config->config.ga.generations       = 200;
        config->config.ga.crossover_rate    = 0.8;
        config->config.ga.mutation_rate     = 0.05;
        config->config.ga.tournament_size   = 4;
        config->config.ga.elitism_fraction  = 0.1;
        config->config.ga.random_seed       = 42;
        break;

    case STRATEGY_PARTITION_BISECTION:
        config->config.partition.max_partitions          = 8;
        config->config.partition.balance_tolerance       = 0.1;
        config->config.partition.use_terminal_propagation = true;
        break;

    case STRATEGY_CLUSTERING:
        config->config.cluster.n_clusters          = 4;
        config->config.cluster.use_spectral        = false;
        config->config.cluster.connectivity_weight = 0.5;
        config->config.cluster.max_kmeans_iters    = 50;
        break;

    case STRATEGY_GREEDY:
    default:
        /* Greedy has no special config */
        break;
    }
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * Count the number of nets a component connects to.
 * Used for connectivity-based component ordering.
 */
static uint32_t component_connectivity(const Component* comp)
{
    if (!comp) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < comp->pad_count; i++) {
        if (comp->net_ids[i] != 0) count++;
    }
    return count;
}

/**
 * Generate candidate positions near a given point.
 * Candidates form a small grid plus random offsets.
 */
static void generate_candidates(const Point2D* center, uint32_t n_candidates,
                                double spread_mm,
                                Point2D* candidates,
                                RandomState* rng)
{
    for (uint32_t i = 0; i < n_candidates; i++) {
        double angle = placement_util_random_uniform(rng) * 2.0 * M_PI;
        double dist  = placement_util_random_uniform(rng) * spread_mm;
        candidates[i].x = center->x + dist * cos(angle);
        candidates[i].y = center->y + dist * sin(angle);
    }
}

/* ============================================================================
 * L5 Algorithm: Greedy Placement
 * ============================================================================ */

uint32_t placement_strategy_greedy(PlacementResult* result)
{
    if (!result || result->component_count < 1) return 0;

    uint32_t C = result->component_count;
    uint32_t placed = 0;

    /* Compute connectivity score and create order */
    typedef struct { uint32_t idx; uint32_t conn; } CompOrder;
    CompOrder* order = (CompOrder*)malloc(C * sizeof(CompOrder));
    if (!order) return 0;

    for (uint32_t i = 0; i < C; i++) {
        order[i].idx  = i;
        order[i].conn = component_connectivity(&result->components[i]);
    }

    /* Sort by connectivity descending (bubble sort for clarity) */
    for (uint32_t i = 0; i < C; i++) {
        for (uint32_t j = i + 1; j < C; j++) {
            if (order[j].conn > order[i].conn) {
                CompOrder tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    /* Place first component at board center */
    Component* first = &result->components[order[0].idx];
    double cx = result->board.outline.width  / 2.0;
    double cy = result->board.outline.height / 2.0;
    placement_component_set_position(first, cx, cy, 0.0);
    placed++;

    /* Place remaining components sequentially */
    for (uint32_t k = 1; k < C; k++) {
        Component* comp = &result->components[order[k].idx];

        /* Skip fixed components (already placed) */
        if (comp->is_fixed) {
            if (comp->is_placed) placed++;
            continue;
        }

        /* Find connected already-placed components */
        uint32_t conn_indices[64];
        uint32_t conn_count = 0;
        for (uint32_t i = 0; i < comp->pad_count && conn_count < 64; i++) {
            uint32_t net_id = comp->net_ids[i];
            if (net_id == 0) continue;
            for (uint32_t j = 0; j < C; j++) {
                if (j == order[k].idx) continue;
                if (!result->components[j].is_placed) continue;
                for (uint32_t p = 0; p < result->components[j].pad_count; p++) {
                    if (result->components[j].net_ids[p] == net_id) {
                        /* Check uniqueness */
                        bool found = false;
                        for (uint32_t m = 0; m < conn_count; m++) {
                            if (conn_indices[m] == j) { found = true; break; }
                        }
                        if (!found) conn_indices[conn_count++] = j;
                        break;
                    }
                }
            }
        }

        /* Generate candidate positions */
        Point2D candidates[64];
        uint32_t n_cand = 0;

        if (conn_count == 0) {
            /* No connected components — place near board center */
            Point2D center = {cx, cy};
            generate_candidates(&center, 20, result->board.outline.width * 0.3,
                               candidates, NULL);
            n_cand = 20;
        } else {
            /* Place near connected components */
            for (uint32_t ci = 0; ci < conn_count && n_cand < 60; ci++) {
                Point2D cpos = result->components[conn_indices[ci]].position;
                uint32_t per_conn = 20 / conn_count;
                if (per_conn < 3) per_conn = 3;
                Point2D nearby[8];
                /* Generate candidates around this connected component */
                double spread = 15.0;
                nearby[0].x = cpos.x + spread; nearby[0].y = cpos.y;
                nearby[1].x = cpos.x - spread; nearby[1].y = cpos.y;
                nearby[2].x = cpos.x;          nearby[2].y = cpos.y + spread;
                nearby[3].x = cpos.x;          nearby[3].y = cpos.y - spread;
                nearby[4].x = cpos.x + spread; nearby[4].y = cpos.y + spread;
                nearby[5].x = cpos.x - spread; nearby[5].y = cpos.y - spread;
                nearby[6].x = cpos.x + spread; nearby[6].y = cpos.y - spread;
                nearby[7].x = cpos.x - spread; nearby[7].y = cpos.y + spread;
                for (uint32_t m = 0; m < 8 && n_cand < 64; m++) {
                    candidates[n_cand++] = nearby[m];
                }
            }
        }

        /* Evaluate each candidate */
        Point2D best_pos = {cx, cy};
        double best_cost = DBL_MAX;
        bool found_legal = false;

        for (uint32_t c = 0; c < n_cand; c++) {
            if (!placement_is_position_legal(result, comp,
                                             candidates[c].x, candidates[c].y)) {
                continue;
            }

            /* Temporarily place and compute cost */
            Point2D orig = comp->position;
            bool orig_placed = comp->is_placed;
            placement_component_set_position(comp, candidates[c].x,
                                             candidates[c].y, 0.0);

            double wl = placement_estimate_wire_length(result);
            if (wl < best_cost) {
                best_cost = wl;
                best_pos = candidates[c];
                found_legal = true;
            }

            /* Restore */
            comp->position = orig;
            comp->is_placed = orig_placed;
        }

        if (found_legal) {
            placement_component_set_position(comp, best_pos.x, best_pos.y, 0.0);
            placed++;
        } else {
            /* Place at board center as fallback */
            placement_component_set_position(comp, cx, cy, 0.0);
            placed++;
        }
    }

    free(order);
    return placed;
}

/* ============================================================================
 * L5 Algorithm: Simulated Annealing
 * ============================================================================ */

uint32_t placement_strategy_simulated_annealing(PlacementResult* result,
                                                 const SAConfig* config)
{
    if (!result || !config || result->component_count < 2) return 0;

    uint32_t C = result->component_count;
    RandomState rng;
    placement_util_random_init(&rng, config->random_seed);

    /* Ensure all components have initial positions */
    for (uint32_t i = 0; i < C; i++) {
        if (!result->components[i].is_placed && !result->components[i].is_fixed) {
            double bx = result->board.outline.width;
            double by = result->board.outline.height;
            double x = placement_util_random_uniform(&rng) * bx;
            double y = placement_util_random_uniform(&rng) * by;
            placement_component_set_position(&result->components[i], x, y, 0.0);
        }
    }

    /* Compute initial cost */
    double current_cost = placement_estimate_wire_length(result);
    double best_cost = current_cost;

    /* Save best positions */
    Point2D* best_positions = (Point2D*)malloc(C * sizeof(Point2D));
    if (!best_positions) return 0;
    for (uint32_t i = 0; i < C; i++) {
        best_positions[i] = result->components[i].position;
    }

    /* Initialize temperature */
    double T = config->initial_temperature;
    double T_final = config->final_temperature;
    uint32_t iter = 0;
    uint32_t accepted = 0;

    while (T > T_final && iter < config->max_iterations) {
        for (uint32_t m = 0; m < config->moves_per_temperature; m++) {
            iter++;

            /* Select perturbation type */
            double r = placement_util_random_uniform(&rng);

            if (r < config->swap_probability) {
                /* --- Swap two components --- */
                uint32_t a = placement_util_random_int(&rng, 0, (int32_t)C - 1);
                uint32_t b = placement_util_random_int(&rng, 0, (int32_t)C - 1);
                if (a == b) continue;
                if (result->components[a].is_fixed || result->components[b].is_fixed)
                    continue;

                /* Swap positions */
                Point2D tmp = result->components[a].position;
                result->components[a].position = result->components[b].position;
                result->components[b].position = tmp;

                double new_cost = placement_estimate_wire_length(result);
                double delta = new_cost - current_cost;

                if (delta <= 0.0 || placement_util_random_uniform(&rng) < exp(-delta / T)) {
                    /* Accept swap */
                    current_cost = new_cost;
                    accepted++;
                    if (current_cost < best_cost) {
                        best_cost = current_cost;
                        for (uint32_t i = 0; i < C; i++) {
                            best_positions[i] = result->components[i].position;
                        }
                    }
                } else {
                    /* Reject — swap back */
                    tmp = result->components[a].position;
                    result->components[a].position = result->components[b].position;
                    result->components[b].position = tmp;
                }
            } else {
                /* --- Move single component --- */
                uint32_t idx = placement_util_random_int(&rng, 0, (int32_t)C - 1);
                if (result->components[idx].is_fixed) continue;

                Point2D orig = result->components[idx].position;
                double dx = (placement_util_random_uniform(&rng) - 0.5)
                            * 2.0 * config->max_move_distance;
                double dy = (placement_util_random_uniform(&rng) - 0.5)
                            * 2.0 * config->max_move_distance;

                double nx = orig.x + dx;
                double ny = orig.y + dy;
                /* Clamp to board */
                double r2 = result->components[idx].body.width / 2.0;
                if (nx < r2) nx = r2;
                if (ny < r2) ny = r2;
                if (nx > result->board.outline.width - r2)
                    nx = result->board.outline.width - r2;
                if (ny > result->board.outline.height - r2)
                    ny = result->board.outline.height - r2;

                result->components[idx].position.x = nx;
                result->components[idx].position.y = ny;

                double new_cost = placement_estimate_wire_length(result);
                double delta = new_cost - current_cost;

                if (delta <= 0.0 || placement_util_random_uniform(&rng) < exp(-delta / T)) {
                    current_cost = new_cost;
                    accepted++;
                    if (current_cost < best_cost) {
                        best_cost = current_cost;
                        for (uint32_t i = 0; i < C; i++) {
                            best_positions[i] = result->components[i].position;
                        }
                    }
                } else {
                    result->components[idx].position = orig;
                }
            }
        }

        /* Cool down */
        switch (config->schedule) {
        case COOLING_LINEAR:
            T -= (config->initial_temperature - config->final_temperature)
                 / (double)(config->max_iterations / config->moves_per_temperature);
            break;
        case COOLING_EXPONENTIAL:
            T *= config->alpha;
            break;
        case COOLING_LOGARITHMIC:
            T = config->initial_temperature / log(1.0 + (double)iter);
            break;
        default:
            T *= config->alpha;
            break;
        }
    }

    /* Restore best placement found */
    for (uint32_t i = 0; i < C; i++) {
        result->components[i].position = best_positions[i];
    }

    result->cost.total_cost = best_cost;
    result->iterations = iter;
    free(best_positions);
    return iter;
}

/* ============================================================================
 * L5 Algorithm: Force-Directed Placement
 * ============================================================================ */

uint32_t placement_strategy_force_directed(PlacementResult* result,
                                            const FDConfig* config)
{
    if (!result || !config || result->component_count < 2) return 0;

    uint32_t C = result->component_count;

    /* Allocate velocity vectors */
    typedef struct { double vx, vy; } Velocity;
    Velocity* velocities = (Velocity*)calloc(C, sizeof(Velocity));
    if (!velocities) return 0;

    /* Initialize positions randomly if not placed */
    RandomState rng;
    placement_util_random_init(&rng, 42);

    for (uint32_t i = 0; i < C; i++) {
        if (!result->components[i].is_placed && !result->components[i].is_fixed) {
            double bx = result->board.outline.width;
            double by = result->board.outline.height;
            double x = placement_util_random_uniform(&rng) * bx;
            double y = placement_util_random_uniform(&rng) * by;
            placement_component_set_position(&result->components[i], x, y, 0.0);
        }
    }

    uint32_t iter;
    for (iter = 0; iter < config->max_iterations; iter++) {
        double max_displacement = 0.0;

        for (uint32_t i = 0; i < C; i++) {
            Component* ci = &result->components[i];
            if (ci->is_fixed) continue;

            double fx = 0.0, fy = 0.0;

            for (uint32_t j = 0; j < C; j++) {
                if (i == j) continue;
                Component* cj = &result->components[j];
                if (!cj->is_placed) continue;

                double dx = ci->position.x - cj->position.x;
                double dy = ci->position.y - cj->position.y;
                double dist = sqrt(dx * dx + dy * dy);
                if (dist < 1e-6) dist = 1e-6; /* Avoid singularity */

                /* Check if i and j share a net → spring force */
                bool connected = false;
                for (uint32_t pi = 0; pi < ci->pad_count && !connected; pi++) {
                    if (ci->net_ids[pi] == 0) continue;
                    for (uint32_t pj = 0; pj < cj->pad_count && !connected; pj++) {
                        if (ci->net_ids[pi] == cj->net_ids[pj]) {
                            connected = true;
                        }
                    }
                }

                if (connected) {
                    /* Attractive spring force: F = K_s * (d - L_ideal) */
                    double force_mag = config->spring_stiffness
                                       * (dist - config->ideal_edge_length_mm);
                    fx -= force_mag * dx / dist;
                    fy -= force_mag * dy / dist;
                }

                /* Repulsive Coulomb-like force: F = K_r / d² */
                double force_repel = config->electrical_repulsion / (dist * dist);
                fx += force_repel * dx / dist;
                fy += force_repel * dy / dist;
            }

            /* Update velocity with damping */
            velocities[i].vx = config->damping_factor
                               * (velocities[i].vx + fx);
            velocities[i].vy = config->damping_factor
                               * (velocities[i].vy + fy);

            /* Cap velocity to prevent instability */
            double vmag = sqrt(velocities[i].vx * velocities[i].vx
                             + velocities[i].vy * velocities[i].vy);
            double vmax = 10.0;
            if (vmag > vmax) {
                velocities[i].vx *= vmax / vmag;
                velocities[i].vy *= vmax / vmag;
            }

            /* Update position */
            double nx = ci->position.x + velocities[i].vx;
            double ny = ci->position.y + velocities[i].vy;

            /* Clamp to board */
            double half_w = ci->body.width / 2.0;
            double half_h = ci->body.height / 2.0;
            if (nx < half_w) nx = half_w;
            if (nx > result->board.outline.width - half_w)
                nx = result->board.outline.width - half_w;
            if (ny < half_h) ny = half_h;
            if (ny > result->board.outline.height - half_h)
                ny = result->board.outline.height - half_h;

            double disp = sqrt((nx - ci->position.x) * (nx - ci->position.x)
                             + (ny - ci->position.y) * (ny - ci->position.y));
            if (disp > max_displacement) max_displacement = disp;

            ci->position.x = nx;
            ci->position.y = ny;
        }

        if (max_displacement < config->convergence_threshold) {
            iter++;
            break;
        }
    }

    result->cost.total_cost = placement_estimate_wire_length(result);
    result->iterations = iter;
    free(velocities);
    return iter;
}

/* ============================================================================
 * L5 Algorithm: Partition-Based Min-Cut Bisection (Fiduccia-Mattheyses)
 * ============================================================================ */

/**
 * Internal FM pass: find best sequence of cell moves.
 * Returns the best cut size achieved during the pass.
 */
static uint32_t fm_pass(uint32_t* partition, const uint32_t* cell_pins,
                         const uint32_t* cell_pin_counts,
                         const uint32_t* net_cells,
                         const uint32_t* net_cell_counts,
                         uint32_t n_cells, uint32_t n_nets,
                         double balance_tol, double* cell_areas)
{
    /* Gain computation for moving each cell */
    int32_t* gains = (int32_t*)calloc(n_cells, sizeof(int32_t));
    if (!gains) return UINT32_MAX;

    /* Initial gain: FS(c) - TE(c) for each cell */
    for (uint32_t c = 0; c < n_cells; c++) {
        int32_t gain = 0;
        uint32_t side = partition[c];

        for (uint32_t pi = 0; pi < cell_pin_counts[c]; pi++) {
            uint32_t net_id = cell_pins[c * 32 + pi];
            if (net_id >= n_nets) continue;

            /* Count cells on each side of this net */
            uint32_t cnt_a = 0, cnt_b = 0;
            for (uint32_t nc = 0; nc < net_cell_counts[net_id]; nc++) {
                uint32_t oc = net_cells[net_id * 32 + nc];
                if (partition[oc] == 0) cnt_a++; else cnt_b++;
            }

            if (side == 0 && cnt_a == 1) gain++; /* FS: this cell is sole occupant on side A */
            if (side == 1 && cnt_b == 1) gain++; /* FS: sole occupant on side B */
            if (cnt_b == 0) gain--;              /* TE: all cells on A already */
            if (cnt_a == 0) gain--;              /* TE: all cells on B already */
        }
        gains[c] = gain;
    }

    /* FM move loop */
    bool* locked = (bool*)calloc(n_cells, sizeof(bool));
    uint32_t init_cut = 0;
    /* Compute initial cut */
    for (uint32_t n = 0; n < n_nets; n++) {
        uint32_t cnt_a = 0, cnt_b = 0;
        for (uint32_t nc = 0; nc < net_cell_counts[n]; nc++) {
            uint32_t oc = net_cells[n * 32 + nc];
            if (partition[oc] == 0) cnt_a++; else cnt_b++;
        }
        if (cnt_a > 0 && cnt_b > 0) init_cut++;
    }
    /* Balance tracking */
    double total_area = 0.0, area_a = 0.0, area_b = 0.0;
    for (uint32_t c = 0; c < n_cells; c++) {
        total_area += cell_areas[c];
        if (partition[c] == 0) area_a += cell_areas[c];
        else area_b += cell_areas[c];
    }
    double max_imbalance = total_area * (0.5 + balance_tol);

    uint32_t pass_best_cut = init_cut;
    for (uint32_t move_count = 0; move_count < n_cells; move_count++) {
        /* Find unlocked cell with highest gain that maintains balance */
        int32_t best_gain = INT32_MIN;
        uint32_t best_cell = 0;
        bool found = false;

        for (uint32_t c = 0; c < n_cells; c++) {
            if (locked[c]) continue;
            /* Check balance */
            double new_area_a = area_a;
            double new_area_b = area_b;
            if (partition[c] == 0) {
                new_area_a -= cell_areas[c];
                new_area_b += cell_areas[c];
            } else {
                new_area_a += cell_areas[c];
                new_area_b -= cell_areas[c];
            }
            if (new_area_a > max_imbalance || new_area_b > max_imbalance) continue;

            if (gains[c] > best_gain) {
                best_gain = gains[c];
                best_cell = c;
                found = true;
            }
        }

        if (!found) break; /* No legal moves remaining */

        /* Execute move */
        locked[best_cell] = true;
        partition[best_cell] = 1 - partition[best_cell];
        if (partition[best_cell] == 1) {
            area_a -= cell_areas[best_cell];
            area_b += cell_areas[best_cell];
        } else {
            area_a += cell_areas[best_cell];
            area_b -= cell_areas[best_cell];
        }

        /* Update cut */
        uint32_t cut = 0;
        for (uint32_t n = 0; n < n_nets; n++) {
            uint32_t cnt_a = 0, cnt_b = 0;
            for (uint32_t nc = 0; nc < net_cell_counts[n]; nc++) {
                uint32_t oc = net_cells[n * 32 + nc];
                if (partition[oc] == 0) cnt_a++; else cnt_b++;
            }
            if (cnt_a > 0 && cnt_b > 0) cut++;
        }
        if (cut < pass_best_cut) pass_best_cut = cut;

        /* Update gains for neighbors */
        for (uint32_t pi = 0; pi < cell_pin_counts[best_cell]; pi++) {
            uint32_t net_id = cell_pins[best_cell * 32 + pi];
            if (net_id >= n_nets) continue;
            for (uint32_t nc = 0; nc < net_cell_counts[net_id]; nc++) {
                uint32_t oc = net_cells[net_id * 32 + nc];
                if (locked[oc]) continue;
                /* Recompute gain for this cell (simplified) */
                /* In a full FM, we'd incrementally update; here we do a full recompute */
            }
        }
        /* Recompute all unlocked gains (full recompute for clarity) */
        for (uint32_t c = 0; c < n_cells; c++) {
            if (locked[c]) continue;
            int32_t gain = 0;
            uint32_t side = partition[c];
            for (uint32_t pi = 0; pi < cell_pin_counts[c]; pi++) {
                uint32_t net_id = cell_pins[c * 32 + pi];
                if (net_id >= n_nets) continue;
                uint32_t cnt_a = 0, cnt_b = 0;
                for (uint32_t nc2 = 0; nc2 < net_cell_counts[net_id]; nc2++) {
                    uint32_t oc2 = net_cells[net_id * 32 + nc2];
                    if (partition[oc2] == 0) cnt_a++; else cnt_b++;
                }
                if (side == 0 && cnt_a == 1) gain++;
                if (side == 1 && cnt_b == 1) gain++;
                if (cnt_b == 0) gain--;
                if (cnt_a == 0) gain--;
            }
            gains[c] = gain;
        }
    }

    free(gains);
    free(locked);
    return pass_best_cut;
}

uint32_t placement_strategy_partition_bisection(PlacementResult* result,
                                                 const PartitionConfig* config)
{
    if (!result || !config || result->component_count < 2) return 0;

    uint32_t C = result->component_count;
    uint32_t N = result->net_count;
    if (N == 0 || C == 0) return 0;

    /* Build connectivity data structures */
    /* Simplified: assign cell_pins by scanning nets */
    uint32_t* partition = (uint32_t*)calloc(C, sizeof(uint32_t));
    double* cell_areas = (double*)calloc(C, sizeof(double));
    uint32_t* cell_pins = (uint32_t*)calloc(C * 32, sizeof(uint32_t));
    uint32_t* cell_pin_counts = (uint32_t*)calloc(C, sizeof(uint32_t));

    if (!partition || !cell_areas || !cell_pins || !cell_pin_counts) {
        free(partition); free(cell_areas); free(cell_pins); free(cell_pin_counts);
        return 0;
    }

    for (uint32_t c = 0; c < C; c++) {
        cell_areas[c] = result->components[c].body.width
                       * result->components[c].body.height;
        for (uint32_t p = 0; p < result->components[c].pad_count; p++) {
            if (cell_pin_counts[c] < 32) {
                cell_pins[c * 32 + cell_pin_counts[c]] = result->components[c].net_ids[p];
                cell_pin_counts[c]++;
            }
        }
        /* Random initial partition */
        partition[c] = placement_util_random_int(NULL, 0, 1);
    }

    /* Build net-to-cell mapping */
    uint32_t* net_cells = (uint32_t*)calloc(N * 32, sizeof(uint32_t));
    uint32_t* net_cell_counts = (uint32_t*)calloc(N, sizeof(uint32_t));
    for (uint32_t n = 0; n < N; n++) {
        for (uint32_t c = 0; c < C; c++) {
            for (uint32_t p = 0; p < result->components[c].pad_count; p++) {
                if (result->components[c].net_ids[p] == result->nets[n].net_id) {
                    if (net_cell_counts[n] < 32) {
                        net_cells[n * 32 + net_cell_counts[n]] = c;
                        net_cell_counts[n]++;
                    }
                    break;
                }
            }
        }
    }

    /* Run FM pass */
    uint32_t best_cut = fm_pass(partition, cell_pins, cell_pin_counts,
                                 net_cells, net_cell_counts, C, N,
                                 config->balance_tolerance, cell_areas);

    /* Assign positions based on partition */
    double bw = result->board.outline.width;
    double bh = result->board.outline.height;
    for (uint32_t c = 0; c < C; c++) {
        double x, y;
        if (partition[c] == 0) {
            /* Left half */
            x = bw * 0.25;
            y = placement_util_random_uniform(NULL) * bh;
        } else {
            /* Right half */
            x = bw * 0.75;
            y = placement_util_random_uniform(NULL) * bh;
        }
        placement_component_set_position(&result->components[c], x, y, 0.0);
    }

    result->cost.total_cost = (double)best_cut;
    result->iterations = 1;

    free(partition); free(cell_areas);
    free(cell_pins); free(cell_pin_counts);
    free(net_cells); free(net_cell_counts);
    return 1;
}

/* ============================================================================
 * L5 Algorithm: Genetic Algorithm
 * ============================================================================ */

typedef struct {
    double* genome;   /* Array of 3*C values: x0,y0,r0, x1,y1,r1, ... */
    double  fitness;
} Individual;

typedef struct {
    Individual* pop;
    uint32_t    size;
    uint32_t    genome_len; /* 3 * n_components */
    RandomState rng;
} Population;

static double ga_compute_fitness(const double* genome, uint32_t n_comp,
                                  PlacementResult* result)
{
    /* Apply genome to placement */
    for (uint32_t i = 0; i < n_comp; i++) {
        if (result->components[i].is_fixed) continue;
        placement_component_set_position(&result->components[i],
                                         genome[3*i], genome[3*i+1],
                                         genome[3*i+2]);
    }

    /* Fitness = 1 / (1 + wire_length)  — higher is better */
    double wl = placement_estimate_wire_length(result);
    return 1.0 / (1.0 + wl);
}

static Individual ga_tournament_select(const Population* pop, uint32_t t_size)
{
    Individual best;
    best.fitness = -1.0;
    for (uint32_t i = 0; i < t_size; i++) {
        uint32_t idx = placement_util_random_int((RandomState*)&pop->rng, 0,
                                                   (int32_t)(pop->size - 1));
        if (pop->pop[idx].fitness > best.fitness) {
            best = pop->pop[idx];
        }
    }
    return best;
}

static void ga_crossover(const Individual* parent1, const Individual* parent2,
                          Individual* child1, Individual* child2,
                          uint32_t genome_len, RandomState* rng)
{
    /* Uniform crossover: each gene from random parent */
    for (uint32_t g = 0; g < genome_len; g++) {
        if (placement_util_random_uniform(rng) < 0.5) {
            child1->genome[g] = parent1->genome[g];
            child2->genome[g] = parent2->genome[g];
        } else {
            child1->genome[g] = parent2->genome[g];
            child2->genome[g] = parent1->genome[g];
        }
    }
}

static void ga_mutate(Individual* ind, uint32_t genome_len,
                       double rate, RandomState* rng,
                       double board_w, double board_h)
{
    for (uint32_t g = 0; g < genome_len; g += 3) {
        if (placement_util_random_uniform(rng) < rate) {
            ind->genome[g]   = placement_util_random_uniform(rng) * board_w;
            ind->genome[g+1] = placement_util_random_uniform(rng) * board_h;
        }
        if (placement_util_random_uniform(rng) < rate) {
            ind->genome[g+2] = ((double)placement_util_random_int(rng, 0, 3)) * 90.0;
        }
    }
}

uint32_t placement_strategy_genetic_algorithm(PlacementResult* result,
                                               const GAConfig* config)
{
    if (!result || !config || result->component_count < 2) return 0;

    uint32_t C = result->component_count;
    uint32_t genome_len = 3 * C;
    double bw = result->board.outline.width;
    double bh = result->board.outline.height;

    Population pop;
    pop.size = config->population_size;
    pop.genome_len = genome_len;
    placement_util_random_init(&pop.rng, config->random_seed);

    /* Allocate population */
    pop.pop = (Individual*)malloc(pop.size * sizeof(Individual));
    if (!pop.pop) return 0;
    for (uint32_t i = 0; i < pop.size; i++) {
        pop.pop[i].genome = (double*)malloc(genome_len * sizeof(double));
        if (!pop.pop[i].genome) {
            for (uint32_t j = 0; j < i; j++) free(pop.pop[j].genome);
            free(pop.pop);
            return 0;
        }
    }

    /* Initialize population randomly */
    for (uint32_t i = 0; i < pop.size; i++) {
        for (uint32_t g = 0; g < genome_len; g += 3) {
            pop.pop[i].genome[g]   = placement_util_random_uniform(&pop.rng) * bw;
            pop.pop[i].genome[g+1] = placement_util_random_uniform(&pop.rng) * bh;
            pop.pop[i].genome[g+2] = ((double)placement_util_random_int(&pop.rng, 0, 3)) * 90.0;
        }
        pop.pop[i].fitness = ga_compute_fitness(pop.pop[i].genome, C, result);
    }

    /* Evolution loop */
    for (uint32_t gen = 0; gen < config->generations; gen++) {
        /* Sort by fitness descending */
        for (uint32_t i = 0; i < pop.size - 1; i++) {
            for (uint32_t j = i + 1; j < pop.size; j++) {
                if (pop.pop[j].fitness > pop.pop[i].fitness) {
                    Individual tmp = pop.pop[i];
                    pop.pop[i] = pop.pop[j];
                    pop.pop[j] = tmp;
                }
            }
        }

        uint32_t elitism_count = (uint32_t)(pop.size * config->elitism_fraction);
        if (elitism_count < 1) elitism_count = 1;

        /* Create new population */
        Individual* new_pop = (Individual*)malloc(pop.size * sizeof(Individual));
        if (!new_pop) continue;
        for (uint32_t i = 0; i < pop.size; i++) {
            new_pop[i].genome = (double*)malloc(genome_len * sizeof(double));
        }

        /* Elitism: keep best */
        for (uint32_t i = 0; i < elitism_count; i++) {
            memcpy(new_pop[i].genome, pop.pop[i].genome,
                   genome_len * sizeof(double));
            new_pop[i].fitness = pop.pop[i].fitness;
        }

        /* Generate rest via crossover and mutation */
        for (uint32_t i = elitism_count; i < pop.size; i += 2) {
            Individual parent1 = ga_tournament_select(&pop, config->tournament_size);
            Individual parent2 = ga_tournament_select(&pop, config->tournament_size);

            if (placement_util_random_uniform(&pop.rng) < config->crossover_rate) {
                ga_crossover(&parent1, &parent2, &new_pop[i],
                             (i + 1 < pop.size) ? &new_pop[i+1] : NULL,
                             genome_len, &pop.rng);
            } else {
                memcpy(new_pop[i].genome, parent1.genome,
                       genome_len * sizeof(double));
                if (i + 1 < pop.size) {
                    memcpy(new_pop[i+1].genome, parent2.genome,
                           genome_len * sizeof(double));
                }
            }

            ga_mutate(&new_pop[i], genome_len, config->mutation_rate,
                      &pop.rng, bw, bh);
            if (i + 1 < pop.size) {
                ga_mutate(&new_pop[i+1], genome_len, config->mutation_rate,
                          &pop.rng, bw, bh);
            }
        }

        /* Compute fitness for new population */
        for (uint32_t i = elitism_count; i < pop.size; i++) {
            new_pop[i].fitness = ga_compute_fitness(new_pop[i].genome, C, result);
        }

        /* Replace old population */
        for (uint32_t i = 0; i < pop.size; i++) {
            free(pop.pop[i].genome);
        }
        free(pop.pop);
        pop.pop = new_pop;
    }

    /* Apply best individual */
    /* Re-sort */
    for (uint32_t i = 0; i < pop.size - 1; i++) {
        for (uint32_t j = i + 1; j < pop.size; j++) {
            if (pop.pop[j].fitness > pop.pop[i].fitness) {
                Individual tmp = pop.pop[i];
                pop.pop[i] = pop.pop[j];
                pop.pop[j] = tmp;
            }
        }
    }

    for (uint32_t i = 0; i < C; i++) {
        if (result->components[i].is_fixed) continue;
        placement_component_set_position(&result->components[i],
                                         pop.pop[0].genome[3*i],
                                         pop.pop[0].genome[3*i+1],
                                         pop.pop[0].genome[3*i+2]);
    }

    result->cost.total_cost = 1.0 / pop.pop[0].fitness - 1.0;
    result->iterations = config->generations;

    for (uint32_t i = 0; i < pop.size; i++) free(pop.pop[i].genome);
    free(pop.pop);
    return config->generations;
}

/* ============================================================================
 * L5 Algorithm: Clustering-Based Placement
 * ============================================================================ */

uint32_t placement_strategy_clustering(PlacementResult* result,
                                        const ClusterConfig* config)
{
    if (!result || !config || result->component_count < 2) return 0;

    uint32_t C = result->component_count;
    uint32_t K = config->n_clusters;
    if (K > C) K = C;

    /* Initialize cluster centroids randomly */
    Point2D* centroids = (Point2D*)malloc(K * sizeof(Point2D));
    uint32_t* assignments = (uint32_t*)malloc(C * sizeof(uint32_t));
    if (!centroids || !assignments) {
        free(centroids); free(assignments);
        return 0;
    }

    /* Random initial positions for centroids within board */
    RandomState rng;
    placement_util_random_init(&rng, 42);
    double bw = result->board.outline.width;
    double bh = result->board.outline.height;
    for (uint32_t k = 0; k < K; k++) {
        centroids[k].x = placement_util_random_uniform(&rng) * bw;
        centroids[k].y = placement_util_random_uniform(&rng) * bh;
    }

    /* K-means iterations */
    for (uint32_t iter = 0; iter < config->max_kmeans_iters; iter++) {
        bool changed = false;

        /* Assignment step: assign each component to nearest centroid */
        for (uint32_t c = 0; c < C; c++) {
            double min_dist = DBL_MAX;
            uint32_t best_k = 0;
            Point2D cpos = result->components[c].position;

            for (uint32_t k = 0; k < K; k++) {
                double dx = cpos.x - centroids[k].x;
                double dy = cpos.y - centroids[k].y;
                double dist = sqrt(dx * dx + dy * dy);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_k = k;
                }
            }

            if (assignments[c] != best_k) {
                assignments[c] = best_k;
                changed = true;
            }
        }

        if (!changed) break;

        /* Update step: recompute centroids */
        for (uint32_t k = 0; k < K; k++) {
            double sum_x = 0.0, sum_y = 0.0;
            uint32_t count = 0;
            for (uint32_t c = 0; c < C; c++) {
                if (assignments[c] == k) {
                    sum_x += result->components[c].position.x;
                    sum_y += result->components[c].position.y;
                    count++;
                }
            }
            if (count > 0) {
                centroids[k].x = sum_x / count;
                centroids[k].y = sum_y / count;
            }
        }
    }

    /* Place clusters in a grid layout on the board */
    uint32_t cols = (uint32_t)ceil(sqrt((double)K));
    double cell_w = bw / cols;
    double cell_h = bh / ((K + cols - 1) / cols);

    for (uint32_t k = 0; k < K; k++) {
        uint32_t row = k / cols;
        uint32_t col = k % cols;
        double base_x = col * cell_w + cell_w / 2.0;
        double base_y = row * cell_h + cell_h / 2.0;

        /* Place all components in this cluster around the cluster center */
        uint32_t in_cluster = 0;
        for (uint32_t c = 0; c < C; c++) {
            if (assignments[c] == k) {
                double offset_x = (placement_util_random_uniform(&rng) - 0.5)
                                  * cell_w * 0.6;
                double offset_y = (placement_util_random_uniform(&rng) - 0.5)
                                  * cell_h * 0.6;
                placement_component_set_position(&result->components[c],
                                                  base_x + offset_x,
                                                  base_y + offset_y, 0.0);
                in_cluster++;
            }
        }
    }

    result->cost.total_cost = placement_estimate_wire_length(result);
    result->iterations = config->max_kmeans_iters;
    free(centroids);
    free(assignments);
    return K;
}

/* ============================================================================
 * Strategy Dispatch
 * ============================================================================ */

uint32_t placement_strategy_execute(PlacementResult* result,
                                     const StrategyConfig* config)
{
    if (!result || !config) return 0;

    switch (config->type) {
    case STRATEGY_GREEDY:
        return placement_strategy_greedy(result);
    case STRATEGY_SIMULATED_ANNEALING:
        return placement_strategy_simulated_annealing(result, &config->config.sa);
    case STRATEGY_FORCE_DIRECTED:
        return placement_strategy_force_directed(result, &config->config.fd);
    case STRATEGY_PARTITION_BISECTION:
        return placement_strategy_partition_bisection(result, &config->config.partition);
    case STRATEGY_GENETIC:
        return placement_strategy_genetic_algorithm(result, &config->config.ga);
    case STRATEGY_CLUSTERING:
        return placement_strategy_clustering(result, &config->config.cluster);
    default:
        return 0;
    }
}
