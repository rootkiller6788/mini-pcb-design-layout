/**
 * @file demo_stackup_view.c
 * @brief Demonstration: Build and visualize a complete rigid-flex stackup.
 *
 * Shows the sequence of API calls to construct, analyze, and validate
 * a 4-layer rigid-flex design. Outputs human-readable stackup summary.
 */

#include "flex_stackup.h"
#include "flex_bend.h"
#include "flex_design_rule.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║   Flex/Rigid-Flex Stackup Visualizer     ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    flex_stackup_t s = flex_stackup_init("Demo_4L_RigidFlex");
    strncpy(s.ipc_class, "2", sizeof(s.ipc_class) - 1);

    /* Build 4L rigid-flex: 2 flex-through + 2 rigid-only */
    flex_stackup_add_signal_layer(&s, "L1_RIGID", 35.0,
        FLEX_COPPER_ED, FLEX_DIELEC_POLYIMIDE, 100.0);
    flex_stackup_add_plane_layer(&s, "L2_GND", 18.0, FLEX_COPPER_RA);
    flex_stackup_add_signal_layer(&s, "L3_SIG", 18.0,
        FLEX_COPPER_RA, FLEX_DIELEC_POLYIMIDE, 50.0);
    flex_stackup_add_plane_layer(&s, "L4_PWR", 35.0, FLEX_COPPER_ED);

    flex_stackup_set_rigid_only(&s, 1);
    flex_stackup_set_flex_through(&s, 2);
    flex_stackup_set_flex_through(&s, 3);
    flex_stackup_set_rigid_only(&s, 4);

    flex_stackup_add_bend_zone(&s, 0, 0, 0, 25.0, 3.0, 90.0, 0);
    flex_stackup_add_stiffener(&s, FLEX_STIFFENER_FR4, 0.8, 1);

    /* Display stackup */
    char desc[1024];
    flex_stackup_describe(&s, desc, sizeof(desc));
    printf("%s\n", desc);

    /* Neutral axis and rigidity */
    double na;
    flex_stackup_neutral_axis(&s, &na);
    double d = flex_stackup_flexural_rigidity(&s);
    printf("Neutral axis: %.4f mm from bottom\n", na);
    printf("Flexural rigidity: %.2f N·mm²/mm\n", d);

    /* DRC */
    int v = flex_stackup_validate_ipc2223(&s);
    printf("IPC-2223 violations: %d\n", v);

    /* Cost */
    double cost = flex_stackup_cost_index(&s);
    printf("Relative cost: %.1f× baseline\n", cost);

    return 0;
}
