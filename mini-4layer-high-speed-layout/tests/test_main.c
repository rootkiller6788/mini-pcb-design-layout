#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "hs_stackup.h"
#include "hs_impedance.h"
#include "hs_transmission.h"
#include "hs_crosstalk.h"
#include "hs_pdn.h"
#include "hs_via.h"

#define ASSERT_NEAR(a,b,tol) assert(fabs((a)-(b))<(tol))
static int tr=0,tp=0;
#define RT(n) do{tr++;printf("  %s ... ",#n);n();tp++;printf("PASS\n");}while(0)

/* Stackup */
static void t_skin_depth(void){double d=hs_skin_depth(1e9,1.72e-8,1.0);assert(d>1.5e-6&&d<3.0e-6);d=hs_skin_depth(10e6,1.72e-8,1.0);assert(d>15e-6&&d<30e-6);assert(hs_skin_depth(0,1.72e-8,1.0)==0.0);}
static void t_er_static(void){double e=hs_effective_er_static(4.3,0.2e-3,0.3e-3,35e-6);assert(e>2.5&&e<4.5);}
static void t_plane_cap(void){double c=hs_plane_capacitance(0.01,0.25e-3,4.2);assert(c>1.0e-9&&c<2.5e-9);}
static void t_cu_weight(void){double t=hs_copper_weight_to_thickness(1.0);assert(t>30e-6&&t<40e-6);t=hs_copper_weight_to_thickness(0.5);assert(t>15e-6&&t<22e-6);}
static void t_stackup_init(void){hs_stackup_t s;hs_stackup_init_default(&s);assert(s.config==HS_CONFIG_SIG_GND_PWR_SIG);assert(s.total_thickness>0.0);}
static void t_stackup_val(void){hs_stackup_t s;hs_stackup_init_default(&s);assert(hs_stackup_validate(&s)==0);assert(hs_stackup_validate(NULL)!=0);}
static void t_material(void){const hs_material_props_t*m=hs_material_get_props(HS_MAT_FR4_STD);assert(m);assert(m->er>3.0);m=hs_material_get_props(HS_MAT_ROGERS_4350B);assert(m);assert(m->tan_d<0.01);}
static void t_roughness(void){double k=hs_roughness_correction(0.4e-6,10e-6);assert(k>=1.0);k=hs_roughness_correction(0.4e-6,0.5e-6);assert(k>=1.0);}
static void t_prop_v(void){double v=hs_propagation_velocity(4.0);ASSERT_NEAR(v,1.5e8,0.1e8);}
static void t_prop_delay(void){double t=hs_propagation_delay_per_meter(4.0);ASSERT_NEAR(t,6.67e-9,1.0e-9);}
static void t_trace_r(void){double r=hs_trace_resistance_per_meter(0.127e-3,35e-6,0,1.72e-8);assert(r>0);double ra=hs_trace_resistance_per_meter(0.127e-3,35e-6,1e9,1.72e-8);assert(ra>r);}

/* Impedance */
static void t_ms_ipc(void){hs_impedance_geometry_t g;memset(&g,0,sizeof(g));g.trace_width=0.3e-3;g.trace_thickness=35e-6;g.dielectric_height=0.2e-3;g.dielectric_constant=4.3;hs_impedance_result_t r;assert(hs_microstrip_impedance(&g,&r)==0);assert(r.z0_single>30&&r.z0_single<70);}
static void t_ms_hj(void){hs_impedance_geometry_t g;memset(&g,0,sizeof(g));g.trace_width=0.3e-3;g.trace_thickness=35e-6;g.dielectric_height=0.2e-3;g.dielectric_constant=4.3;hs_impedance_result_t r;assert(hs_microstrip_impedance_hj(&g,&r)==0);assert(r.z0_single>30&&r.z0_single<70);}
static void t_stripline(void){hs_impedance_geometry_t g;memset(&g,0,sizeof(g));g.trace_width=0.15e-3;g.trace_thickness=17e-6;g.dielectric_height=0.3e-3;g.dielectric_constant=4.3;hs_impedance_result_t r;assert(hs_stripline_impedance(&g,&r)==0);assert(r.z0_single>25);}
static void t_diff_ms(void){hs_impedance_geometry_t g;memset(&g,0,sizeof(g));g.trace_width=0.15e-3;g.trace_thickness=35e-6;g.dielectric_height=0.2e-3;g.dielectric_constant=4.3;g.spacing=0.2e-3;hs_impedance_result_t r;assert(hs_diff_microstrip_impedance(&g,&r)==0);assert(r.z0_diff>50);assert(r.z0_even>r.z0_odd);}
static void t_gamma(void){ASSERT_NEAR(hs_reflection_coefficient(50.0,50.0),0.0,1e-10);ASSERT_NEAR(hs_reflection_coefficient(0.0,50.0),-1.0,1e-10);}
static void t_vswr(void){ASSERT_NEAR(hs_vswr(0.0),1.0,1e-10);ASSERT_NEAR(hs_vswr(0.5),3.0,1e-10);assert(isinf(hs_vswr(1.0)));}
static void t_rl(void){ASSERT_NEAR(hs_return_loss_db(0.1),20.0,0.1);ASSERT_NEAR(hs_return_loss_db(0.316),10.0,0.2);}
static void t_ml(void){ASSERT_NEAR(hs_mismatch_loss_db(0.0),0.0,1e-10);double m=hs_mismatch_loss_db(0.316);assert(m>0.0&&m<1.0);}
static void t_coupling(void){ASSERT_NEAR(hs_coupling_coefficient(60.0,40.0),0.2,0.05);}
static void t_solve_w(void){hs_impedance_geometry_t g;memset(&g,0,sizeof(g));g.dielectric_height=0.2e-3;g.dielectric_constant=4.3;g.trace_thickness=35e-6;hs_impedance_result_t r;assert(hs_solve_trace_width(50.0,&g,&r)==0);assert(r.z0_single>45&&r.z0_single<55);}
static void t_sm_corr(void){hs_impedance_geometry_t g;memset(&g,0,sizeof(g));g.trace_width=0.3e-3;g.dielectric_height=0.2e-3;g.coating_thickness=25e-6;g.coating_er=3.8;double e=hs_solder_mask_er_correction(&g,3.2);assert(e>3.2);}

/* Transmission */
static void t_rlgc(void){hs_rlgc_params_t r=hs_microstrip_rlgc(50.0,3.4,0.02,0.3e-3,35e-6,1e9);assert(r.capacitance>0);assert(r.inductance>0);}
static void t_prop_const(void){hs_rlgc_params_t r=hs_microstrip_rlgc(50.0,3.4,0.02,0.3e-3,35e-6,1e9);hs_propagation_constant_t p=hs_propagation_constant(&r,1e9);assert(p.beta_rad>0);}
static void t_z0_rlgc(void){hs_rlgc_params_t r=hs_microstrip_rlgc(50.0,3.4,0.02,0.3e-3,35e-6,1e9);double z=hs_characteristic_impedance_rlgc(&r,1e9);assert(z>35&&z<75);}
static void t_abcd_id(void){hs_rlgc_params_t r=hs_microstrip_rlgc(50.0,3.4,0.02,0.3e-3,35e-6,1e9);hs_propagation_constant_t p=hs_propagation_constant(&r,1e9);hs_abcd_matrix_t m=hs_tline_abcd(0,50,&p);ASSERT_NEAR(m.a_real,1.0,1e-10);ASSERT_NEAR(m.d_real,1.0,1e-10);}
static void t_abcd_cascade(void){hs_rlgc_params_t r=hs_microstrip_rlgc(50.0,3.4,0.02,0.3e-3,35e-6,1e9);hs_propagation_constant_t p=hs_propagation_constant(&r,1e9);hs_abcd_matrix_t m1=hs_tline_abcd(0,50,&p),m2=hs_tline_abcd(0,50,&p);hs_abcd_matrix_t mc=hs_abcd_cascade(&m1,&m2);ASSERT_NEAR(mc.a_real,1.0,1e-10);}
static void t_s_recip(void){hs_rlgc_params_t r;memset(&r,0,sizeof(r));r.capacitance=sqrt(3.4)/(299792458.0*50.0);r.inductance=50.0*sqrt(3.4)/299792458.0;hs_propagation_constant_t p;memset(&p,0,sizeof(p));p.beta_rad=2*M_PI*1e9*sqrt(3.4)/299792458.0;hs_abcd_matrix_t a=hs_tline_abcd(0.0375,50,&p);hs_sparams_t s=hs_abcd_to_sparams(&a,50);double m21=sqrt(s.s21_real*s.s21_real+s.s21_imag*s.s21_imag);double m12=sqrt(s.s12_real*s.s12_real+s.s12_imag*s.s12_imag);ASSERT_NEAR(m21,m12,1e-10);}
static void t_il(void){hs_sparams_t s;memset(&s,0,sizeof(s));s.s21_real=1.0;ASSERT_NEAR(hs_insertion_loss_db(&s),0.0,1e-10);s.s21_real=0.707;ASSERT_NEAR(hs_insertion_loss_db(&s),3.0,0.1);}
static void t_rl_s(void){hs_sparams_t s;memset(&s,0,sizeof(s));s.s11_real=0.1;ASSERT_NEAR(hs_return_loss_db_sparam(&s),20.0,0.5);}
static void t_bw_rise(void){ASSERT_NEAR(hs_bandwidth_from_rise_time(100e-12,0),3.5e9,0.1e9);ASSERT_NEAR(hs_bandwidth_from_rise_time(100e-12,1),5.0e9,0.1e9);}
static void t_crit_len(void){double l=hs_critical_length(300e-12,6.16e-9);assert(l>0.01&&l<0.05);}
static void t_wavelength(void){ASSERT_NEAR(hs_wavelength(1e9,3.4),0.163,0.02);}
static void t_qw_freq(void){ASSERT_NEAR(hs_quarter_wave_frequency(0.017,3.4),2.39e9,0.5e9);}
static void t_step_resp(void){hs_rlgc_params_t r=hs_microstrip_rlgc(50.0,3.4,0.02,0.3e-3,35e-6,1e9);hs_waveform_sample_t w[100];hs_step_response(&r,0.1,50,100,10,w);assert(w[0].voltage>=0);assert(w[0].time_ps>=0);}
static void t_eye(void){hs_waveform_sample_t w[200];for(int i=0;i<200;i++){w[i].time_ps=i*5.0;w[i].voltage=(i%20<10)?0.8:0.2;}hs_eye_diagram_t e;assert(hs_eye_diagram(w,200,50,20,&e)==0);assert(e.eye_height_v>0);assert(e.is_open==1);}
static void t_tdr(void){hs_freq_point_t s11[50];for(int i=0;i<50;i++){s11[i].frequency_hz=(i+1)*0.2e9;s11[i].s.s11_real=0;s11[i].s.s11_imag=0;}hs_tdr_point_t p[100];hs_tdr_impedance_profile(s11,50,50,3.4,100,p);for(int i=0;i<100;i++)assert(p[i].impedance_ohm>40&&p[i].impedance_ohm<60);}

/* Crosstalk */
static void t_mut_c(void){hs_coupled_pair_t p;memset(&p,0,sizeof(p));p.trace_width_m=0.15e-3;p.edge_spacing_m=0.3e-3;p.height_to_plane_m=0.2e-3;p.er_eff=3.4;p.z0_ohm=50;double c=hs_mutual_capacitance(&p);assert(c>=0);}
static void t_mut_l(void){hs_coupled_pair_t p;memset(&p,0,sizeof(p));p.trace_width_m=0.15e-3;p.edge_spacing_m=0.3e-3;p.height_to_plane_m=0.2e-3;p.z0_ohm=50;p.er_eff=3.4;double lm=hs_mutual_inductance(&p);double ls=hs_self_inductance(50,3.4);assert(lm<ls);}
static void t_coupling_an(void){hs_coupled_pair_t p;memset(&p,0,sizeof(p));p.trace_width_m=0.15e-3;p.edge_spacing_m=0.3e-3;p.height_to_plane_m=0.2e-3;p.er_eff=3.4;p.z0_ohm=50;hs_coupling_params_t cp;hs_coupling_analyze(&p,&cp);assert(cp.backward_crosstalk_coeff>0);}
static void t_next(void){hs_coupled_pair_t p;memset(&p,0,sizeof(p));p.trace_width_m=0.15e-3;p.edge_spacing_m=0.3e-3;p.height_to_plane_m=0.2e-3;p.er_eff=3.4;p.z0_ohm=50;p.coupling_length_m=0.05;p.aggressor_rise_time_s=200e-12;p.aggressor_amplitude_v=1.8;hs_coupling_params_t cp;hs_coupling_analyze(&p,&cp);hs_crosstalk_result_t r;hs_next_calculate(&p,&cp,&r);assert(r.next_db<0);}
static void t_fext(void){hs_coupled_pair_t p;memset(&p,0,sizeof(p));p.trace_width_m=0.15e-3;p.edge_spacing_m=0.3e-3;p.height_to_plane_m=0.2e-3;p.er_eff=3.4;p.z0_ohm=50;p.coupling_length_m=0.05;p.aggressor_rise_time_s=200e-12;p.aggressor_amplitude_v=1.8;hs_coupling_params_t cp;hs_coupling_analyze(&p,&cp);hs_crosstalk_result_t r;hs_fext_calculate(&p,&cp,&r);ASSERT_NEAR(r.fext_pulse_width_ps,200,50);}
static void t_ps_xtalk(void){double n[]={-30,-32,-35,-28};double pc=hs_power_sum_crosstalk_db(n,4,1);double pr=hs_power_sum_crosstalk_db(n,4,0);assert(pc>pr);assert(pc<0);}
static void t_3w(void){double s=hs_three_w_rule_spacing(0.15e-3);ASSERT_NEAR(s,0.3e-3,1e-6);}
static void t_guard(void){double r=hs_guard_trace_reduction_db(0.15e-3,0.2e-3,0.3e-3);assert(r>0);}
static void t_diff_imm(void){double d=hs_diff_crosstalk_immunity(-25,30);assert(d<-25);}

/* PDN */
static void t_pdn_zt(void){ASSERT_NEAR(hs_pdn_target_impedance(1.0,0.05,2.0,1.0),0.025,0.001);ASSERT_NEAR(hs_pdn_target_impedance(1.0,0.05,2.0,2.0),0.0125,0.001);}
static void t_decap_srf(void){hs_decap_t d;hs_decap_init(&d,HS_CAP_CERAMIC_X7R,100e-9,"0402");double s=hs_decap_srf(&d);assert(s>10e6&&s<50e6);}
static void t_decap_z(void){hs_decap_t d;hs_decap_init(&d,HS_CAP_CERAMIC_X7R,100e-9,"0402");double s=hs_decap_srf(&d);double zs=hs_decap_impedance(&d,s);ASSERT_NEAR(zs,d.esr_ohm,d.esr_ohm*0.5);assert(hs_decap_impedance(&d,1e3)>zs);}
static void t_plane_cap2(void){hs_plane_pair_t pp;memset(&pp,0,sizeof(pp));pp.plane_width_m=0.1;pp.plane_height_m=0.1;pp.separation_m=0.25e-3;pp.dielectric_er=4.2;double c=hs_plane_pair_capacitance(&pp);assert(c>1e-9&&c<2e-9);}
static void t_plane_res(void){hs_plane_pair_t pp;memset(&pp,0,sizeof(pp));pp.plane_width_m=0.1;pp.plane_height_m=0.08;pp.dielectric_er=4.2;double f=hs_plane_resonance_frequency(&pp);assert(f>500e6&&f<1000e6);}
static void t_ir_drop(void){hs_plane_pair_t pp;memset(&pp,0,sizeof(pp));pp.plane_width_m=0.1;pp.plane_height_m=0.05;pp.copper_thickness_m=35e-6;assert(hs_plane_ir_drop(&pp,5,0)<0.1);}
static void t_plane_z(void){hs_plane_pair_t pp;memset(&pp,0,sizeof(pp));pp.plane_width_m=0.1;pp.plane_height_m=0.1;pp.separation_m=0.25e-3;pp.dielectric_er=4.2;pp.copper_thickness_m=35e-6;double z=hs_plane_impedance(&pp,100e6,0.005);assert(z>0&&isfinite(z));}
static void t_ssn(void){double v=hs_ssn_estimate(32,0.02,0.5e-9,1e-9);assert(v>0.5&&v<1.5);}
static void t_decap_init(void){hs_decap_t d;hs_decap_init(&d,HS_CAP_CERAMIC_X7R,1e-6,"0603");assert(d.type==HS_CAP_CERAMIC_X7R);assert(d.esr_ohm>0);assert(strcmp(d.package_code,"0603")==0);}
static void t_vrm_init(void){hs_vrm_t v;hs_vrm_init_typical(&v,1.0,3.0);assert(v.bandwidth_hz==100e3);hs_vrm_init_typical(&v,3.3,1.0);assert(v.bandwidth_hz==50e3);}
static void t_pdn_total_z(void){hs_pdn_network_t n;memset(&n,0,sizeof(n));hs_decap_t b[2],c[4];hs_vrm_init_typical(&n.vrm,1.0,3.0);n.plane.plane_width_m=0.1;n.plane.plane_height_m=0.1;n.plane.separation_m=0.25e-3;n.plane.dielectric_er=4.2;n.plane.copper_thickness_m=35e-6;hs_decap_init(&c[0],HS_CAP_CERAMIC_X7R,100e-9,"0402");hs_decap_init(&c[1],HS_CAP_CERAMIC_X7R,470e-9,"0603");hs_decap_init(&b[0],HS_CAP_TANTALUM,10e-6,"1206");n.ceramic_caps=c;n.num_ceramic=2;n.bulk_caps=b;n.num_bulk=1;double z=hs_pdn_total_impedance(&n,10e6,0.005);assert(z>0&&isfinite(z));}
static void t_pdn_swp(void){hs_pdn_network_t n;memset(&n,0,sizeof(n));hs_decap_t b[2],c[4];hs_vrm_init_typical(&n.vrm,1.0,3.0);n.plane.plane_width_m=0.1;n.plane.plane_height_m=0.1;n.plane.separation_m=0.25e-3;n.plane.dielectric_er=4.2;n.plane.copper_thickness_m=35e-6;hs_decap_init(&c[0],HS_CAP_CERAMIC_X7R,100e-9,"0402");n.ceramic_caps=c;n.num_ceramic=1;n.bulk_caps=b;n.num_bulk=0;hs_pdn_impedance_point_t p[50];hs_pdn_result_t r;hs_pdn_analyze(&n,1e3,1e9,50,p,&r);assert(r.plane_capacitance_nf>0);assert(r.first_plane_resonance_hz>0);}
static void t_decap_sel(void){hs_pdn_network_t n;memset(&n,0,sizeof(n));hs_decap_t b[4],c[8];hs_vrm_init_typical(&n.vrm,1.0,3.0);n.plane.plane_width_m=0.1;n.plane.plane_height_m=0.1;n.plane.separation_m=0.25e-3;n.plane.dielectric_er=4.2;n.plane.copper_thickness_m=35e-6;n.bulk_caps=b;n.ceramic_caps=c;int nb,nc;int rc=hs_decap_select(&n,4,8,0.025,1e9,&nb,&nc);assert(rc>=0);assert(nc>0);}

/* Via */
static void t_via_l(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.hole_diameter_m=0.25e-3;g.barrel_length_m=1.6e-3;g.plating_thickness_m=25e-6;double l=hs_via_inductance(&g);assert(l>0.1e-9&&l<5e-9);}
static void t_via_c(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.hole_diameter_m=0.25e-3;g.pad_diameter_m=0.6e-3;g.antipad_diameter_m=0.9e-3;double c=hs_via_pad_capacitance(&g,0);assert(c>0&&c<5e-12);}
static void t_via_rdc(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.hole_diameter_m=0.25e-3;g.barrel_length_m=1.6e-3;g.plating_thickness_m=25e-6;double r=hs_via_dc_resistance(&g);assert(r>0&&r<0.1);}
static void t_via_model(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.hole_diameter_m=0.25e-3;g.pad_diameter_m=0.6e-3;g.antipad_diameter_m=0.9e-3;g.barrel_length_m=1.6e-3;g.plating_thickness_m=25e-6;hs_via_model_t m=hs_via_build_model(&g,4.0);assert(m.inductance_ph>0);assert(m.impedance_ohm>0);}
static void t_stub_res(void){ASSERT_NEAR(hs_via_stub_resonance(5e-3,4.0),7.5e9,1e9);}
static void t_backdrill(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.barrel_length_m=1.6e-3;g.stub_length_m=1.5e-3;assert(hs_via_backdrill_depth(&g,10e9)>0);assert(hs_via_backdrill_depth(&g,1e9)==0);}
static void t_via_z(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.hole_diameter_m=0.25e-3;g.pad_diameter_m=0.6e-3;g.antipad_diameter_m=1e-3;g.barrel_length_m=1.6e-3;g.plating_thickness_m=25e-6;hs_via_model_t m=hs_via_build_model(&g,4.0);double z=hs_via_impedance(&m);assert(z>10&&z<100);}
static void t_via_bw(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.hole_diameter_m=0.25e-3;g.pad_diameter_m=0.6e-3;g.antipad_diameter_m=0.9e-3;g.barrel_length_m=1.6e-3;g.plating_thickness_m=25e-6;hs_via_model_t m=hs_via_build_model(&g,4.0);assert(hs_via_bandwidth(&m)>1e9);}
static void t_diff_via(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.hole_diameter_m=0.25e-3;g.pad_diameter_m=0.6e-3;g.antipad_diameter_m=0.9e-3;g.barrel_length_m=1.6e-3;g.plating_thickness_m=25e-6;g.pitch_m=1e-3;hs_diff_via_model_t d=hs_diff_via_model(&g,4.0);assert(d.diff_impedance_ohm>0);assert(d.coupling_coeff>0&&d.coupling_coeff<1);}
static void t_via_opt(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.hole_diameter_m=0.25e-3;g.pad_diameter_m=0.6e-3;g.antipad_diameter_m=0.9e-3;g.barrel_length_m=1.6e-3;g.plating_thickness_m=25e-6;int rc=hs_via_optimize_antipad(&g,50,4.0);assert(rc==0||rc==-1);}
static void t_stitch(void){double n=hs_stitching_via_count(0.1,7e9,3.4);assert(n>30&&n<60);}
static void t_via_feas(void){hs_via_geometry_t g;memset(&g,0,sizeof(g));g.hole_diameter_m=0.25e-3;g.pad_diameter_m=0.6e-3;g.antipad_diameter_m=0.9e-3;g.barrel_length_m=1.6e-3;g.plating_thickness_m=25e-6;hs_via_constraints_t c;memset(&c,0,sizeof(c));c.max_aspect_ratio=8;c.min_antipad_ratio=1.2;c.max_stub_length_m=1e-3;assert(hs_via_check_feasibility(&g,&c)==0);}

int main(void){
printf("=== mini-4layer-high-speed-layout Test Suite ===\n\n");
printf("[Stackup]\n");RT(t_skin_depth);RT(t_er_static);RT(t_plane_cap);RT(t_cu_weight);RT(t_stackup_init);RT(t_stackup_val);RT(t_material);RT(t_roughness);RT(t_prop_v);RT(t_prop_delay);RT(t_trace_r);
printf("\n[Impedance]\n");RT(t_ms_ipc);RT(t_ms_hj);RT(t_stripline);RT(t_diff_ms);RT(t_gamma);RT(t_vswr);RT(t_rl);RT(t_ml);RT(t_coupling);RT(t_solve_w);RT(t_sm_corr);
printf("\n[Transmission]\n");RT(t_rlgc);RT(t_prop_const);RT(t_z0_rlgc);RT(t_abcd_id);RT(t_abcd_cascade);RT(t_s_recip);RT(t_il);RT(t_rl_s);RT(t_bw_rise);RT(t_crit_len);RT(t_wavelength);RT(t_qw_freq);RT(t_step_resp);RT(t_eye);RT(t_tdr);
printf("\n[Crosstalk]\n");RT(t_mut_c);RT(t_mut_l);RT(t_coupling_an);RT(t_next);RT(t_fext);RT(t_ps_xtalk);RT(t_3w);RT(t_guard);RT(t_diff_imm);
printf("\n[PDN]\n");RT(t_pdn_zt);RT(t_decap_srf);RT(t_decap_z);RT(t_plane_cap2);RT(t_plane_res);RT(t_ir_drop);RT(t_plane_z);RT(t_ssn);RT(t_decap_init);RT(t_vrm_init);RT(t_pdn_total_z);RT(t_pdn_swp);RT(t_decap_sel);
printf("\n[Via]\n");RT(t_via_l);RT(t_via_c);RT(t_via_rdc);RT(t_via_model);RT(t_stub_res);RT(t_backdrill);RT(t_via_z);RT(t_via_bw);RT(t_diff_via);RT(t_via_opt);RT(t_stitch);RT(t_via_feas);
printf("\n=== All %d tests passed ===\n",tp);
return 0;}
