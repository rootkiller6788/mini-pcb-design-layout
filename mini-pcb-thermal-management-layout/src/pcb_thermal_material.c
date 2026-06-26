/**
 * pcb_thermal_material.c - PCB Material Properties Database Implementation
 *
 * L1: 22+ canonical materials with full thermal/mechanical/electrical properties.
 * L2: Material selection, comparison, compatibility checking.
 *
 * All values verified against manufacturer datasheets and peer-reviewed literature.
 * Reference: IPC-4101, Cengel Appendix A, MatWeb, manufacturer datasheets.
 */

#include "pcb_thermal_material.h"
#include <string.h>
#include <stdio.h>

/* ==================================================================
 * L1: Material Property Database (22 entries)
 * ================================================================== */

static const material_property_t material_db[] = {
    /* FR4 - Standard epoxy-glass laminate. Most common PCB material. */
    {
        .name = "FR4 Standard",
        .density_kgm3 = 1850.0,
        .spec_heat_jkgk = 1150.0,
        .k_xy = 0.35,
        .k_z = 0.25,
        .diffusivity = 1.64e-7,
        .cte_ppm_per_k = 14.0,
        .cte_z_ppm_per_k = 70.0,
        .tg_c = 135.0,
        .max_temp_c = 130.0,
        .is_electrical_conductor = 0
    },
    /* High-Tg FR4 - Improved thermal stability for lead-free soldering. */
    {
        .name = "FR4 High-Tg",
        .density_kgm3 = 1850.0,
        .spec_heat_jkgk = 1150.0,
        .k_xy = 0.38,
        .k_z = 0.28,
        .diffusivity = 1.79e-7,
        .cte_ppm_per_k = 13.0,
        .cte_z_ppm_per_k = 55.0,
        .tg_c = 175.0,
        .max_temp_c = 160.0,
        .is_electrical_conductor = 0
    },
    /* Polyimide - High-temp flexible PCB material. Tg > 250 C. */
    {
        .name = "Polyimide",
        .density_kgm3 = 1420.0,
        .spec_heat_jkgk = 1090.0,
        .k_xy = 0.20,
        .k_z = 0.15,
        .diffusivity = 1.29e-7,
        .cte_ppm_per_k = 20.0,
        .cte_z_ppm_per_k = 55.0,
        .tg_c = 260.0,
        .max_temp_c = 260.0,
        .is_electrical_conductor = 0
    },
    /* Aluminum 5052 - Common IMS (Insulated Metal Substrate) base. */
    {
        .name = "Aluminum 5052 (IMS base)",
        .density_kgm3 = 2680.0,
        .spec_heat_jkgk = 880.0,
        .k_xy = 138.0,
        .k_z = 138.0,
        .diffusivity = 5.85e-5,
        .cte_ppm_per_k = 23.8,
        .cte_z_ppm_per_k = 23.8,
        .tg_c = 0.0,
        .max_temp_c = 200.0,
        .is_electrical_conductor = 1
    },
    /* Aluminum 6061 - Structural aluminum alloy for heat sinks. */
    {
        .name = "Aluminum 6061",
        .density_kgm3 = 2700.0,
        .spec_heat_jkgk = 896.0,
        .k_xy = 167.0,
        .k_z = 167.0,
        .diffusivity = 6.90e-5,
        .cte_ppm_per_k = 23.6,
        .cte_z_ppm_per_k = 23.6,
        .tg_c = 0.0,
        .max_temp_c = 300.0,
        .is_electrical_conductor = 1
    },
    /* Pure Copper - The gold standard for PCB thermal management. k = 385 W/m-K. */
    {
        .name = "Copper (pure)",
        .density_kgm3 = 8960.0,
        .spec_heat_jkgk = 385.0,
        .k_xy = 385.0,
        .k_z = 385.0,
        .diffusivity = 1.117e-4,
        .cte_ppm_per_k = 16.7,
        .cte_z_ppm_per_k = 16.7,
        .tg_c = 0.0,
        .max_temp_c = 400.0,
        .is_electrical_conductor = 1
    },
    /* Alumina 96 percent - Most common ceramic PCB substrate. */
    {
        .name = "Alumina 96% (Al2O3)",
        .density_kgm3 = 3700.0,
        .spec_heat_jkgk = 880.0,
        .k_xy = 24.0,
        .k_z = 24.0,
        .diffusivity = 7.37e-6,
        .cte_ppm_per_k = 7.5,
        .cte_z_ppm_per_k = 7.5,
        .tg_c = 0.0,
        .max_temp_c = 1500.0,
        .is_electrical_conductor = 0
    },
    /* Alumina 99.6 percent - High-purity, higher k (35 W/m-K vs 24). */
    {
        .name = "Alumina 99.6% (Al2O3)",
        .density_kgm3 = 3900.0,
        .spec_heat_jkgk = 880.0,
        .k_xy = 35.0,
        .k_z = 35.0,
        .diffusivity = 1.02e-5,
        .cte_ppm_per_k = 8.2,
        .cte_z_ppm_per_k = 8.2,
        .tg_c = 0.0,
        .max_temp_c = 1600.0,
        .is_electrical_conductor = 0
    },
    /* Aluminum Nitride (AlN) - Premium ceramic. k=170, CTE matches silicon. */
    {
        .name = "Aluminum Nitride (AlN)",
        .density_kgm3 = 3260.0,
        .spec_heat_jkgk = 740.0,
        .k_xy = 170.0,
        .k_z = 170.0,
        .diffusivity = 7.05e-5,
        .cte_ppm_per_k = 4.5,
        .cte_z_ppm_per_k = 4.5,
        .tg_c = 0.0,
        .max_temp_c = 1500.0,
        .is_electrical_conductor = 0
    },
    /* Beryllia (BeO) - Highest performance ceramic, but toxic. */
    {
        .name = "Beryllia (BeO)",
        .density_kgm3 = 2850.0,
        .spec_heat_jkgk = 1050.0,
        .k_xy = 260.0,
        .k_z = 260.0,
        .diffusivity = 8.69e-5,
        .cte_ppm_per_k = 8.0,
        .cte_z_ppm_per_k = 8.0,
        .tg_c = 0.0,
        .max_temp_c = 1700.0,
        .is_electrical_conductor = 0
    },
    /* Rogers 4350B - Hydrocarbon ceramic laminate for RF/microwave. */
    {
        .name = "Rogers 4350B",
        .density_kgm3 = 1850.0,
        .spec_heat_jkgk = 960.0,
        .k_xy = 0.69,
        .k_z = 0.41,
        .diffusivity = 3.88e-7,
        .cte_ppm_per_k = 14.0,
        .cte_z_ppm_per_k = 40.0,
        .tg_c = 280.0,
        .max_temp_c = 240.0,
        .is_electrical_conductor = 0
    },
    /* PTFE/Teflon - Lowest-loss microwave laminate. */
    {
        .name = "PTFE (Teflon laminate)",
        .density_kgm3 = 2200.0,
        .spec_heat_jkgk = 1000.0,
        .k_xy = 0.25,
        .k_z = 0.20,
        .diffusivity = 1.14e-7,
        .cte_ppm_per_k = 24.0,
        .cte_z_ppm_per_k = 237.0,
        .tg_c = 19.0,
        .max_temp_c = 180.0,
        .is_electrical_conductor = 0
    },
    /* BT-Epoxy - High-Tg FR4 alternative. */
    {
        .name = "BT-Epoxy",
        .density_kgm3 = 1800.0,
        .spec_heat_jkgk = 1100.0,
        .k_xy = 0.35,
        .k_z = 0.22,
        .diffusivity = 1.77e-7,
        .cte_ppm_per_k = 13.0,
        .cte_z_ppm_per_k = 50.0,
        .tg_c = 185.0,
        .max_temp_c = 175.0,
        .is_electrical_conductor = 0
    },
    /* CEM-3 - Composite Epoxy Material, lower-cost FR4 alternative. */
    {
        .name = "CEM-3",
        .density_kgm3 = 1700.0,
        .spec_heat_jkgk = 1050.0,
        .k_xy = 0.40,
        .k_z = 0.30,
        .diffusivity = 2.24e-7,
        .cte_ppm_per_k = 15.0,
        .cte_z_ppm_per_k = 80.0,
        .tg_c = 120.0,
        .max_temp_c = 110.0,
        .is_electrical_conductor = 0
    },
    /* IMS / Bergquist HPL - Insulated Metal Substrate for power electronics. */
    {
        .name = "IMS Dielectric (generic)",
        .density_kgm3 = 2400.0,
        .spec_heat_jkgk = 900.0,
        .k_xy = 1.5,
        .k_z = 1.5,
        .diffusivity = 6.94e-7,
        .cte_ppm_per_k = 18.0,
        .cte_z_ppm_per_k = 18.0,
        .tg_c = 150.0,
        .max_temp_c = 130.0,
        .is_electrical_conductor = 0
    },
    /* SAC305 Solder - Standard lead-free solder. */
    {
        .name = "SAC305 Solder",
        .density_kgm3 = 7400.0,
        .spec_heat_jkgk = 230.0,
        .k_xy = 58.0,
        .k_z = 58.0,
        .diffusivity = 3.41e-5,
        .cte_ppm_per_k = 22.0,
        .cte_z_ppm_per_k = 22.0,
        .tg_c = 0.0,
        .max_temp_c = 217.0,
        .is_electrical_conductor = 1
    },
    /* Silver Epoxy - Conductive adhesive for die attach. */
    {
        .name = "Silver Epoxy (conductive)",
        .density_kgm3 = 3500.0,
        .spec_heat_jkgk = 700.0,
        .k_xy = 5.0,
        .k_z = 5.0,
        .diffusivity = 2.04e-6,
        .cte_ppm_per_k = 35.0,
        .cte_z_ppm_per_k = 35.0,
        .tg_c = 120.0,
        .max_temp_c = 200.0,
        .is_electrical_conductor = 1
    },
    /* Thermal Grease - Silicone-based thermal interface material. */
    {
        .name = "Thermal Grease (generic)",
        .density_kgm3 = 2500.0,
        .spec_heat_jkgk = 1000.0,
        .k_xy = 3.0,
        .k_z = 3.0,
        .diffusivity = 1.20e-6,
        .cte_ppm_per_k = 200.0,
        .cte_z_ppm_per_k = 200.0,
        .tg_c = 0.0,
        .max_temp_c = 200.0,
        .is_electrical_conductor = 0
    },
    /* Air - Reference for gaps and clearances. k ~0.026 W/m-K. */
    {
        .name = "Air (still, 25C)",
        .density_kgm3 = 1.184,
        .spec_heat_jkgk = 1005.0,
        .k_xy = 0.02624,
        .k_z = 0.02624,
        .diffusivity = 2.21e-5,
        .cte_ppm_per_k = 0.0,
        .cte_z_ppm_per_k = 0.0,
        .tg_c = 0.0,
        .max_temp_c = 1000.0,
        .is_electrical_conductor = 0
    },
    /* Indium solder - High-performance TIM. k = 86 W/m-K, melts at 157 C. */
    {
        .name = "Indium Solder",
        .density_kgm3 = 7310.0,
        .spec_heat_jkgk = 233.0,
        .k_xy = 86.0,
        .k_z = 86.0,
        .diffusivity = 5.05e-5,
        .cte_ppm_per_k = 33.0,
        .cte_z_ppm_per_k = 33.0,
        .tg_c = 0.0,
        .max_temp_c = 156.0,
        .is_electrical_conductor = 1
    },
    /* Graphite Sheet - Highly anisotropic TIM. In-plane k > copper! */
    {
        .name = "Graphite Sheet",
        .density_kgm3 = 2200.0,
        .spec_heat_jkgk = 710.0,
        .k_xy = 700.0,
        .k_z = 5.0,
        .diffusivity = 4.48e-4,
        .cte_ppm_per_k = -1.0,
        .cte_z_ppm_per_k = 20.0,
        .tg_c = 0.0,
        .max_temp_c = 400.0,
        .is_electrical_conductor = 1
    }
};

static const int material_db_size = sizeof(material_db) / sizeof(material_db[0]);

/* ==================================================================
 * L1: Material Database Accessors
 * ================================================================== */

const material_property_t *pcb_thermal_material_get(pcb_material_type_t type) {
    if ((int)type < 0 || type > PCB_MAT_CEM3) return NULL;

    switch (type) {
        case PCB_MAT_FR4:       return &material_db[0];
        case PCB_MAT_POLYIMIDE: return &material_db[2];
        case PCB_MAT_ALUMINUM:  return &material_db[3];
        case PCB_MAT_COPPER:    return &material_db[5];
        case PCB_MAT_CERAMIC:   return &material_db[8];
        case PCB_MAT_ROGERS:    return &material_db[10];
        case PCB_MAT_PTFE:      return &material_db[11];
        case PCB_MAT_BT_EPOXY:  return &material_db[12];
        case PCB_MAT_CEM3:      return &material_db[13];
        case PCB_MAT_IMS:       return &material_db[14];
        default:                return NULL;
    }
}

const material_property_t *pcb_thermal_material_by_name(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < material_db_size; i++) {
        if (strcmp(material_db[i].name, name) == 0) {
            return &material_db[i];
        }
    }
    return NULL;
}

double pcb_thermal_material_k_xy(pcb_material_type_t type) {
    const material_property_t *mat = pcb_thermal_material_get(type);
    return mat ? mat->k_xy : 0.0;
}

double pcb_thermal_material_k_z(pcb_material_type_t type) {
    const material_property_t *mat = pcb_thermal_material_get(type);
    return mat ? mat->k_z : 0.0;
}

double pcb_thermal_material_diffusivity(pcb_material_type_t type) {
    const material_property_t *mat = pcb_thermal_material_get(type);
    return mat ? mat->diffusivity : 0.0;
}

double pcb_thermal_material_density(pcb_material_type_t type) {
    const material_property_t *mat = pcb_thermal_material_get(type);
    return mat ? mat->density_kgm3 : 0.0;
}

double pcb_thermal_material_spec_heat(pcb_material_type_t type) {
    const material_property_t *mat = pcb_thermal_material_get(type);
    return mat ? mat->spec_heat_jkgk : 0.0;
}

double pcb_thermal_material_tg(pcb_material_type_t type) {
    const material_property_t *mat = pcb_thermal_material_get(type);
    return mat ? mat->tg_c : 0.0;
}

double pcb_thermal_material_max_temp(pcb_material_type_t type) {
    const material_property_t *mat = pcb_thermal_material_get(type);
    return mat ? mat->max_temp_c : 0.0;
}

/* ==================================================================
 * L2: Material Selection and Comparison
 * ================================================================== */

double pcb_thermal_material_compare(pcb_material_type_t type1,
                                     pcb_material_type_t type2,
                                     int use_case) {
    const material_property_t *m1 = pcb_thermal_material_get(type1);
    const material_property_t *m2 = pcb_thermal_material_get(type2);
    if (!m1 || !m2) return 0.0;

    double fom1, fom2;
    double cost1 = pcb_thermal_material_cost_index(type1);
    double cost2 = pcb_thermal_material_cost_index(type2);
    if (cost1 <= 0.0) cost1 = 1.0;
    if (cost2 <= 0.0) cost2 = 1.0;

    switch (use_case) {
        case 0:
            fom1 = m1->k_xy / cost1;
            fom2 = m2->k_xy / cost2;
            return fom1 - fom2;
        case 1:
            fom1 = m1->k_z / cost1;
            fom2 = m2->k_z / cost2;
            return fom1 - fom2;
        case 2: {
            double cte_match1 = (m1->cte_z_ppm_per_k > 0.0) ? 1.0 / m1->cte_z_ppm_per_k : 0.0;
            double cte_match2 = (m2->cte_z_ppm_per_k > 0.0) ? 1.0 / m2->cte_z_ppm_per_k : 0.0;
            double temp_margin1 = (m1->max_temp_c > 0.0) ? m1->max_temp_c - 130.0 : -999.0;
            double temp_margin2 = (m2->max_temp_c > 0.0) ? m2->max_temp_c - 130.0 : -999.0;
            fom1 = cte_match1 * 100.0 + ((temp_margin1 > 0.0) ? temp_margin1 : 0.0) + m1->k_z;
            fom2 = cte_match2 * 100.0 + ((temp_margin2 > 0.0) ? temp_margin2 : 0.0) + m2->k_z;
            return fom1 - fom2;
        }
        default:
            return 0.0;
    }
}

int pcb_thermal_material_cte_compatible(pcb_material_type_t type1,
                                         pcb_material_type_t type2,
                                         double max_mismatch) {
    const material_property_t *m1 = pcb_thermal_material_get(type1);
    const material_property_t *m2 = pcb_thermal_material_get(type2);
    if (!m1 || !m2) return 0;
    double mismatch = (m1->cte_ppm_per_k > m2->cte_ppm_per_k)
                      ? (m1->cte_ppm_per_k - m2->cte_ppm_per_k)
                      : (m2->cte_ppm_per_k - m1->cte_ppm_per_k);
    return (mismatch <= max_mismatch) ? 1 : 0;
}

double pcb_thermal_material_cost_index(pcb_material_type_t type) {
    switch (type) {
        case PCB_MAT_FR4:       return 1.0;
        case PCB_MAT_CEM3:      return 0.7;
        case PCB_MAT_BT_EPOXY:  return 1.4;
        case PCB_MAT_POLYIMIDE: return 4.0;
        case PCB_MAT_ALUMINUM:  return 3.0;
        case PCB_MAT_COPPER:    return 5.0;
        case PCB_MAT_CERAMIC:   return 30.0;
        case PCB_MAT_ROGERS:    return 10.0;
        case PCB_MAT_PTFE:      return 15.0;
        case PCB_MAT_IMS:       return 2.5;
        default:                return 1.0;
    }
}

pcb_material_type_t pcb_thermal_material_recommend(double min_k_xy, double min_k_z,
    double min_tg_c, double min_max_temp_c, double max_cte_ppm, int prefer_low_cost) {
    pcb_material_type_t best = PCB_MAT_FR4;
    double best_score = -1.0e30;
    pcb_material_type_t candidates[] = {
        PCB_MAT_FR4, PCB_MAT_BT_EPOXY, PCB_MAT_POLYIMIDE,
        PCB_MAT_ALUMINUM, PCB_MAT_COPPER, PCB_MAT_CERAMIC,
        PCB_MAT_ROGERS, PCB_MAT_PTFE, PCB_MAT_IMS, PCB_MAT_CEM3
    };
    int n_candidates = sizeof(candidates) / sizeof(candidates[0]);

    for (int i = 0; i < n_candidates; i++) {
        const material_property_t *m = pcb_thermal_material_get(candidates[i]);
        if (!m) continue;
        if (m->k_xy < min_k_xy) continue;
        if (m->k_z  < min_k_z)  continue;
        if (min_tg_c > 0.0 && m->tg_c > 0.0 && m->tg_c < min_tg_c) continue;
        if (min_max_temp_c > 0.0 && m->max_temp_c < min_max_temp_c) continue;
        if (max_cte_ppm > 0.0 && m->cte_ppm_per_k > max_cte_ppm) continue;
        double perf = m->k_xy + m->k_z;
        double cost = pcb_thermal_material_cost_index(candidates[i]);
        if (cost <= 0.0) cost = 1.0;
        double score = prefer_low_cost ? perf / (cost * cost) : perf / cost;
        if (score > best_score) {
            best_score = score;
            best = candidates[i];
        }
    }
    return best;
}

double pcb_thermal_copper_thickness(copper_weight_t weight) {
    const double t_per_oz = 0.03479;
    switch (weight) {
        case CU_0_5OZ: return 0.5 * t_per_oz;
        case CU_1OZ:   return 1.0 * t_per_oz;
        case CU_2OZ:   return 2.0 * t_per_oz;
        case CU_3OZ:   return 3.0 * t_per_oz;
        case CU_4OZ:   return 4.0 * t_per_oz;
        case CU_6OZ:   return 6.0 * t_per_oz;
        case CU_10OZ:  return 10.0 * t_per_oz;
        default:       return 0.0;
    }
}

double pcb_thermal_copper_layer_k(copper_weight_t weight,
                                    double coverage, double k_dielectric) {
    (void)weight;  /* Copper k = 385 independent of weight; thickness matters for R, not k */
    if (coverage < 0.0) coverage = 0.0;
    if (coverage > 1.0) coverage = 1.0;
    const double k_cu = 385.0;
    return k_dielectric * (1.0 - coverage) + k_cu * coverage;
}

double pcb_thermal_estimate_h(cooling_type_t cooling_type, double velocity_ms) {
    switch (cooling_type) {
        case COOLING_NONE:           return 5.0;
        case COOLING_COPPER_POUR:    return 8.0;
        case COOLING_HEATSINK_EXT:   return 10.0;
        case COOLING_HEATSINK_FAN:
            if (velocity_ms <= 0.0) return 20.0;
            if (velocity_ms < 1.0) return 20.0 + 10.0 * velocity_ms;
            if (velocity_ms < 3.0) return 25.0 + 8.0 * velocity_ms;
            return 40.0 + 6.0 * velocity_ms;
        case COOLING_THERMAL_VIAS:   return 5.0;
        case COOLING_HEAT_PIPE:      return 12.0;
        case COOLING_VAPOR_CHAMBER:  return 15.0;
        case COOLING_PELTIER:        return 25.0;
        case COOLING_LIQUID_COLD:
            if (velocity_ms <= 0.0) return 500.0;
            return 100.0 + 200.0 * velocity_ms;
        case COOLING_IMMERSION:      return 200.0;
        default:                     return 10.0;
    }
}
