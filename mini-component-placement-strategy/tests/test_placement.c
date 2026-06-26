/**
 * @file test_placement.c
 * @brief Comprehensive test suite for mini-component-placement-strategy
 */
#include "placement_core.h"
#include "placement_constraint.h"
#include "placement_optimizer.h"
#include "placement_strategy.h"
#include "placement_thermal.h"
#include "placement_util.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#define ASSERT_NEAR(a,b,tol) assert(fabs((a)-(b))<(tol))
#define RUN_TEST(n) do{printf("  %-48s",#n);fflush(stdout);test_##n();printf("PASS\n");}while(0)

static PlacementResult* mkpr(uint32_t nc, uint32_t nn) {
    Board b; placement_board_init(&b,"TB",100.0,80.0,4);
    PlacementResult* r=calloc(1,sizeof(PlacementResult));
    assert(r&&placement_result_init(r,&b,nc,nn)); return r;
}
static void fc(Component*c,uint32_t id,const char*d,ComponentCategory cat,
               PackageType pk,double w,double h,double p){
    placement_component_init(c,d,cat,pk);c->comp_id=id;
    c->body.width=w;c->body.height=h;c->power_dissipation_W=p;
    c->max_junction_temp_C=125.0;c->theta_JA_C_per_W=50.0;c->theta_JC_C_per_W=10.0;
}

/* ===== L1: Definitions ===== */
static void test_comp_init(void){
    Component c; placement_component_init(&c,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603);
    assert(!strcmp(c.designator,"R1")); assert(c.category==COMP_CAT_PASSIVE);
    assert(!c.is_placed&&!c.is_fixed);
    placement_component_init(NULL,"X",0,0);
    placement_component_init(&c,NULL,COMP_CAT_ACTIVE,PKG_SOT_23);
    assert(c.designator[0]=='\0');
}
static void test_comp_add_pad(void){
    Component c; placement_component_init(&c,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64);
    assert(placement_component_add_pad(&c,1,"VCC",-5.0,5.0,0.5,0.3));
    assert(c.pad_count==1); assert(c.pads[0].pin_number==1);
    assert(!strcmp(c.pads[0].pin_name,"VCC"));
    for(uint32_t i=2;i<=64;i++)
        assert(placement_component_add_pad(&c,i,"GP",0,0,0.3,0.3));
    assert(c.pad_count==64);
    assert(!placement_component_add_pad(NULL,1,"X",0,0,0.1,0.1));
}
static void test_comp_set_pos(void){
    Component c; placement_component_init(&c,"C1",COMP_CAT_PASSIVE,PKG_SMD_0805);
    placement_component_set_position(&c,25.0,30.0,45.0);
    assert(c.rotation==90.0&&c.is_placed); /* round(0.5)=1, 45->90 */
    placement_component_set_position(&c,0,0,95.0); assert(c.rotation==90.0);
    placement_component_set_position(&c,0,0,-95.0); assert(c.rotation==270.0);
    placement_component_set_position(&c,0,0,370.0); assert(c.rotation==0.0);
}
static void test_comp_bounds(void){
    Component c; placement_component_init(&c,"R2",COMP_CAT_PASSIVE,PKG_SMD_0603);
    c.body.width=10.0;c.body.height=6.0;
    placement_component_set_position(&c,50.0,40.0,0.0);
    BoundingBox bb=placement_component_get_bounds(&c);
    ASSERT_NEAR(bb.x_min,45.0,0.01);ASSERT_NEAR(bb.x_max,55.0,0.01);
    placement_component_set_position(&c,50.0,40.0,90.0);
    bb=placement_component_get_bounds(&c);
    ASSERT_NEAR(bb.x_min,47.0,0.01);
    bb=placement_component_get_bounds(NULL);assert(bb.x_min==0);
}
static void test_board_init(void){
    Board b; placement_board_init(&b,"Main",160.0,100.0,4);
    assert(!strcmp(b.board_name,"Main")&&b.outline.width==160.0);
    assert(b.layer_count==4);assert(b.thickness_mm==1.6);
    Board b2; placement_board_init(&b2,"Big",100,100,20);
    assert(b2.layer_count==16); placement_board_init(NULL,NULL,0,0,0);
}
static void test_board_layer(void){
    Board b; placement_board_init(&b,"S",100,100,4);
    assert(placement_board_add_layer(&b,0,"Top",1,0,1.0,0.035));
    assert(!strcmp(b.layers[0].layer_name,"Top"));
    assert(!placement_board_add_layer(&b,4,"X",1,0,0.5,0.018));
}
static void test_net_init(void){
    Net n; placement_net_init(&n,42,"SPI_CLK");
    assert(n.net_id==42&&!strcmp(n.net_name,"SPI_CLK"));
    placement_net_init(NULL,0,NULL);
}
static void test_result_init(void){
    Board b; placement_board_init(&b,"T",100,80,2);
    PlacementResult r; assert(placement_result_init(&r,&b,50,100));
    assert(r.components&&r.nets); placement_result_free(&r);
    assert(!placement_result_init(&r,NULL,10,10));
}
static void test_centroid(void){
    PlacementResult* r=mkpr(3,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,0.5);
    fc(&r->components[1],2,"U2",COMP_CAT_ANALOG_IC,PKG_SOIC_8,5,4,0.1);
    fc(&r->components[2],3,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
    r->component_count=3;
    placement_component_set_position(&r->components[0],25,25,0);
    placement_component_set_position(&r->components[1],75,25,0);
    placement_component_set_position(&r->components[2],50,70,0);
    Point2D c=placement_compute_centroid(r);
    ASSERT_NEAR(c.x,50.0,0.01);ASSERT_NEAR(c.y,40.0,0.01);
    placement_result_free(r);free(r);
}

/* ===== L2: Wire Length & Legality ===== */
static void test_wire_length(void){
    PlacementResult* r=mkpr(4,2);
    for(int i=0;i<4;i++){char d[8];snprintf(d,8,"U%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_DIGITAL_IC,PKG_QFP_32,8,8,0.2);}
    r->component_count=4;
    placement_net_init(&r->nets[0],1,"A");r->nets[0].pin_count=2;
    placement_net_init(&r->nets[1],2,"B");r->nets[1].pin_count=2;r->net_count=2;
    for(int i=0;i<4;i++) placement_component_add_pad(&r->components[i],1,"P",0,0,0.3,0.3);
    r->components[0].net_ids[0]=1;r->components[1].net_ids[0]=1;
    r->components[2].net_ids[0]=2;r->components[3].net_ids[0]=2;
    placement_component_set_position(&r->components[0],20,40,0);
    placement_component_set_position(&r->components[1],60,40,0);
    placement_component_set_position(&r->components[2],20,70,0);
    placement_component_set_position(&r->components[3],60,70,0);
    double wl=placement_estimate_wire_length(r); assert(wl>0);
    placement_result_free(r);free(r);
}
static void test_pos_legal(void){
    PlacementResult* r=mkpr(2,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,0.5);
    fc(&r->components[1],2,"U2",COMP_CAT_ANALOG_IC,PKG_SOIC_8,5,4,0.1);
    r->component_count=2;
    placement_component_set_position(&r->components[0],50,40,0);
    assert(placement_is_position_legal(r,&r->components[1],20,20));
    assert(!placement_is_position_legal(r,&r->components[1],52,42));
    assert(!placement_is_position_legal(r,&r->components[1],-10,40));
    placement_result_free(r);free(r);
}

/* ===== L3: Cost Functions ===== */
static void test_cost_hpwl(void){
    PlacementResult* r=mkpr(3,2);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_32,8,8,0.2);
    fc(&r->components[1],2,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
    fc(&r->components[2],3,"R2",COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
    r->component_count=3;
    placement_net_init(&r->nets[0],1,"A");r->nets[0].pin_count=2;
    placement_net_init(&r->nets[1],2,"B");r->nets[1].pin_count=2;r->net_count=2;
    placement_component_add_pad(&r->components[0],1,"PA",-3,3,0.3,0.3);
    placement_component_add_pad(&r->components[0],2,"PB",3,-3,0.3,0.3);
    placement_component_add_pad(&r->components[1],1,"P1",0,0,0.3,0.3);
    placement_component_add_pad(&r->components[2],1,"P2",0,0,0.3,0.3);
    r->components[0].net_ids[0]=1;r->components[1].net_ids[0]=1;
    r->components[0].net_ids[1]=2;r->components[2].net_ids[0]=2;
    placement_component_set_position(&r->components[0],50,40,0);
    placement_component_set_position(&r->components[1],20,40,0);
    placement_component_set_position(&r->components[2],80,40,0);
    assert(placement_cost_hpwl(r)>0); assert(placement_cost_hpwl(NULL)==0);
    placement_result_free(r);free(r);
}
static void test_cost_steiner(void){
    PlacementResult* r=mkpr(4,1);
    for(int i=0;i<4;i++){char d[8];snprintf(d,8,"S%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
        placement_component_add_pad(&r->components[i],1,"P",0,0,0.3,0.3);
        r->components[i].net_ids[0]=1;}
    r->component_count=4;
    placement_net_init(&r->nets[0],1,"N1");r->nets[0].pin_count=4;r->net_count=1;
    placement_component_set_position(&r->components[0],10,10,0);
    placement_component_set_position(&r->components[1],90,10,0);
    placement_component_set_position(&r->components[2],10,70,0);
    placement_component_set_position(&r->components[3],50,50,0);
    double rsmt=placement_cost_steiner(r); assert(rsmt>0);
    placement_result_free(r);free(r);
}
static void test_cost_thermal_v(void){
    PlacementResult* r=mkpr(3,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,2.0);
    fc(&r->components[1],2,"U2",COMP_CAT_POWER,PKG_TO_220,10,15,5.0);
    fc(&r->components[2],3,"R1",COMP_CAT_PASSIVE,PKG_SMD_0805,2,1.2,0.25);
    r->component_count=3;
    placement_component_set_position(&r->components[0],30,40,0);
    placement_component_set_position(&r->components[1],70,40,0);
    placement_component_set_position(&r->components[2],50,20,0);
    assert(placement_cost_thermal(r,25.0)>0);
    placement_result_free(r);free(r);
}
static void test_cost_thermal_grad(void){
    PlacementResult* r=mkpr(3,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,1.0);
    fc(&r->components[1],2,"U2",COMP_CAT_POWER,PKG_TO_220,10,15,4.0);
    fc(&r->components[2],3,"U3",COMP_CAT_POWER,PKG_TO_220,10,15,2.0);
    r->component_count=3;
    placement_component_set_position(&r->components[0],30,40,0);
    placement_component_set_position(&r->components[1],50,40,0);
    placement_component_set_position(&r->components[2],70,40,0);
    double tg=placement_cost_thermal_gradient(r,25.0);
    assert(tg>=0);
    placement_result_free(r);free(r);
}
static void test_cost_overlap(void){
    PlacementResult* r=mkpr(3,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,0.5);
    fc(&r->components[1],2,"U2",COMP_CAT_ANALOG_IC,PKG_SOIC_8,5,4,0.1);
    fc(&r->components[2],3,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
    r->component_count=3;
    placement_component_set_position(&r->components[0],20,40,0);
    placement_component_set_position(&r->components[1],60,40,0);
    placement_component_set_position(&r->components[2],40,70,0);
    assert(placement_cost_overlap(r)==0);
    placement_component_set_position(&r->components[0],50,50,0);
    placement_component_set_position(&r->components[1],52,51,0);
    assert(placement_cost_overlap(r)>0);
    placement_result_free(r);free(r);
}
static void test_cost_density(void){
    PlacementResult* r=mkpr(10,0);
    for(int i=0;i<10;i++){char d[8];snprintf(d,8,"U%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,0.5);
        placement_component_set_position(&r->components[i],10.0+i*5.0,40,0);}
    r->component_count=10;
    double dc=placement_cost_density(r,10.0,0.5);(void)dc;
    assert(placement_cost_density(NULL,10.0,0.5)==0);
    placement_result_free(r);free(r);
}
static void test_cost_si(void){
    PlacementResult* r=mkpr(2,1);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,0.5);
    fc(&r->components[1],2,"U2",COMP_CAT_DIGITAL_IC,PKG_BGA_256,20,20,1.0);
    r->component_count=2;
    placement_net_init(&r->nets[0],1,"CRIT");r->nets[0].pin_count=2;
    r->nets[0].is_critical=1;r->net_count=1;
    placement_component_add_pad(&r->components[0],1,"P",0,0,0.3,0.3);
    placement_component_add_pad(&r->components[1],1,"P",0,0,0.3,0.3);
    r->components[0].net_ids[0]=1;r->components[1].net_ids[0]=1;
    placement_component_set_position(&r->components[0],30,40,0);
    placement_component_set_position(&r->components[1],50,40,0);
    assert(placement_cost_signal_integrity(r)==0);
    placement_result_free(r);free(r);
}
static void test_cost_mfg(void){
    PlacementResult* r=mkpr(2,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,0.5);
    fc(&r->components[1],2,"U2",COMP_CAT_CRYSTAL_OSC,PKG_SOIC_8,5,5,0.1);
    r->components[0].envelope.z_max=5;r->components[1].envelope.z_max=15;
    r->component_count=2;
    placement_component_set_position(&r->components[0],50,40,0);
    placement_component_set_position(&r->components[1],55,40,0);
    double mc=placement_cost_manufacturability(r);(void)mc;
    assert(placement_cost_manufacturability(NULL)==0);
    placement_result_free(r);free(r);
}
static void test_cost_weights(void){
    CostWeights w; placement_cost_weights_init_default(&w);
    assert(w.w_hpwl>0.4); assert(w.w_thermal>0);
    placement_cost_weights_init_default(NULL);
}
static void test_cost_total(void){
    PlacementResult* r=mkpr(3,1);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_32,8,8,0.5);
    fc(&r->components[1],2,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
    fc(&r->components[2],3,"C1",COMP_CAT_PASSIVE,PKG_SMD_0805,2,1.2,0);
    r->component_count=3;
    placement_net_init(&r->nets[0],1,"SIG");r->nets[0].pin_count=2;r->net_count=1;
    placement_component_add_pad(&r->components[0],1,"A",0,0,0.3,0.3);
    placement_component_add_pad(&r->components[1],1,"B",0,0,0.3,0.3);
    r->components[0].net_ids[0]=1;r->components[1].net_ids[0]=1;
    placement_component_set_position(&r->components[0],30,40,0);
    placement_component_set_position(&r->components[1],60,40,0);
    placement_component_set_position(&r->components[2],50,70,0);
    CostWeights w; placement_cost_weights_init_default(&w);
    PlacementCost pc=placement_cost_compute_total(r,&w,25,10,0.8);
    assert(pc.wire_length_cost>0);assert(pc.total_cost>0);
    PlacementCost pc2=placement_cost_compute_total(NULL,&w,0,0,0);
    assert(pc2.total_cost==0);
    placement_result_free(r);free(r);
}
/* ===== L4: Constraints ===== */
static void test_c_spacing(void){
    Component a,b;
    placement_component_init(&a,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_32);
    placement_component_init(&b,"U2",COMP_CAT_ANALOG_IC,PKG_SOIC_8);
    a.body.width=10;a.body.height=10;b.body.width=5;b.body.height=4;
    a.comp_id=1;b.comp_id=2;
    placement_component_set_position(&a,20,40,0);
    placement_component_set_position(&b,60,40,0);
    assert(placement_constraint_check_spacing(&a,&b,IPC_LEVEL_B,NULL));
}

static void test_c_all_spacing(void){
    PlacementResult* r=mkpr(3,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_32,8,8,0.2);
    fc(&r->components[1],2,"U2",COMP_CAT_ANALOG_IC,PKG_SOIC_8,5,4,0.1);
    fc(&r->components[2],3,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
    r->component_count=3;
    placement_component_set_position(&r->components[0],20,40,0);
    placement_component_set_position(&r->components[1],60,40,0);
    placement_component_set_position(&r->components[2],40,70,0);
    ConstraintResult cr=placement_constraint_check_all_spacing(r,IPC_LEVEL_B);
    assert(cr.all_clear); placement_constraint_result_free(&cr);
    placement_result_free(r);free(r);
}
static void test_c_ipc(void){
    double s=placement_constraint_get_ipc_spacing(PKG_QFP_64,PKG_QFP_64,IPC_LEVEL_B);
    assert(s>0);
    double sa=placement_constraint_get_ipc_spacing(PKG_SMD_0603,PKG_SMD_0603,IPC_LEVEL_A);
    double sc=placement_constraint_get_ipc_spacing(PKG_SMD_0603,PKG_SMD_0603,IPC_LEVEL_C);
    assert(sa>sc);
}
static void test_c_boundary(void){
    Board b; placement_board_init(&b,"T",100,80,2);
    Component c; placement_component_init(&c,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603);
    c.body.width=10;c.body.height=6;
    placement_component_set_position(&c,50,40,0);
    assert(placement_constraint_check_board_boundary(&c,&b));
    placement_component_set_position(&c,-5,40,0);
    assert(!placement_constraint_check_board_boundary(&c,&b));
}

static void test_c_keepout(void){
    KeepOutZone z[1];memset(z,0,sizeof(z));
    z[0].region.origin.x=0;z[0].region.origin.y=0;
    z[0].region.width=20;z[0].region.height=20;
    strcpy(z[0].reason,"Hole");z[0].restrict_top=1;z[0].restrict_bottom=1;
    Component c; placement_component_init(&c,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_32);
    c.body.width=10;c.body.height=10;c.mount=MOUNT_SMD_TOP;
    placement_component_set_position(&c,50,40,0);
    assert(placement_constraint_check_keepout(&c,z,1));
    placement_component_set_position(&c,10,10,0);
    assert(!placement_constraint_check_keepout(&c,z,1));
}

static void test_c_thermal(void){
    PlacementResult* r=mkpr(3,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,1.0);
    fc(&r->components[1],2,"U2",COMP_CAT_POWER,PKG_TO_220,10,15,3.0);
    fc(&r->components[2],3,"R1",COMP_CAT_PASSIVE,PKG_SMD_0805,2,1.2,0.1);
    r->component_count=3;
    placement_component_set_position(&r->components[0],30,40,0);
    placement_component_set_position(&r->components[1],70,40,0);
    placement_component_set_position(&r->components[2],50,70,0);
    Violation v; placement_constraint_check_thermal(r,25.0,&v);
    assert(placement_constraint_check_thermal(NULL,0,NULL));
    placement_result_free(r);free(r);
}
static void test_c_therm_coupling(void){
    Component a,b;
    placement_component_init(&a,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64);
    placement_component_init(&b,"U2",COMP_CAT_POWER,PKG_TO_220);
    a.body.width=10;a.body.height=10;b.body.width=10;b.body.height=15;
    placement_component_set_position(&a,30,40,0);
    placement_component_set_position(&b,35,40,0);
    double cp=placement_constraint_thermal_coupling(&a,&b,1.6);
    assert(cp>0);
    placement_component_set_position(&b,90,40,0);
    double cf=placement_constraint_thermal_coupling(&a,&b,1.6);
    assert(cf<cp);
}
static void test_c_trace(void){
    PlacementResult* r=mkpr(2,1);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,0.5);
    fc(&r->components[1],2,"U2",COMP_CAT_DIGITAL_IC,PKG_BGA_256,20,20,1.0);
    r->component_count=2;
    placement_net_init(&r->nets[0],1,"CLK");r->nets[0].pin_count=2;r->net_count=1;
    placement_component_add_pad(&r->components[0],1,"A",0,0,0.3,0.3);
    placement_component_add_pad(&r->components[1],1,"B",0,0,0.3,0.3);
    r->components[0].net_ids[0]=1;r->components[1].net_ids[0]=1;
    placement_component_set_position(&r->components[0],30,40,0);
    placement_component_set_position(&r->components[1],40,40,0);
    assert(placement_constraint_check_trace_length(r,1,200.0,NULL));
    Violation v;memset(&v,0,sizeof(v));
    assert(!placement_constraint_check_trace_length(r,1,5.0,&v));
    assert(v.type==CONSTRAINT_HIGH_SPEED);
    placement_result_free(r);free(r);
}
static void test_c_diffpair(void){
    PlacementResult* r=mkpr(2,2);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_32,8,8,0.2);
    fc(&r->components[1],2,"U2",COMP_CAT_DIGITAL_IC,PKG_QFP_32,8,8,0.2);
    r->component_count=2;
    placement_net_init(&r->nets[0],1,"DP_P");r->nets[0].pin_count=2;
    placement_net_init(&r->nets[1],2,"DP_N");r->nets[1].pin_count=2;r->net_count=2;
    for(int i=0;i<2;i++) placement_component_add_pad(&r->components[0],i+1,"P",0,0,0.3,0.3);
    for(int i=0;i<2;i++) placement_component_add_pad(&r->components[1],i+1,"P",0,0,0.3,0.3);
    r->components[0].net_ids[0]=1;r->components[0].net_ids[1]=2;
    r->components[1].net_ids[0]=1;r->components[1].net_ids[1]=2;
    placement_component_set_position(&r->components[0],20,40,0);
    placement_component_set_position(&r->components[1],60,40,0);
    assert(placement_constraint_check_diff_pair(r,1,2,200.0,NULL));
    placement_result_free(r);free(r);
}
static void test_c_wave(void){
    Component c; placement_component_init(&c,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603);
    c.body.width=1.6;c.body.height=0.8;c.mount=MOUNT_SMD_BOTTOM;
    placement_component_set_position(&c,30,40,0);
    placement_constraint_check_wave_solder(&c,0.0,NULL);
    c.mount=MOUNT_SMD_TOP;
    assert(placement_constraint_check_wave_solder(&c,0.0,NULL));
}
static void test_c_shadow(void){
    Component t,s;
    placement_component_init(&t,"T1",COMP_CAT_CRYSTAL_OSC,PKG_SOIC_8);
    placement_component_init(&s,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603);
    t.body.width=5;t.body.height=5;t.envelope.z_max=10;t.envelope.z_min=0;
    s.body.width=1.6;s.body.height=0.8;s.envelope.z_max=0.5;s.envelope.z_min=0;
    t.comp_id=1;s.comp_id=2;
    placement_component_set_position(&t,30,40,0);
    placement_component_set_position(&s,33,40,0);
    assert(!placement_constraint_check_shadowing(&t,&s,0.0,NULL));
}
static void test_c_height(void){
    Component c; placement_component_init(&c,"U1",COMP_CAT_CRYSTAL_OSC,PKG_SOIC_8);
    c.envelope.z_max=8;c.envelope.z_min=0;
    assert(placement_constraint_check_height(&c,10));
    assert(!placement_constraint_check_height(&c,5));
}
static void test_c_connector(void){
    Board b; placement_board_init(&b,"T",100,80,2);
    Component c; placement_component_init(&c,"J1",COMP_CAT_CONNECTOR,PKG_DIP_8);
    c.body.width=15;c.body.height=10;c.category=COMP_CAT_CONNECTOR;
    placement_component_set_position(&c,30,40,0);
    assert(placement_constraint_check_connector_clearance(&c,5,5,&b));
    placement_component_set_position(&c,2,40,0);
    assert(!placement_constraint_check_connector_clearance(&c,10,10,&b));
}
static void test_c_all(void){
    PlacementResult* r=mkpr(4,2);
    for(int i=0;i<4;i++){char d[8];snprintf(d,8,"U%d",i+1);
        fc(&r->components[i],i+1,d,i<2?COMP_CAT_DIGITAL_IC:COMP_CAT_PASSIVE,
           i<2?PKG_QFP_32:PKG_SMD_0603,i<2?8.0:1.6,i<2?8.0:0.8,i<2?0.5:0);}
    r->component_count=4;
    placement_net_init(&r->nets[0],1,"S1");r->nets[0].pin_count=2;
    placement_net_init(&r->nets[1],2,"S2");r->nets[1].pin_count=2;r->net_count=2;
    for(int i=0;i<4;i++) placement_component_add_pad(&r->components[i],1,"P",0,0,0.3,0.3);
    r->components[0].net_ids[0]=1;r->components[1].net_ids[0]=1;
    r->components[2].net_ids[0]=2;r->components[3].net_ids[0]=2;
    placement_component_set_position(&r->components[0],20,40,0);
    placement_component_set_position(&r->components[1],40,40,0);
    placement_component_set_position(&r->components[2],60,40,0);
    placement_component_set_position(&r->components[3],80,40,0);
    KeepOutZone z[1];memset(z,0,sizeof(z));
    z[0].region.origin.x=0;z[0].region.origin.y=0;
    z[0].region.width=5;z[0].region.height=5;
    strcpy(z[0].reason,"Screw");z[0].restrict_top=1;
    ConstraintResult cr=placement_constraint_check_all(r,IPC_LEVEL_B,z,1,25);
    placement_constraint_result_free(&cr);
    placement_result_free(r);free(r);
}


/* ===== L5: Strategy Algorithms ===== */
static void test_s_greedy(void){
    PlacementResult* r=mkpr(8,4);
    for(int i=0;i<8;i++){char d[8];snprintf(d,8,"U%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
        placement_component_add_pad(&r->components[i],1,"P",0,0,0.3,0.3);}
    r->component_count=8;
    for(int n=0;n<4;n++){char nm[8];snprintf(nm,8,"N%d",n);
        placement_net_init(&r->nets[n],n+1,nm);r->nets[n].pin_count=2;
        r->components[n*2].net_ids[0]=n+1;r->components[n*2+1].net_ids[0]=n+1;}
    r->net_count=4;
    assert(placement_strategy_greedy(r)==8);
    for(int i=0;i<8;i++) assert(r->components[i].is_placed);
    placement_result_free(r);free(r);
}
static void test_s_sa(void){
    PlacementResult* r=mkpr(8,4);
    for(int i=0;i<8;i++){char d[8];snprintf(d,8,"C%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
        placement_component_add_pad(&r->components[i],1,"P",0,0,0.3,0.3);}
    r->component_count=8;
    for(int n=0;n<4;n++){placement_net_init(&r->nets[n],n+1,"N");r->nets[n].pin_count=2;
        r->components[n*2].net_ids[0]=n+1;r->components[n*2+1].net_ids[0]=n+1;}
    r->net_count=4;
    SAConfig sa={100.0,0.1,COOLING_EXPONENTIAL,0.9,10,100,12345,0.3,10.0};
    assert(placement_strategy_simulated_annealing(r,&sa)>0);
    placement_result_free(r);free(r);
}
static void test_s_fd(void){
    PlacementResult* r=mkpr(8,4);
    for(int i=0;i<8;i++){char d[8];snprintf(d,8,"D%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_PASSIVE,PKG_SMD_0805,2,1.2,0);
        placement_component_add_pad(&r->components[i],1,"P",0,0,0.3,0.3);}
    r->component_count=8;
    for(int n=0;n<4;n++){placement_net_init(&r->nets[n],n+1,"N");r->nets[n].pin_count=2;
        r->components[n*2].net_ids[0]=n+1;r->components[n*2+1].net_ids[0]=n+1;}
    r->net_count=4;
    FDConfig fd={0.5,5000,15,0.85,0.5,50};
    assert(placement_strategy_force_directed(r,&fd)>0);
    placement_result_free(r);free(r);
}
static void test_s_part(void){
    PlacementResult* r=mkpr(6,3);
    for(int i=0;i<6;i++){char d[8];snprintf(d,8,"P%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_DIGITAL_IC,PKG_SOIC_8,5,4,0.1);
        placement_component_add_pad(&r->components[i],1,"X",0,0,0.3,0.3);}
    r->component_count=6;
    for(int n=0;n<3;n++){placement_net_init(&r->nets[n],n+1,"N");r->nets[n].pin_count=2;
        r->components[n*2].net_ids[0]=n+1;r->components[n*2+1].net_ids[0]=n+1;}
    r->net_count=3;
    PartitionConfig pc={2,0.2,0};
    assert(placement_strategy_partition_bisection(r,&pc)>0);
    placement_result_free(r);free(r);
}
static void test_s_ga(void){
    PlacementResult* r=mkpr(5,2);
    for(int i=0;i<5;i++){char d[8];snprintf(d,8,"G%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
        placement_component_add_pad(&r->components[i],1,"P",0,0,0.3,0.3);}
    r->component_count=5;
    placement_net_init(&r->nets[0],1,"N1");r->nets[0].pin_count=2;
    r->components[0].net_ids[0]=1;r->components[1].net_ids[0]=1;
    placement_net_init(&r->nets[1],2,"N2");r->nets[1].pin_count=2;
    r->components[2].net_ids[0]=2;r->components[3].net_ids[0]=2;r->net_count=2;
    GAConfig ga={20,10,0.8,0.05,3,0.1,42};
    assert(placement_strategy_genetic_algorithm(r,&ga)>0);
    placement_result_free(r);free(r);
}
static void test_s_cluster(void){
    PlacementResult* r=mkpr(12,0);
    for(int i=0;i<12;i++){char d[8];snprintf(d,8,"K%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
        placement_component_set_position(&r->components[i],(i%4)*20.0+10,(i/4)*20.0+10,0);}
    r->component_count=12;
    ClusterConfig cc={3,0,0.5,20};
    assert(placement_strategy_clustering(r,&cc)>0);
    placement_result_free(r);free(r);
}
static void test_s_exec(void){
    PlacementResult* r=mkpr(4,2);
    for(int i=0;i<4;i++){char d[8];snprintf(d,8,"E%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
        placement_component_add_pad(&r->components[i],1,"P",0,0,0.3,0.3);}
    r->component_count=4;
    placement_net_init(&r->nets[0],1,"N1");r->nets[0].pin_count=2;
    r->components[0].net_ids[0]=1;r->components[1].net_ids[0]=1;
    placement_net_init(&r->nets[1],2,"N2");r->nets[1].pin_count=2;
    r->components[2].net_ids[0]=2;r->components[3].net_ids[0]=2;r->net_count=2;
    StrategyConfig sc;placement_strategy_config_init(&sc,STRATEGY_GREEDY);
    assert(placement_strategy_execute(r,&sc)>0);
    placement_result_free(r);free(r);
}


/* ===== L5: Pareto Front ===== */
static void test_pareto(void){
    ParetoFront pf; placement_pareto_front_init(&pf,10);
    assert(pf.count==0&&pf.capacity==10);
    ObjectivePoint p1={10,20,5,0},p2={12,18,6,1},p3={8,25,4,2};
    placement_pareto_front_insert(&pf,&p1);
    placement_pareto_front_insert(&pf,&p2);
    placement_pareto_front_insert(&pf,&p3);
    assert(pf.count>0);
    ObjectivePoint d={5,10,2,0},dd={10,20,5,1};
    assert(placement_pareto_dominates(&d,&dd));
    assert(!placement_pareto_dominates(&dd,&d));
    ObjectivePoint ref={30,40,15,0};
    double hv=placement_pareto_hypervolume(&pf,&ref);
    assert(hv>=0);
    placement_pareto_front_free(&pf);
}

/* ===== L6: Thermal Analysis ===== */
static void test_t_network(void){
    PlacementResult* r=mkpr(3,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,2);
    fc(&r->components[1],2,"U2",COMP_CAT_POWER,PKG_TO_220,10,15,5);
    fc(&r->components[2],3,"R1",COMP_CAT_PASSIVE,PKG_SMD_0805,2,1.2,0.25);
    r->component_count=3;
    placement_component_set_position(&r->components[0],30,40,0);
    placement_component_set_position(&r->components[1],70,40,0);
    placement_component_set_position(&r->components[2],50,20,0);
    ThermalNetwork tn;memset(&tn,0,sizeof(tn));
    assert(placement_thermal_build_network(&tn,r,25));
    assert(tn.node_count>=2&&tn.edge_count>0);
    placement_thermal_network_free(&tn);
    placement_result_free(r);free(r);
}
static void test_t_solve(void){
    PlacementResult* r=mkpr(2,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,1);
    fc(&r->components[1],2,"U2",COMP_CAT_POWER,PKG_TO_220,10,15,3);
    r->component_count=2;
    placement_component_set_position(&r->components[0],30,40,0);
    placement_component_set_position(&r->components[1],70,40,0);
    ThermalNetwork tn;memset(&tn,0,sizeof(tn));
    assert(placement_thermal_build_network(&tn,r,25));
    assert(placement_thermal_solve_steady_state(&tn));
    for(uint32_t i=1;i<tn.node_count;i++)
        assert(tn.nodes[i].temperature_C>=tn.ambient_temperature_C);
    double tj=placement_thermal_junction_temp(&r->components[0],&tn);
    assert(tj>=25);
    placement_thermal_network_free(&tn);
    placement_result_free(r);free(r);
}
static void test_t_spread(void){
    Component c;placement_component_init(&c,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64);
    c.body.width=10;c.body.height=10;
    double R=placement_thermal_spreading_resistance(&c,0.3,1.6);
    assert(R>0);
    assert(placement_thermal_spreading_resistance(NULL,0.3,1.6)==0);
}
static void test_t_temp_at(void){
    double T=placement_thermal_temperature_at(30,40,50,40,2.0,0.3,1.6,10.0,25);
    assert(T>25);
    double Tf=placement_thermal_temperature_at(90,40,50,40,2.0,0.3,1.6,10.0,25);
    assert(Tf<T);
}
static void test_t_via_R(void){
    ThermalVia tv;memset(&tv,0,sizeof(tv));
    tv.drill_diameter_mm=0.3;tv.outer_diameter_mm=0.4;
    tv.plating_thickness_um=25;tv.via_count=1;
    double R=placement_thermal_via_resistance(&tv,1.6);
    assert(R>0&&R<1e12);
    tv.via_count=4;
    assert(placement_thermal_via_resistance(&tv,1.6)<R);
    assert(placement_thermal_via_resistance(NULL,1.6)>1e10);
}
static void test_t_vias_req(void){
    Component c;placement_component_init(&c,"U1",COMP_CAT_POWER,PKG_TO_220);
    c.body.width=10;c.body.height=15;c.power_dissipation_W=5;
    c.max_junction_temp_C=125;c.theta_JC_C_per_W=5;c.theta_JA_C_per_W=30;
    Rect2D a={{0,0},10,10};
    int32_t n=placement_thermal_vias_required(&c,100,25,0.3,1.6,a,0.3);
    assert(n>=-1);
    c.power_dissipation_W=0;
    assert(placement_thermal_vias_required(&c,100,25,0.3,1.6,a,0.3)==0);
}
static void test_t_hotspots(void){
    PlacementResult* r=mkpr(3,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,0.5);
    fc(&r->components[1],2,"U2",COMP_CAT_POWER,PKG_TO_220,10,15,8);
    fc(&r->components[2],3,"U3",COMP_CAT_DIGITAL_IC,PKG_QFP_32,8,8,0.3);
    r->component_count=3;
    placement_component_set_position(&r->components[0],20,40,0);
    placement_component_set_position(&r->components[1],50,40,0);
    placement_component_set_position(&r->components[2],80,40,0);
    ThermalNetwork tn;memset(&tn,0,sizeof(tn));
    if(placement_thermal_build_network(&tn,r,25)&&placement_thermal_solve_steady_state(&tn)){
        Point2D spots[4];
        uint32_t n=placement_thermal_detect_hotspots(r,&tn,5,spots,4);(void)n;
        double mg=placement_thermal_max_gradient(&tn);
        assert(mg>=0);
        placement_thermal_network_free(&tn);
    }
    placement_result_free(r);free(r);
}


/* ===== Utilities ===== */
static void test_u_dist(void){
    Point2D a={0,0},b={3,4};
    ASSERT_NEAR(placement_util_distance(&a,&b),5,0.001);
    ASSERT_NEAR(placement_util_manhattan_distance(&a,&b),7,0.001);
}
static void test_u_rect(void){
    Rect2D a={{0,0},10,10},b={{5,5},10,10};
    assert(placement_util_rect_overlap(&a,&b));
    ASSERT_NEAR(placement_util_rect_overlap_area(&a,&b),25,0.01);
    Rect2D c={{20,20},10,10};
    assert(!placement_util_rect_overlap(&a,&c));
    Rect2D isect;
    assert(placement_util_rect_intersection(&a,&b,&isect));
}
static void test_u_rotate(void){
    Point2D p={10,0},o={0,0};
    placement_util_rotate_point(&p,&o,90);
    ASSERT_NEAR(p.x,0,0.001);ASSERT_NEAR(p.y,10,0.001);
}
static void test_u_rotated_bb(void){
    Rect2D r={{0,0},10,6};
    Point2D o={0,0};
    Rect2D bb=placement_util_bounding_box_rotated(&r,&o,0);
    ASSERT_NEAR(bb.origin.x,-5,0.01);
    ASSERT_NEAR(bb.width,10,0.01);
}
static void test_u_hull(void){
    Point2D pts[]={{0,0},{10,0},{10,10},{0,10},{5,5}};
    Point2D hull[5];uint32_t hc=0;
    placement_util_convex_hull(pts,5,hull,&hc);
    assert(hc>=4);
}
static void test_u_quadtree(void){
    PlacementResult* r=mkpr(10,0);
    for(int i=0;i<10;i++){char d[8];snprintf(d,8,"Q%d",i+1);
        fc(&r->components[i],i+1,d,COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
        placement_component_set_position(&r->components[i],i*10+5,40,0);}
    r->component_count=10;
    QuadTree qt;memset(&qt,0,sizeof(qt));
    assert(placement_util_quadtree_build(&qt,r,5));
    Rect2D qr={{25,30},30,20};uint32_t results[10];
    uint32_t nf=placement_util_quadtree_query(&qt,&qr,results,10);
    assert(nf>0);
    Point2D pt={55,42};int32_t nid=-1;
    double d=placement_util_quadtree_nearest(&qt,r,&pt,&nid);
    assert(d>=0&&nid>=0);
    placement_util_quadtree_free(&qt);
    placement_result_free(r);free(r);
}
static void test_u_random(void){
    RandomState rng;placement_util_random_init(&rng,12345);
    for(int i=0;i<100;i++){double u=placement_util_random_uniform(&rng);assert(u>=0&&u<1);}
    for(int i=0;i<100;i++){int32_t v=placement_util_random_int(&rng,0,10);assert(v>=0&&v<=10);}
    placement_util_random_init(&rng,12345);
    double u1=placement_util_random_uniform(&rng);
    RandomState rng2;placement_util_random_init(&rng2,12345);
    double u2=placement_util_random_uniform(&rng2);
    assert(u1==u2);
    double g=placement_util_random_gaussian(&rng,0,1);(void)g;
}
static void test_u_stats(void){
    double d[]={2,4,4,4,5,5,7,9};
    ASSERT_NEAR(placement_util_mean(d,8),5,0.01);
    ASSERT_NEAR(placement_util_stddev(d,8),2.138,0.1); /* sample stddev (n-1 denominator) */
    double x[]={1,2,3,4,5},y[]={2,4,6,8,10};
    ASSERT_NEAR(placement_util_correlation(x,y,5),1,0.001);
    double s,i,r2;placement_util_linear_regression(x,y,5,&s,&i,&r2);
    ASSERT_NEAR(s,2,0.01);ASSERT_NEAR(r2,1,0.01);
}
static void test_u_csv(void){
    PlacementResult* r=mkpr(3,0);
    fc(&r->components[0],1,"U1",COMP_CAT_DIGITAL_IC,PKG_QFP_64,10,10,0.5);
    fc(&r->components[1],2,"R1",COMP_CAT_PASSIVE,PKG_SMD_0603,1.6,0.8,0);
    fc(&r->components[2],3,"C1",COMP_CAT_PASSIVE,PKG_SMD_0805,2,1.2,0);
    r->component_count=3;
    placement_component_set_position(&r->components[0],30,40,0);
    placement_component_set_position(&r->components[1],60,40,90);
    placement_component_set_position(&r->components[2],50,70,180);
    assert(placement_util_export_csv(r,"_test_export.csv"));
    assert(placement_util_import_csv(r,"_test_export.csv")==3);
    placement_util_print_summary(r);
    remove("_test_export.csv");
    placement_result_free(r);free(r);
}


/* ===== Edge Cases ===== */
static void test_edges(void){
    Board bb;placement_board_init(&bb,"Large",1000,800,16);
    assert(bb.layer_count==16);
    Component c;placement_component_init(&c,"O1",COMP_CAT_PASSIVE,PKG_SMD_0201);
    c.body.width=0.6;c.body.height=0.3;
    placement_component_set_position(&c,0.31,0.16,0);
    PlacementResult er;Board eb;placement_board_init(&eb,"E",100,100,2);
    assert(placement_result_init(&er,&eb,10,10));
    assert(placement_estimate_wire_length(&er)==0);
    assert(placement_cost_hpwl(&er)==0);
    assert(placement_cost_steiner(&er)==0);
    placement_result_free(&er);
    assert(placement_cost_manufacturability(NULL)==0);
    assert(placement_cost_thermal_gradient(NULL,0)==0);
    RandomState rs;placement_util_random_init(&rs,0);
    placement_util_random_uniform(&rs);
    double sd=placement_util_stddev(NULL,0);(void)sd;
    double corr=placement_util_correlation(NULL,NULL,0);(void)corr;
}

int main(void){
    printf("\n=== mini-component-placement-strategy Test Suite ===\n\n");
    printf("L1: Definitions\n");
    RUN_TEST(comp_init); RUN_TEST(comp_add_pad); RUN_TEST(comp_set_pos);
    RUN_TEST(comp_bounds); RUN_TEST(board_init); RUN_TEST(board_layer);
    RUN_TEST(net_init); RUN_TEST(result_init); RUN_TEST(centroid);
    printf("\nL2: Core Concepts\n");
    RUN_TEST(wire_length); RUN_TEST(pos_legal);
    printf("\nL3: Cost Functions\n");
    RUN_TEST(cost_hpwl); RUN_TEST(cost_steiner); RUN_TEST(cost_thermal_v);
    RUN_TEST(cost_thermal_grad); RUN_TEST(cost_overlap); RUN_TEST(cost_density);
    RUN_TEST(cost_si); RUN_TEST(cost_mfg); RUN_TEST(cost_weights); RUN_TEST(cost_total);
    printf("\nL4: Constraint Checking\n");
    RUN_TEST(c_spacing); RUN_TEST(c_all_spacing); RUN_TEST(c_ipc);
    RUN_TEST(c_boundary); RUN_TEST(c_keepout); RUN_TEST(c_thermal);
    RUN_TEST(c_therm_coupling); RUN_TEST(c_trace); RUN_TEST(c_diffpair);
    RUN_TEST(c_wave); RUN_TEST(c_shadow); RUN_TEST(c_height);
    RUN_TEST(c_connector); RUN_TEST(c_all);
    printf("\nL5: Algorithms & Strategies\n");
    RUN_TEST(s_greedy); RUN_TEST(s_sa); RUN_TEST(s_fd);
    RUN_TEST(s_part); RUN_TEST(s_ga); RUN_TEST(s_cluster);
    RUN_TEST(s_exec); RUN_TEST(pareto);
    printf("\nL6: Thermal Analysis\n");
    RUN_TEST(t_network); RUN_TEST(t_solve); RUN_TEST(t_spread);
    RUN_TEST(t_temp_at); RUN_TEST(t_via_R); RUN_TEST(t_vias_req);
    RUN_TEST(t_hotspots);
    printf("\nUtilities\n");
    RUN_TEST(u_dist); RUN_TEST(u_rect); RUN_TEST(u_rotate);
    RUN_TEST(u_rotated_bb); RUN_TEST(u_hull); RUN_TEST(u_quadtree);
    RUN_TEST(u_random); RUN_TEST(u_stats); RUN_TEST(u_csv);
    printf("\nEdge Cases\n");
    RUN_TEST(edges);
    printf("\n=== All Tests PASSED ===\n\n");
    return 0;
}
