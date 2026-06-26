/**
 * @file    example_yield_cost.c
 * @brief   Example: Yield-Cost Trade-off Analysis
 *
 * Demonstrates the critical relationship between manufacturing yield,
 * defect density, and PCB cost. Shows how to:
 * 1. Estimate yield under different models for a given design
 * 2. Compute required defect density for target yield
 * 3. Optimize layer count for minimum cost
 * 4. Analyze cost sensitivity to yield improvement
 * 5. Quantity discount effects on total cost
 *
 * Real-world scenario: deciding whether to invest in a cleaner
 * manufacturing process (lower D) vs accepting lower yield.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "dfm_yield.h"
#include "dfm_cost.h"
#include "dfm_core.h"

int main(void)
{
    printf("\n");
    printf("====================================================\n");
    printf("  Yield-Cost Trade-off Analysis\n");
    printf("====================================================\n");

    /* Design parameters */
    double board_area_mm2 = 12000.0; /* 150x80mm board */
    double total_trace_cm = 800.0;   /* 8 meters total routing */
    double min_spacing_um = 100.0;
    double min_trace_um = 100.0;
    int quantity = 5000;

    /* Compute critical area */
    double crit_area = compute_critical_area(
        total_trace_cm, min_spacing_um, min_trace_um,
        board_area_mm2 * 0.01);
    printf("\nCritical Area: %.2f cm^2\n", crit_area);
    printf("(Board area: %.1f cm^2)\n\n", board_area_mm2 * 0.01);

    /* ---- Section 1: Yield vs Defect Density ---- */
    printf("--- Section 1: Yield vs Defect Density ---\n");
    printf("Defect Density | Poisson  | Murphy   | Seeds    | NegBin(a=2)\n");
    printf("(def/cm^2)     | Yield%%   | Yield%%   | Yield%%   | Yield%%\n");
    printf("---------------|----------|----------|----------|------------\n");

    double densities[] = {0.01, 0.02, 0.05, 0.10, 0.20, 0.50, 1.0};
    for (int i = 0; i < 7; i++) {
        double D = densities[i];
        double Yp = yield_poisson(crit_area, D) * 100.0;
        double Ym = yield_murphy(crit_area, D) * 100.0;
        double Ys = yield_seeds(crit_area, D) * 100.0;
        double Yn = yield_neg_binomial(crit_area, D, 2.0) * 100.0;
        printf("  %7.3f      | %6.1f   | %6.1f   | %6.1f   | %6.1f\n",
               D, Yp, Ym, Ys, Yn);
    }

    /* ---- Section 2: Required Defect Density ---- */
    printf("\n--- Section 2: Required Defect Density for Target Yield ---\n");
    printf("Target Yield | Required D (Poisson) | Notes\n");
    printf("-------------|----------------------|----------------------\n");

    double targets[] = {0.99, 0.95, 0.90, 0.80, 0.70};
    for (int i = 0; i < 5; i++) {
        double D_req = estimate_required_defect_density(targets[i], crit_area);
        const char *note;
        if (D_req < 0.02) {
            note = "Clean room required";
        } else if (D_req < 0.10) {
            note = "Standard fab, good process";
        } else if (D_req < 0.30) {
            note = "Relaxed process, low cost";
        } else {
            note = "Very low quality fab";
        }
        printf("  %.0f%%         | %.4f def/cm^2        | %s\n",
               targets[i] * 100.0, D_req, note);
    }

    /* ---- Section 3: Layer Count Optimization ---- */
    printf("\n--- Section 3: Layer Count Optimization ---\n");
    printf("Layers | Area (mm^2) | Cost/Board | Yield (Murphy) | Good Board Cost\n");
    printf("-------|-------------|------------|----------------|----------------\n");

    for (int L = 2; L <= 12; L += 2) {
        /* Estimate area needed for routing density */
        double area_L = board_area_mm2 * sqrt(2.0 / (double)L);
        double A_crit_L = crit_area * sqrt(2.0 / (double)L);

        cost_breakdown_t cost = estimate_pcb_cost(
            area_L, L, IPC_CLASS_2, FINISH_ENIG, CU_WEIGHT_1_0_OZ,
            quantity, false);

        double Y_murphy = yield_murphy(A_crit_L, 0.05);

        /* Good board cost = cost_per_board / yield */
        double good_cost = cost.cost_per_board / Y_murphy;

        printf("  %-2d    | %-8.0f   | $%-8.2f | %-12.1f%% | $%-14.2f\n",
               L, area_L, cost.cost_per_board, Y_murphy * 100.0, good_cost);
    }

    /* ---- Section 4: Quantity Discount Analysis ---- */
    printf("\n--- Section 4: Quantity Discount (Learning Curve) ---\n");
    printf("Quantity | Discount Factor | Effective Per-Board Cost\n");
    printf("---------|-----------------|-------------------------\n");

    double base_cost = estimate_pcb_cost(
        board_area_mm2, 4, IPC_CLASS_2, FINISH_ENIG,
        CU_WEIGHT_1_0_OZ, 1, false).cost_per_board;

    int qtys[] = {1, 10, 50, 100, 500, 1000, 5000, 10000};
    for (int i = 0; i < 8; i++) {
        double factor = compute_quantity_discount(qtys[i], 0.90);
        double effective = base_cost * factor;
        printf("  %-6d  | %.4f           | $%.2f\n",
               qtys[i], factor, effective);
    }

    /* ---- Section 5: YIELD-COST Sensitivity ---- */
    printf("\n--- Section 5: Cost of Yield Improvement ---\n");
    printf("Improving from D=0.10 to D=0.05 defect/cm^2\n");
    printf("(requires cleaner process, better materials, more inspection)\n\n");

    double Y_improved = yield_murphy(crit_area, 0.05);
    double Y_baseline  = yield_murphy(crit_area, 0.10);

    cost_breakdown_t cost_baseline = estimate_pcb_cost(
        board_area_mm2, 4, IPC_CLASS_2, FINISH_ENIG,
        CU_WEIGHT_1_0_OZ, quantity, false);
    /* Lower defect density adds ~20% to processing cost */
    cost_breakdown_t cost_improved = estimate_pcb_cost(
        board_area_mm2, 4, IPC_CLASS_3, FINISH_ENIG,
        CU_WEIGHT_1_0_OZ, quantity, false);

    double good_cost_baseline = cost_baseline.cost_per_board / Y_baseline;
    double good_cost_improved = cost_improved.cost_per_board / Y_improved;

    printf("                 Baseline (D=0.10)   Improved (D=0.05)\n");
    printf("Yield:           %.1f%%               %.1f%%\n",
           Y_baseline * 100.0, Y_improved * 100.0);
    printf("Fab cost/board:  $%.2f              $%.2f\n",
           cost_baseline.cost_per_board, cost_improved.cost_per_board);
    printf("Good board cost: $%.2f              $%.2f\n",
           good_cost_baseline, good_cost_improved);
    printf("Net savings:     $%.2f/board\n",
           good_cost_baseline - good_cost_improved);

    /* ---- Summary ---- */
    printf("\n====================================================\n");
    printf("  Recommendations:\n");
    yield_result_t final_yield = yield_compute_full(crit_area, 0.05, 2.0);
    printf("  1. Conservative yield estimate: %.1f%%\n",
           final_yield.yield_percentage);
    printf("  2. Economic viability: %s\n",
           final_yield.is_economical ? "YES" : "NO");
    printf("  3. Panel yield impact for 16-up: %.1f%%\n",
           compute_panel_yield(final_yield.predicted_yield, 16) * 100.0);
    printf("====================================================\n\n");

    return 0;
}
