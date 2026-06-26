/**
 * @file    example_panel_optimization.c
 * @brief   Example: Panel Utilization Optimization for Different Board Sizes
 *
 * Demonstrates panel utilization optimization across multiple board
 * sizes and panel formats. Shows how board aspect ratio, spacing,
 * and breakaway method affect utilization and cost.
 *
 * Tests four common panel sizes:
 *   - Standard: 457x610mm (18x24")
 *   - Small: 300x400mm
 *   - Large: 610x914mm (24x36")
 *   - Custom: 250x350mm
 */

#include <stdio.h>
#include <stdlib.h>
#include "dfm_panel.h"
#include "dfm_cost.h"

static void analyze_panel(double bw_mm, double bh_mm,
                           double pw_mm, double ph_mm,
                           const char *panel_name,
                           int quantity, ipc_class_t ipc_class)
{
    panel_optimization_result_t result = optimize_panel_utilization(
        bw_mm, bh_mm, pw_mm, ph_mm, 2.0, 10.0, 12.0,
        BREAKAWAY_TAB_ROUTE);

    double panel_cost = estimate_panel_cost(pw_mm, ph_mm, 4, ipc_class);
    double cost_per_board = panel_cost / (double)result.config.total_boards;

    printf("%-18s | %d x %d = %-3d | %6.1f%% | $%6.2f | $%6.2f | $%6.2f\n",
           panel_name,
           result.config.boards_x,
           result.config.boards_y,
           result.config.total_boards,
           result.utilization_pct,
           panel_cost,
           cost_per_board,
           panel_cost * (double)quantity);
}

int main(void)
{
    printf("\n");
    printf("=================================================================\n");
    printf("  PCB Panel Utilization Optimization\n");
    printf("=================================================================\n\n");

    /* Board configurations to test */
    printf("Board: 50x30mm (small sensor module)\n");
    printf("=================================================================\n");
    printf("%-18s | Layout       | Util%%   | Panel   | Per Brd | %d panels\n",
           "Panel Size", 1000);
    printf("-------------------|-------------|---------|---------|---------|----------\n");

    analyze_panel(50.0, 30.0, 457.0, 610.0, "Standard 457x610", 1000, IPC_CLASS_2);
    analyze_panel(50.0, 30.0, 300.0, 400.0, "Small 300x400", 1000, IPC_CLASS_2);
    analyze_panel(50.0, 30.0, 610.0, 914.0, "Large 610x914", 1000, IPC_CLASS_2);
    analyze_panel(50.0, 30.0, 250.0, 350.0, "Custom 250x350", 1000, IPC_CLASS_2);

    printf("\n\nBoard: 100x80mm (IoT gateway)\n");
    printf("=================================================================\n");
    printf("%-18s | Layout       | Util%%   | Panel   | Per Brd | %d panels\n",
           "Panel Size", 500);
    printf("-------------------|-------------|---------|---------|---------|----------\n");

    analyze_panel(100.0, 80.0, 457.0, 610.0, "Standard 457x610", 500, IPC_CLASS_2);
    analyze_panel(100.0, 80.0, 300.0, 400.0, "Small 300x400", 500, IPC_CLASS_2);
    analyze_panel(100.0, 80.0, 610.0, 914.0, "Large 610x914", 500, IPC_CLASS_2);
    analyze_panel(100.0, 80.0, 250.0, 350.0, "Custom 250x350", 500, IPC_CLASS_2);

    printf("\n\nBoard: 160x100mm (automotive ECU)\n");
    printf("=================================================================\n");
    printf("%-18s | Layout       | Util%%   | Panel   | Per Brd | %d panels\n",
           "Panel Size", 100);
    printf("-------------------|-------------|---------|---------|---------|----------\n");

    analyze_panel(160.0, 100.0, 457.0, 610.0, "Standard 457x610", 100, IPC_CLASS_3);
    analyze_panel(160.0, 100.0, 300.0, 400.0, "Small 300x400", 100, IPC_CLASS_3);
    analyze_panel(160.0, 100.0, 610.0, 914.0, "Large 610x914", 100, IPC_CLASS_3);

    printf("\n=================================================================\n");
    printf("  Key Insights:\n");
    printf("  1. Larger panels give better utilization for small boards\n");
    printf("  2. Board aspect ratio affects panel fit\n");
    printf("  3. Class 3 adds ~40-60%% cost premium vs Class 2\n");
    printf("  4. Rail width tradeoff: more rail = less usable area\n");
    printf("=================================================================\n\n");

    return 0;
}
