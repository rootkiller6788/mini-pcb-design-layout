# Knowledge Graph — mini-flex-rigid-flex-design

## L1: Definitions
| # | Term | Type | Location |
|---|------|------|----------|
| 1 | Flex dielectric type (PI, LCP, PET, PEN, PTFE, MPI, epoxy) | enum | flex_material.h |
| 2 | Copper foil type (RA, ED, RA-LP, ED-LP) | enum | flex_material.h |
| 3 | Adhesive system type (acrylic, epoxy, PI, PSA, adhesiveless) | enum | flex_material.h |
| 4 | Stiffener type (none, FR-4, PI, aluminum, stainless, copper) | enum | flex_material.h |
| 5 | Coverlay type (PI film, LPI, PIC) | enum | flex_material.h |
| 6 | Layer type (signal, plane, mixed, adhesive-only) | enum | flex_material.h |
| 7 | Section type (rigid, flex, transition) | enum | flex_material.h |
| 8 | Bend grain orientation (parallel, perpendicular, 45°) | enum | flex_bend.h |
| 9 | Bend configuration (single, U-shape, Z-shape, spiral, fold) | enum | flex_bend.h |
| 10 | Transmission line type (microstrip, stripline, differential, CPW) | enum | flex_signal_integrity.h |
| 11 | Dielectric electrical properties (εr, tanδ, DI strength) | struct | flex_material.h |
| 12 | Dielectric mechanical properties (E, CTE, Tg, k_thermal) | struct | flex_material.h |
| 13 | Copper foil properties (ρ, k, tensile, roughness, fatigue) | struct | flex_material.h |
| 14 | Adhesive properties (εr, E, CTE, Tg, peel, flow) | struct | flex_material.h |
| 15 | Complete material spec (all properties + grade + IPC sheet) | struct | flex_material.h |
| 16 | Layer definition (index, type, materials, thicknesses) | struct | flex_stackup.h |
| 17 | Bend zone geometry (coordinates, radius, angle, dynamic) | struct | flex_stackup.h |
| 18 | Transition zone geometry (location, length, anchor, tear-stop) | struct | flex_stackup.h |
| 19 | Complete stackup (layers, zones, stiffeners, metrics) | struct | flex_stackup.h |
| 20 | Bend parameters (radius, angle, materials, cycles, temp) | struct | flex_bend.h |
| 21 | Bend analysis result (safety, strain, life, failure mode) | struct | flex_bend.h |
| 22 | DRC violation (severity, standard, values, description) | struct | flex_design_rule.h |
| 23 | DRC report (violations aggregated with severity counts) | struct | flex_design_rule.h |
| 24 | TL parameters (geometry, material, frequency) | struct | flex_signal_integrity.h |
| 25 | TL analysis result (Z0, Zdiff, loss, delay, crosstalk) | struct | flex_signal_integrity.h |
| 26 | Transition parameters (type, geometry, materials, thermal) | struct | flex_rigid_transition.h |
| 27 | Transition result (stress, SCF, anchor, impedance, life) | struct | flex_rigid_transition.h |
| 28 | Thermal config (ambient, trace, board geometry, airflow) | struct | flex_thermal.h |
| 29 | Thermal result (ampacity, temp rise, θ_BA, junction Tj) | struct | flex_thermal.h |

## L2: Core Concepts
| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Symmetric vs asymmetric stackup | flex_stackup_verify_symmetry() |
| 2 | Bend zone isolation (rigid layers don't bend) | flex_stackup_set_rigid_only() |
| 3 | Neutral axis in composite bending | flex_stackup_neutral_axis() |
| 4 | IPC-2223 design rule hierarchy | flex_drc_run_full() |
| 5 | Microstrip vs stripline in flex | flex_tl_analyze() with TL type switch |
| 6 | Differential signaling on flex | flex_diff_microstrip_z0() |
| 7 | Skin effect and conductor loss | flex_conductor_loss_db_per_mm() |
| 8 | Dielectric relaxation (frequency-dependent εr) | flex_dk_at_frequency() |
| 9 | CTE mismatch stress | flex_cte_mismatch_stress() |
| 10 | Glass transition and moisture effects | flex_tg_moisture_shift() |
| 11 | Adhesive selection for thermal cycling | flex_adhesive_properties_standard() |
| 12 | Anchor tab mechanical anchoring | flex_anchor_tab_length() |
| 13 | Tear-stop crack arrest mechanism | flex_tear_stop_spacing() |
| 14 | Impedance continuity across transition | flex_transition_impedance_delta() |

## L3: Mathematical Structures
| # | Structure | Implementation |
|---|-----------|---------------|
| 1 | Beam bending strain distribution | ε(y) = (y - y_NA) / R in flex_strain_profile() |
| 2 | Composite flexural rigidity | D = Σ E_i·(b·t_i³/12 + b·t_i·d_i²) in flex_stackup_flexural_rigidity() |
| 3 | Multilayer thermal resistance series | R_th = Σ(t_i / k_i·A) in flex_thermal_conduction_1d() |
| 4 | Parallel/series conductivity rules | flex_thermal_conductivity_in_plane/through() |
| 5 | Wheeler transmission line equations | Two-regime formula in flex_microstrip_z0_wheeler() |
| 6 | Hammerstad-Jensen effective DK | εeff formula in flex_effective_dk_microstrip() |
| 7 | Cohn stripline impedance | Logarithmic formula in flex_stripline_z0() |
| 8 | Djordjevic-Sarkar DK(f) model | tanh-based relaxation in flex_dk_at_frequency() |
| 9 | Hammerstad roughness correction | Kr = 1 + (2/π)·atan(1.4·(Rrms/δ)²) |
| 10 | NEXT coupling approximation | K_NEXT ≈ 1/(1+(s/h)²) |
| 11 | Exponential impedance taper | Z0(x) = Z0_start·exp(x·ln(Z0_end/Z0_start)) |
| 12 | Thermal time constant (lumped) | τ = ρ·c_p·V / (h·A) |

## L4: Fundamental Laws
| # | Law | Implementation | Reference |
|---|-----|---------------|-----------|
| 1 | IPC-2223 Minimum Bend Radius (§5.2.4) | flex_min_bend_radius_ipc2223() | IPC-2223C |
| 2 | IPC-2223 Annular Ring (§9.1) | flex_drc_annular_ring() | IPC-2223 |
| 3 | IPC-2223 Via-in-Bend Prohibition (§5.2.6) | flex_drc_via_in_bend_zone() | IPC-2223 |
| 4 | IPC-2223 Transition Zone Rules (§8) | flex_drc_transition_zone() | IPC-2223 |
| 5 | Fourier's Law of Heat Conduction | flex_thermal_conduction_1d() | Fourier 1822 |
| 6 | Bernoulli-Euler Beam Bending | flex_min_bend_radius_beam_theory() | Timoshenko |
| 7 | Hooke's Law (σ = E·ε) in bending | flex_copper_strain_percent() | Hooke 1678 |
| 8 | Coffin-Manson Fatigue Law | flex_cycles_to_failure_coffin_manson() | Coffin 1954 |
| 9 | Suhir Bimaterial Interface Stress | flex_transition_shear_stress() | Suhir 1986 |
| 10 | Newton's Law of Cooling (convection) | flex_convection_coefficient() | Newton 1701 |
| 11 | LEFM — Griffith Crack Criterion | flex_tear_stop_spacing() | Griffith 1921 |
| 12 | Arrhenius Temperature Acceleration | flex_cycles_temperature_derate() | Arrhenius 1889 |
| 13 | Engelmaier-Wild Strain-Life Fatigue | flex_transition_thermal_life() | Engelmaier 1983 |
| 14 | IPC-2152 Trace Ampacity | flex_trace_ampacity_ipc2152() | IPC-2152 |
| 15 | Timoshenko Bi-Metal Warpage | flex_stackup_warpage_estimate() | Timoshenko 1925 |

## L5: Algorithms/Methods
| # | Algorithm | Implementation |
|---|-----------|---------------|
| 1 | IPC-2223 k-factor bend radius | flex_min_bend_radius_ipc2223() |
| 2 | Beam theory bend radius | flex_min_bend_radius_beam_theory() |
| 3 | Strain profile through thickness | flex_strain_profile() |
| 4 | Interfacial shear stress (Cu-dielectric) | flex_interfacial_shear_stress() |
| 5 | Coffin-Manson fatigue life | flex_cycles_to_failure_coffin_manson() |
| 6 | IPC-TM-650 flexural fatigue | flex_cycles_ipc_tm650() |
| 7 | Arrhenius temperature derating | flex_cycles_temperature_derate() |
| 8 | Springback angle prediction | flex_springback_angle() |
| 9 | Grain orientation optimization | flex_optimal_grain_orientation() |
| 10 | Combined bend analysis pipeline | flex_bend_analyze() |
| 11 | Wheeler microstrip Z0 | flex_microstrip_z0_wheeler() |
| 12 | Embedded microstrip (coverlay) Z0 | flex_microstrip_embedded_z0() |
| 13 | Symmetric stripline Z0 | flex_stripline_z0() |
| 14 | Asymmetric stripline Z0 | flex_stripline_asymmetric_z0() |
| 15 | Effective DK (Hammerstad-Jensen) | flex_effective_dk_microstrip() |
| 16 | Differential microstrip Z0 | flex_diff_microstrip_z0() |
| 17 | Differential stripline Z0 | flex_diff_stripline_z0() |
| 18 | Conductor loss (skin effect) | flex_conductor_loss_db_per_mm() |
| 19 | Dielectric loss | flex_dielectric_loss_db_per_mm() |
| 20 | Hammerstad roughness correction | flex_hammerstad_roughness_factor() |
| 21 | Skin depth calculation | flex_skin_depth_um() |
| 22 | NEXT estimation | flex_next_coefficient() |
| 23 | FEXT estimation | flex_fext_db() |
| 24 | Propagation delay | flex_propagation_delay_ps_per_mm() |
| 25 | Critical length (TL threshold) | flex_critical_length_mm() |
| 26 | Combined TL analysis | flex_tl_analyze() |
| 27 | Transition shear stress (Suhir) | flex_transition_shear_stress() |
| 28 | Transition peel stress | flex_transition_peel_stress() |
| 29 | Stress concentration (Peterson) | flex_transition_stress_concentration() |
| 30 | Anchor tab length calculation | flex_anchor_tab_length() |
| 31 | Anchor tab pull-out strength | flex_anchor_tab_strength() |
| 32 | Tear-stop spacing (LEFM) | flex_tear_stop_spacing() |
| 33 | Tear-stop sizing verification | flex_tear_stop_is_adequate() |
| 34 | Impedance discontinuity | flex_transition_impedance_delta() |
| 35 | Exponential impedance taper | flex_impedance_taper_exponential() |
| 36 | Thermal fatigue life (Engelmaier) | flex_transition_thermal_life() |
| 37 | Combined transition analysis | flex_transition_analyze() |
| 38 | Transition robustness rating | flex_transition_rating() |
| 39 | IPC-2152 trace ampacity | flex_trace_ampacity_ipc2152() |
| 40 | Trace temperature rise (inverse) | flex_trace_temp_rise() |
| 41 | Flex ampacity derating | flex_ampacity_derate_flex() |
| 42 | Convection coefficient (natural/forced) | flex_convection_coefficient() |
| 43 | Board thermal resistance | flex_thermal_resistance_board() |
| 44 | Thermal strain (CTE mismatch) | flex_thermal_strain() |
| 45 | Critical temp delta for delamination | flex_critical_temp_delta() |
| 46 | Thermal time constant | flex_thermal_time_constant() |
| 47 | Junction temperature estimation | flex_junction_temperature() |
| 48 | Combined thermal analysis | flex_thermal_analyze() |
| 49 | Stackup neutral axis | flex_stackup_neutral_axis() |
| 50 | Flexural rigidity | flex_stackup_flexural_rigidity() |
| 51 | Stackup symmetry verification | flex_stackup_verify_symmetry() |
| 52 | Asymmetry metric | flex_stackup_asymmetry_metric() |
| 53 | Warpage estimation (Timoshenko) | flex_stackup_warpage_estimate() |
| 54 | IPC-2223 stackup validation | flex_stackup_validate_ipc2223() |
| 55 | Manufacturing cost index | flex_stackup_cost_index() |
| 56 | DRC report aggregation | flex_drc_run_full() |

## L6: Canonical Problems
| # | Problem | Solution |
|---|---------|----------|
| 1 | Minimum bend radius for given stackup | flex_bend_analyze() + IPC-2223 check |
| 2 | Dynamic flex fatigue life prediction | Coffin-Manson + IPC-TM-650 + Arrhenius |
| 3 | 50Ω/90Ω/100Ω impedance design on flex | flex_tl_analyze() with Wheeler/Stripline |
| 4 | Loss budget for high-speed flex interconnect | conductor + dielectric + roughness loss |
| 5 | Rigid-flex transition stress analysis | Suhir shear + peel + SCF |
| 6 | Thermal derating for automotive flex | ampacity + convection + θ_BA + Tj |
| 7 | Stackup warpage from CTE asymmetry | Timoshenko bi-metal formula |
| 8 | Design rule violation detection and reporting | flex_drc_run_full() + report print |

## L7: Applications (Partial+)
| # | Application | Implementation |
|---|-------------|---------------|
| 1 | USB 3.2 / Thunderbolt flex interconnect | example_signal_integrity.c |
| 2 | Automotive ECU rigid-flex interconnect | example_rigid_flex_design.c |
| 3 | Foldable phone hinge flex | example_bend_analysis.c |

## L8: Advanced Topics (Partial+)
| # | Topic | Implementation |
|---|-------|---------------|
| 1 | Linear Elastic Fracture Mechanics for tear-stop | flex_tear_stop_spacing() |
| 2 | Djordjevic-Sarkar frequency-dependent DK | flex_dk_at_frequency() |
| 3 | Suhir bimaterial interfacial stress | flex_transition_shear_stress() |

## L9: Research Frontiers (Partial)
| # | Topic | Status |
|---|-------|--------|
| 1 | 6G sub-THz flexible interconnects (100+ GHz) | Documented — LCP DK/tanδ at 10 GHz |
| 2 | Additive manufacturing of flex circuits | Documented — inkjet/sputtered Cu properties |
| 3 | Stretchable electronics beyond bend radius limits | Documented — elongation limits in material DB |
| 4 | AI-driven flex stackup optimization | Future — cost_index() as objective function |
