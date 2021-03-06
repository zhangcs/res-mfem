 /* This sample code solves the oil_water_phase equation:         *
 * ****************************************************************
 * problem: p_{t} -\Delta{p} = func_f(x, t) \in \Omega X [0, T]   *
 *          p = func_g (x, t) \in \partial\Omega X [0, T]         * 
 *          p = func_p0       \in \Omega t = 0.0                  *
 *WARNING! The unit is h, m, mpa!!                                *
 b_o = 1./ bo, mu_o = 1./ muo b_g = 1./ bg mu_g = 1./mug;
 revision 1.1
date: 2011/04/29 01:18:39;  author: liuming;  state: Exp;
 ******************************************************************/
#include "phg.h"
#include <string.h>
#include <math.h>
#include "parameter.c"
#include "well.c"
#include "quadfunc.c"
#include "uzawa.c"
#define USE_UZAWA 1
#define USE_BLOCK 0
#define DEBUG 0
#define USE_OIL_EQN 1
#define USE_GAS_EQN 0
static double
elapsed_time(GRID *g, BOOLEAN flag, double mflops)
/* returns and prints elapsed time since last call to this function */
{
    static double t0 = 0.0;
    double et, tt[3];
    size_t mem;
 
    phgGetTime(tt);
    et = tt[2] - t0;
    t0 = tt[2];

    mem = phgMemoryUsage(g, NULL);

    if (flag) {
	if (mflops <= 0)
	    phgPrintf("[%0.4lgMB %0.4lfs]\n", mem / (1024.0 * 1024.0), et);
	else
	    phgPrintf("[%0.4lgMB %0.4lfs %0.4lgGF]\n", mem / (1024.0 * 1024.0),
			 et, mflops*1e-3);
    }
    return et;
}
static void
/*build_linear_system*/
build_mat_vec(DOF *u_o, DOF *dp, DOF *p_h, DOF *ds, DOF *s_o, DOF *s_o_l, DOF *mu_o, DOF *b_o, DOF *b_o_l, DOF *kro, DOF *dot_kro, DOF *phi, DOF *phi_l, DOF *dot_phi, DOF *dot_bo, DOF *dot_muo, DOF *Rs, DOF *Rs_l, DOF *dot_Rs, DOF *q_o, DOF *mu_g, DOF *b_g, DOF *b_g_l, DOF *krg, DOF *dot_krg, DOF *dot_bg, DOF *dot_mug, DOF *q_g, MAP *map_u, MAP *map_p, MAT *A, MAT *B, MAT *TB, MAT *C, VEC *vec_f, VEC *vec_g)
{
	GRID *g = u_o->g;
	SIMPLEX *e;
	FLOAT *p_so, *p_sol, *p_bo, *p_bol, *p_kro, *p_phi, *p_phil, *p_dotbo, *p_Rs, *p_dotRs, *p_bg, *p_bgl, *p_krg, *p_dotbg, *p_muo, *p_mug, *p_dotphi, *p_Rsl, *p_dp, *p_ds, *p_dotmuo, *p_dotmug, *p_dotkro, *p_dotkrg;
	INT N = u_o->type->nbas * u_o->dim;
	INT M = dp->type->nbas * dp->dim;
	INT I[N], J[M];
	FLOAT mat_A[N][N], mat_TB[N][M], mat_B[M][N], mat_C[M][M], rhs_f[N], rhs_g[M];
	phgVecDisassemble(vec_f);
	phgVecDisassemble(vec_g);
	int i, j, k;
	ForAllElements(g, e){
		p_so = DofElementData(s_o, e->index);
		p_sol = DofElementData(s_o_l, e->index);
		p_bo = DofElementData(b_o, e->index);
		p_bol = DofElementData(b_o_l, e->index);
		p_kro = DofElementData(kro, e->index);
		p_phi = DofElementData(phi, e->index);
		p_phil = DofElementData(phi_l, e->index);
		p_dotphi = DofElementData(dot_phi, e->index);
		p_dotbo = DofElementData(dot_bo, e->index);
		p_dotmuo = DofElementData(dot_muo, e->index);
		p_Rs = DofElementData(Rs, e->index);
		p_Rsl = DofElementData(Rs_l, e->index);
		p_dotRs = DofElementData(dot_Rs, e->index);
		p_bg = DofElementData(b_g, e->index);
		p_bgl = DofElementData(b_g_l, e->index);
		p_krg= DofElementData(krg, e->index);
		p_dotkrg= DofElementData(dot_krg, e->index);
		p_dotkro= DofElementData(dot_kro, e->index);
		p_dotbg = DofElementData(dot_bg, e->index);
		p_dotmug = DofElementData(dot_mug, e->index);
		p_muo = DofElementData(mu_o, e->index);
		p_mug = DofElementData(mu_g, e->index);
		p_dp = DofElementData(dp, e->index);
		p_ds = DofElementData(ds, e->index);
		/*Spend the velocity*/
		FLOAT alpha_o = 0, alpha_g = 0, T_g =0, T_o = 0;
		alpha_g = p_krg[0] * p_bg[0] * p_mug[0] + p_krg[0] * (p_mug[0] * p_dotbg[0] + p_dotmug[0] * p_bg[0]) * p_dp[0] + p_bg[0] * p_mug[0] * p_dotkrg[0] * p_ds[0]; 
		alpha_o = p_kro[0] * p_bo[0] * p_muo[0] +  p_kro[0] * (p_muo[0] * p_dotbo[0] + p_dotmuo[0] * p_bo[0]) * p_dp[0] + p_bo[0] * p_muo[0] * p_dotkro[0] * p_ds[0]; 
		T_g =  p_krg[0] * p_bg[0] * p_mug[0];
		T_o =  p_kro[0] * p_bo[0] * p_muo[0];
		for (i = 0; i < N; i++){
			for (j = 0; j < N; j++){
				mat_A[i][j] = stime * phgQuadKBasDotBas(e, u_o, i, u_o, j, QUAD_DEFAULT) / alpha_o;
			}
		}
		for (i = 0; i < N; i++){
			for (j = 0; j < M; j++){
				mat_TB[i][j] = -stime * phgQuadDivBasDotBas(e, u_o, i, dp, j, QUAD_DEFAULT);
			}
		}
		/*Create two inverse*/
		FLOAT oil_so = 0, gas_so = 0;
		for (i = 0; i < M; i++){
			for (j = 0; j < M; j++){
				oil_so = p_phi[0] * p_bo[0] * phgQuadBasDotBas(e, ds, i, ds, j, QUAD_DEFAULT);
				gas_so = p_phi[0] * (p_bg[0] - p_Rs[0] * p_bo[0]) * phgQuadBasDotBas(e, ds, i, ds, j, QUAD_DEFAULT);
			}
		}
		FLOAT beta = 0;
		beta = 1. / oil_so + (p_Rs[0] + alpha_g / alpha_o) / gas_so;
		FLOAT quad = 0., oil_cp = 0., gas_cp = 0;
		oil_cp = p_so[0] * (p_bo[0] * p_dotphi[0] + p_phi[0] * p_dotbo[0]);
		gas_cp = p_so[0] * p_phi[0] * p_bo[0] * p_dotRs[0] + p_Rs[0] * oil_cp + (1. - p_so[0]) * (p_phi[0] * p_dotbg[0] + p_bg[0] * p_dotphi[0]);
		for (i = 0; i < M; i++){
			for (j = 0; j < M; j++){
				quad = phgQuadBasDotBas(e, dp, i, dp, j, QUAD_DEFAULT);
				mat_C[i][j] = (oil_cp /oil_so + gas_cp / gas_so) * quad / beta;
			}
		}
		/*Create rhs*/
		FLOAT quad_qo = 0;
		FLOAT quad_qg = 0, quad_phi = 0; 
		FLOAT quad_phil = 0;
		FLOAT rhs_oil = 0, rhs_gas = 0;
		for (i = 0; i < M; i++){
			phgQuadDofTimesBas(e, q_o, dp, i, QUAD_DEFAULT, &quad_qo);
			phgQuadDofTimesBas(e, phi_l, dp, i, QUAD_DEFAULT, &quad_phil);
			
			phgQuadDofTimesBas(e, q_g, dp, i, QUAD_DEFAULT, &quad_qg);
			phgQuadDofTimesBas(e, phi, dp, i, QUAD_DEFAULT, &quad_phi);
			rhs_oil = -stime * quad_qo + p_bo[0] * p_so[0] * quad_phi - p_sol[0] * p_bol[0] * quad_phil;
			rhs_gas = -stime * quad_qg + (p_Rs[0] * p_bo[0] * p_so[0] + p_bg[0] * (1. - p_so[0])) * quad_phi - (p_Rsl[0] * p_bol[0] * p_sol[0] + p_bgl[0] * (1. - p_sol[0])) * quad_phil;
			rhs_g[i] = (rhs_oil / oil_so + rhs_gas / gas_so) / beta;
		}
		for (i = 0; i < N; i++){
			rhs_f[i] = stime * phgQuadDofTimesDivBas(e, p_h, u_o, i, QUAD_DEFAULT);
		}
		/* Handle Bdry Conditions */
		for (i = 0; i < N; i++){
			if (phgDofGetElementBoundaryType(u_o, e, i) & (NEUMANN | DIRICHLET)){
				bzero(mat_A[i], N *sizeof(mat_A[i][0]));
				bzero(mat_TB[i], M *sizeof(mat_TB[i][0]));
				for (j = 0; j < N; j++){
					mat_A[j][i] = 0.0;
				}
				mat_A[i][i] = 1.0;
				rhs_f[i] = 0.;
			}
		}
		for (i = 0; i < N; i++){
			for (j = 0; j < M; j++){
				mat_B[j][i] = mat_TB[i][j];
			}
		}
		for (i = 0; i < N; i++){
			I[i] = phgMapE2L(map_u, 0, e, i);
		}
		for (i = 0; i < M; i++){
			J[i] = phgMapE2L(map_p, 0, e, i);
		}
		phgMatAddEntries(A, N, I, N, I, mat_A[0]);
		phgMatAddEntries(TB, N, I, M, J, mat_TB[0]);
		phgMatAddEntries(B, M, J, N, I, mat_B[0]);
		phgMatAddEntries(C, M, J, M, J, mat_C[0]);
		phgVecAddEntries(vec_f, 0, N, I, rhs_f);
		phgVecAddEntries(vec_g, 0, M, J, rhs_g);
	}
	phgMatAssemble(A);
	phgMatAssemble(TB);
	phgMatAssemble(B);
	phgMatAssemble(C);
	phgVecAssemble(vec_f);
	phgVecAssemble(vec_g);
}
static void
oil_conservation(DOF *p_h, DOF *p0_h, DOF *phi_l, DOF *b_o_l, DOF *s_o, DOF *s_o_l, DOF *q_o, DOF *u_o)
{
	GRID *g = p_h->g;
   	DOF *phi, *b_o;		
	phi = phgDofNew(g, DOF_P0, 1, "phi", DofNoAction);
	update_phi(p_h, p0_h, phi);
 	b_o = phgDofNew(g, DOF_P0, 1, "b_o", DofNoAction);
	update_bo(p_h, b_o);	
	
	DOF *tmp;
	tmp = phgDofNew(g, DOF_P0, 1, "tmp", DofNoAction);
	phgDofSetDataByValue(tmp, 1.0);
	DOF *div_uo;
	div_uo = phgDofDivergence(u_o, NULL, NULL, NULL);
	SIMPLEX *e;
	FLOAT *p_bo, *p_phi, *p_bol, *p_phil;
	FLOAT v1 =0, v2=0, div_u =0;
	FLOAT input = 0, output = 0, div = 0;
	ForAllElements(g, e){
		p_bo = DofElementData(b_o, e->index);
		p_phi = DofElementData(phi, e->index);
		p_bol = DofElementData(b_o_l, e->index);
		p_phil = DofElementData(phi_l, e->index);
		div_u += phgQuadDofDotDof(e, div_uo, tmp, 5);
		v1 += phgQuadDofDotDof(e, q_o, tmp, 5);
		v2 += p_bo[0] * p_phi[0] * phgQuadDofDotDof(e, s_o, tmp, 5) / stime
			- p_bol[0] * p_phil[0] * phgQuadDofDotDof(e, s_o_l, tmp, 5) / stime;
	}
#if USE_MPI
    output = v2, input = v1, div = div_u;
    MPI_Allreduce(&v1, &input, 1, PHG_MPI_FLOAT, MPI_SUM, p_h->g->comm);
    MPI_Allreduce(&v2, &output, 1, PHG_MPI_FLOAT, MPI_SUM,p_h->g->comm);
    MPI_Allreduce(&div_u, &div, 1, PHG_MPI_FLOAT, MPI_SUM,p_h->g->comm);
#else
    input = v1;
    output = v2;
    div = div_u;
#endif
	phgPrintf("Oil_Conserve: LHS = %le,RHS = %le\n\n", output, input);
	phgPrintf("(Div uo, 1) = %lf\n", div);
	phgPrintf("v2 + (div_uo, 1) = %lf , v1 = %lf\n", output+div, input);
	phgDofFree(&phi);
	phgDofFree(&b_o);
	phgDofFree(&tmp);
	phgDofFree(&div_uo);
}
static void 
gas_fluidity(DOF *q_o, DOF *q_g, DOF *kro, DOF *krg, DOF *b_o, DOF *b_g, DOF *mu_o, DOF *mu_g, DOF *Rs)
{
	GRID *g = q_o->g;
	SIMPLEX *e;
	FLOAT lambda_o = 0, lambda_g = 0;
	FLOAT *p_qo, *p_qg, *p_kro, *p_krg, *p_bo, *p_bg, *p_muo, *p_mug, *p_Rs;
	ForAllElements(g, e){
			p_qg = DofElementData(q_g, e->index);
			p_qo = DofElementData(q_o, e->index);
			p_kro = DofElementData(kro, e->index);
			p_krg = DofElementData(krg, e->index);
			p_bo = DofElementData(b_o, e->index);
			p_bg = DofElementData(b_g, e->index);
			p_muo = DofElementData(mu_o, e->index);
			p_mug = DofElementData(mu_g, e->index);
			p_Rs = DofElementData(Rs, e->index);
			lambda_o = p_kro[0] * p_bo[0] * p_muo[0];
			lambda_g = p_krg[0] * p_bg[0]  * p_mug[0];
			p_qg[0] = (lambda_g + p_Rs[0] * lambda_o) / lambda_o * p_qo[0];
	}
}
static void
Solve_Pressure(DOF *u_o, DOF *dp, DOF *p_h, DOF *p0_h, DOF *q_o, DOF *ds, DOF *s_o, DOF *s_o_l, DOF *q_g, DOF *phi_l, DOF *b_o_l, DOF *b_g_l, DOF *Rs_l)
{
		GRID *g = u_o->g;	
	     MAT *A, *B, *TB, *C;
	     VEC *vec_f, *vec_g;
	     MAP *map_u, *map_p;
#if USE_BLOCK
		SOLVER *solver;
		MAT *pmat[4];
		FLOAT coef[4];
#endif
     	/* The parameter DOF */
 	  	DOF *phi, *b_o, *b_g, *kro, *krg, *Rs, *mu_o, *mu_g, *dot_bo, *dot_bg, *dot_Rs, *dot_phi, *dot_muo, *dot_mug, *dot_kro, *dot_krg;
		/*Create MAP for Mat and Vec*/
	    map_p = phgMapCreate(dp, NULL);
		map_u = phgMapCreate(u_o, NULL);
		A = phgMapCreateMat(map_u, map_u);
		A->handle_bdry_eqns = FALSE;
		B = phgMapCreateMat(map_p, map_u);
		B->handle_bdry_eqns = FALSE;
		TB = phgMapCreateMat(map_u, map_p);
		TB->handle_bdry_eqns = FALSE;
		C = phgMapCreateMat(map_p, map_p);
		C->handle_bdry_eqns = FALSE;
		vec_f = phgMapCreateVec(map_u, 1);
		vec_g = phgMapCreateVec(map_p, 1);
			
		phi = phgDofNew(g, DOF_P0, 1, "phi", DofNoAction);
		update_phi(p_h, p0_h, phi);
 		b_o = phgDofNew(g, DOF_P0, 1, "b_o", DofNoAction);
		update_bo(p_h, b_o);
 		mu_o = phgDofNew(g, DOF_P0, 1, "mu_o", DofNoAction);
		update_muo(p_h, mu_o);
 		b_g = phgDofNew(g, DOF_P0, 1, "b_g", DofNoAction);
		update_bg(p_h, b_g);
 		mu_g = phgDofNew(g, DOF_P0, 1, "mu_g", DofNoAction);
		update_mug(p_h, mu_g);
 		Rs = phgDofNew(g, DOF_P0, 1, "Rs", DofNoAction);
		update_Rs(p_h, Rs);
		kro = phgDofNew(g, DOF_P0, 1, "kro", DofNoAction);
		create_kro(s_o, kro);
		krg = phgDofNew(g, DOF_P0, 1, "krg", DofNoAction);
		create_krg(s_o, krg);
		dot_bo = phgDofNew(g, DOF_P0, 1, "dot_bo", DofNoAction);
		dot_bg = phgDofNew(g, DOF_P0, 1, "dot_bg", DofNoAction);
		dot_Rs = phgDofNew(g, DOF_P0, 1, "dot_Rs", DofNoAction);
		dot_phi = phgDofNew(g, DOF_P0, 1, "dot_phi", DofNoAction); 
		dot_kro = phgDofNew(g, DOF_P0, 1, "dot_kro", DofNoAction); 
		dot_krg = phgDofNew(g, DOF_P0, 1, "dot_krg", DofNoAction); 
		dot_muo = phgDofNew(g, DOF_P0, 1, "dot_muo", DofNoAction); 
		dot_mug = phgDofNew(g, DOF_P0, 1, "dot_mug", DofNoAction); 
		update_dot_bo(p_h, dot_bo);
		update_dot_bg(p_h, dot_bg);
		update_dot_muo(p_h, dot_muo);
		update_dot_mug(p_h, dot_mug);
		update_dot_Rs(p_h, dot_Rs);
		update_dot_phi(p_h, dot_phi);
		create_dot_krg(s_o, dot_krg);
		create_dot_kro(s_o, dot_kro);
		gas_fluidity(q_o, q_g, kro, krg, b_o, b_g, mu_o, mu_g, Rs);
		build_mat_vec(u_o, dp, p_h, ds, s_o, s_o_l, mu_o, b_o, b_o_l, kro, dot_kro, phi, phi_l, dot_phi, dot_bo, dot_muo, Rs, Rs_l, dot_Rs, q_o, mu_g, b_g, b_g_l, krg, dot_krg, dot_bg, dot_mug, q_g, map_u, map_p, A, B, TB, C, vec_f, vec_g);
		phgDofFree(&phi);
		phgDofFree(&mu_o);
		phgDofFree(&b_o);
		phgDofFree(&kro);
		phgDofFree(&mu_g);
		phgDofFree(&b_g);
		phgDofFree(&krg);
		phgDofFree(&Rs);
		phgDofFree(&dot_bo);
		phgDofFree(&dot_bg);
		phgDofFree(&dot_Rs);
		phgDofFree(&dot_phi);
		phgDofFree(&dot_muo);
		phgDofFree(&dot_mug);
		phgDofFree(&dot_krg);
		phgDofFree(&dot_kro);
#if USE_UZAWA
		int nits_amg = 0;
        int nits_uzawa = 0;
		double time_amg = 0;
		elapsed_time(g, FALSE, 0.);
		DOF *B_data = DivRT(dp, u_o);
        MAT *H = BTAB(A, B_data, dp, u_o);
      	phgDofFree(&B_data);
		phgPrintf("Assemble H:             ");
		elapsed_time(g, TRUE, 0.);
		phgPrintf("solve p_h:              ");
	//	nits_uzawa = phgUzawa(H, u_w, p_h, map_u, map_p, A, B, TB, C, vec_f, vec_g, &nits_amg, &time_amg);
		elapsed_time(g, FALSE, 0.);
		nits_uzawa = uzawapcg(H, u_o, dp, map_u, map_p, A, B, TB, C, vec_f, vec_g, &nits_amg);
		elapsed_time(g, TRUE, 0.);
		phgPrintf("MAx iter of AMG---------%d\n", nits_amg );
		phgPrintf("Nits: uzawa:----------------%d\n", nits_uzawa);
		phgMatDestroy(&H);
#endif
#if USE_BLOCK

		solver = phgSolverCreate(SOLVER_MUMPS, u_o, dp, NULL);
		solver->mat->handle_bdry_eqns = FALSE;
		pmat[0] = A;  pmat[1] = TB;
		coef[0] = 1.; coef[1] = 1.;
		pmat[2] = B;  pmat[3] = C;
		coef[2] = 1.; coef[3] = -1.;
		phgMatDestroy(&solver->mat);
		solver->mat = phgMatCreateBlockMatrix(g, 2, 2, pmat, coef, NULL);
		solver->rhs->mat = solver->mat;
    		INT N = vec_f->map->nlocal;
    		INT M = vec_g->map->nlocal;
		memcpy(solver->rhs->data, vec_f->data, sizeof(*vec_f->data) * N);
		memcpy(solver->rhs->data+N, vec_g->data, sizeof(*vec_g->data) * M);
		phgSolverSolve(solver, TRUE, u_o, dp, NULL);
		phgSolverDestroy(&solver);
#endif
		elapsed_time(g, TRUE, 0.);
		phgMatDestroy(&A);
		phgMatDestroy(&B);
		phgMatDestroy(&TB);
		phgMatDestroy(&C);
		phgMapDestroy(&map_p);
		phgMapDestroy(&map_u);
		phgVecDestroy(&vec_g);
		phgVecDestroy(&vec_f);
}
static void
gas_conservation(DOF *p_h, DOF *p0_h, DOF *phi_l, DOF *b_g_l, DOF *b_o_l, DOF *s_o, DOF *s_o_l, DOF *Rs_l, DOF *q_g)
{
	GRID *g = p_h->g;
   	DOF *phi, *b_g,  *b_o, *Rs;
	phi = phgDofNew(g, DOF_P0, 1, "phi", DofNoAction);
	update_phi(p_h, p0_h, phi);
 	b_g = phgDofNew(g, DOF_P0, 1, "b_g", DofNoAction);
	update_bg(p_h, b_g);	
 	b_o = phgDofNew(g, DOF_P0, 1, "b_o", DofNoAction);
	update_bo(p_h, b_o);	
 	Rs = phgDofNew(g, DOF_P0, 1, "Rs", DofNoAction);
	update_Rs(p_h, Rs);
	
	DOF *tmp;
	tmp = phgDofNew(g, DOF_P0, 1, "tmp", DofNoAction);
	phgDofSetDataByValue(tmp, 1.0);
	SIMPLEX *e;
	FLOAT *p_bg, *p_bgl, *p_bo, *p_bol, *p_Rs, *p_Rsl, *p_so, *p_sol;
	FLOAT v1 =0, v2=0;
	FLOAT output = 0, input = 0;
	ForAllElements(g, e){
		p_bo = DofElementData(b_o, e->index);
		p_bol = DofElementData(b_o_l, e->index);
		p_so = DofElementData(s_o, e->index);
		p_sol = DofElementData(s_o_l, e->index);
		p_Rs = DofElementData(Rs, e->index);
		p_Rsl = DofElementData(Rs_l, e->index);
		p_bgl = DofElementData(b_g_l, e->index);
		p_bg = DofElementData(b_g, e->index);
		v1 += phgQuadDofDotDof(e, q_g, tmp, 5);
		v2 += (p_Rs[0] * p_bo[0] * p_so[0] + p_bg[0] * (1. - p_so[0])) * phgQuadDofDotDof(e, phi, tmp, 5) / stime
			- (p_Rsl[0] * p_bol[0] * p_sol[0] + p_bgl[0] * (1. -p_sol[0]))* phgQuadDofDotDof(e, phi_l, tmp, 5) / stime;
	}
#if USE_MPI
    input = v1, output = v2;
    MPI_Allreduce(&v1, &input, 1, PHG_MPI_FLOAT, MPI_SUM, p_h->g->comm);
    MPI_Allreduce(&v2, &output, 1, PHG_MPI_FLOAT, MPI_SUM,p_h->g->comm);
#else
    input = v1;
    output = v2;
#endif
	phgPrintf("Gas_Conserve: LHS = %le,RHS = %le\n\n", output, input);
	phgDofFree(&phi);
	phgDofFree(&b_o);
	phgDofFree(&b_g);
	phgDofFree(&Rs);
	phgDofFree(&tmp);
}
#if USE_OIL_EQN
static void
build_oileqn_so(SOLVER *solver, DOF *ds, DOF *s_o, DOF *s_o_l, DOF *div_uo, DOF *q_o, DOF *dp, DOF *b_o, DOF *phi, DOF *dot_phi, DOF *dot_bo, DOF *phi_l, DOF *b_o_l)
{
	GRID *g = s_o->g;
	SIMPLEX *e;
	int i,j;
	int N = ds->type->nbas;
	FLOAT A[N][N], rhs[N];
	INT I[N];
	FLOAT *p_phi, *p_bo, *p_so, *p_dotbo, *p_dotphi, *p_phil, *p_bol, *p_sol;
	ForAllElements(g, e){
		p_phi = DofElementData(phi, e->index);
		p_phil = DofElementData(phi_l, e->index);
		p_bo  = DofElementData(b_o, e->index);
		p_bol = DofElementData(b_o_l, e->index);
		p_so  = DofElementData(s_o, e->index);
		p_sol  = DofElementData(s_o_l, e->index);
		p_dotbo = DofElementData(dot_bo, e->index);
		p_dotphi = DofElementData(dot_phi, e->index);
		for (i = 0; i < N; i++){
			I[i] = phgSolverMapE2L(solver, 0, e, i);
			for (j = 0; j < N; j++){
				A[i][j] =  p_bo[0] * p_phi[0] * phgQuadBasDotBas(e, ds, i, ds, j, QUAD_DEFAULT); 
			}
		}
		FLOAT quad_qo = 0, quad_dp = 0, quad_phi = 0, quad_phil = 0, quad_divuo = 0;
		FLOAT oil_cp = 0;
		oil_cp = p_so[0] * (p_bo[0] * p_dotphi[0] + p_phi[0] * p_dotbo[0]);
		for (i = 0; i < N; i++){
			phgQuadDofTimesBas(e, q_o, ds, i, QUAD_DEFAULT, &quad_qo);
			phgQuadDofTimesBas(e, dp, ds, i, QUAD_DEFAULT, &quad_dp);
			phgQuadDofTimesBas(e, phi_l, ds, i, QUAD_DEFAULT, &quad_phil);
			phgQuadDofTimesBas(e, phi, ds, i, QUAD_DEFAULT, &quad_phi);
			phgQuadDofTimesBas(e, div_uo, ds, i, QUAD_DEFAULT, &quad_divuo);
			rhs[i] = stime * quad_qo + p_sol[0] * p_bol[0] * quad_phil - p_so[0] * p_bo[0] * quad_phi - oil_cp * quad_dp - stime * quad_divuo;
		}
		for (i = 0; i < N; i++){
			phgSolverAddMatrixEntries(solver, 1, I+i, N, I, A[i]);
		}
		phgSolverAddRHSEntries(solver, N, I, rhs);
	}
}
static void
Solver_oileqn_so(DOF *ds, DOF *dp, DOF *s_o, DOF *u_o, DOF *q_o, DOF *s_o_l, DOF *p_h, DOF *p0_h, DOF *phi_l, DOF *b_o_l)
{
	GRID *g = s_o->g;
	SOLVER *solver;
	DOF *div_uo, *dot_phi, *dot_bo, *b_o, *phi;
	div_uo = phgDofDivergence(u_o, NULL, NULL, NULL);
	solver = phgSolverCreate(SOLVER_PCG, ds, NULL);
	dot_bo = phgDofNew(g, DOF_P0, 1, "dot_bo", DofNoAction);
	dot_phi = phgDofNew(g, DOF_P0, 1, "dot_phi", DofNoAction);
	update_dot_bo(p_h, dot_bo);
	update_dot_phi(p_h, dot_phi);
	phi = phgDofNew(g, DOF_P0, 1, "phi", DofNoAction);
	update_phi(p_h, p0_h, phi);
	b_o = phgDofNew(g, DOF_P0, 1, "b_o", DofNoAction);
	update_bo(p_h, b_o);
	build_oileqn_so(solver, ds, s_o, s_o_l, div_uo, q_o, dp, b_o, phi, dot_phi, dot_bo, phi_l, b_o_l);
	phgSolverSolve(solver, TRUE, ds, NULL);
	phgDofFree(&div_uo);
	phgDofFree(&dot_bo); 
	phgDofFree(&dot_phi);
	phgDofFree(&phi);
	phgDofFree(&b_o);
	phgSolverDestroy(&solver);
}
#endif
#if USE_GAS_EQN
static void
build_gaseqn_ug(SOLVER *solver, DOF *u_g, DOF *p_h, DOF *krg, DOF *b_g, DOF *mu_g, DOF *kro, DOF *b_o, DOF *mu_o)
{
	int i, j, k;
	GRID *g = p_h->g;
	SIMPLEX *e;
	ForAllElements(g, e){
		int N = DofGetNBas(u_g, e);
		FLOAT A[N][N], rhs[N], *p_krg, *p_bg, *p_mug;
		INT I[N];
		p_krg = DofElementData(krg, e->index);
		p_bg = DofElementData(b_g, e->index);
		p_mug = DofElementData(mu_g, e->index);
		for (i = 0; i < N; i++){
			I[i] = phgSolverMapE2L(solver, 0, e, i);
			for (j = 0; j < N; j++){
				   A[i][j] = phgQuadBasDotBas(e, u_g, i, u_g, j, QUAD_DEFAULT) * p_mug[0] / (p_krg[0] * p_bg[0] * K);
			}
			rhs[i] = phgQuadDofTimesDivBas(e, p_h, u_g, i, 5);
		     /* handle bdry */
	   		if (phgDofGetElementBoundaryType(u_g, e, i) & (DIRICHLET | NEUMANN)) {
				bzero(A[i], N * sizeof(A[i][0]));
				for (j = 0; j < N; j++){
					A[j][i] = 0.;
				}
				A[i][i] = 1.;
				rhs[i] = 0.;
	   		 }
		}
		for(i = 0; i < N; i++){
			phgSolverAddMatrixEntries(solver, 1, I+i, N, I, A[i]);
		}
		phgSolverAddRHSEntries(solver, N, I, rhs);
	}

}
static void
build_gaseqn_so(SOLVER *solver, DOF *s_o, DOF *s_o_l, DOF *div_uo, DOF *q_o, DOF *p_h, DOF *p_h_newton, DOF *b_o, DOF *phi, DOF *dot_phi, DOF *dot_bo, DOF *phi_l, DOF *b_o_l)
{
	GRID *g = s_o->g;
	SIMPLEX *e;
	int i,j;
	int N = s_o->type->nbas;
	FLOAT A[N][N], rhs[N];
	INT I[N];
	FLOAT *p_phi, *p_bo, *p_so, *p_dotbo, *p_dotphi, *p_phil, *p_bol;
	ForAllElements(g, e){
		p_phi = DofElementData(phi, e->index);
		p_phil = DofElementData(phi_l, e->index);
		p_bo  = DofElementData(b_o, e->index);
		p_bol = DofElementData(b_o_l, e->index);
		p_so  = DofElementData(s_o, e->index);
		p_dotbo = DofElementData(dot_bo, e->index);
		p_dotphi = DofElementData(dot_phi, e->index);
		for (i = 0; i < N; i++){
			I[i] = phgSolverMapE2L(solver, 0, e, i);
			for (j = 0; j < N; j++){
				A[i][j] =  p_bo[0] * p_phi[0] * phgQuadBasDotBas(e, s_o, i, s_o, j, QUAD_DEFAULT); 
			}
		}
		FLOAT quad_qo = 0, quad_p = 0, quad_pnew = 0, quad_sol = 0, quad_divuo = 0;
		FLOAT oil_cp = 0;
		oil_cp = p_so[0] * (p_bo[0] * p_dotphi[0] + p_phi[0] * p_dotbo[0]);
		for (i = 0; i < N; i++){
			phgQuadDofTimesBas(e, q_o, s_o, i, QUAD_DEFAULT, &quad_qo);
			phgQuadDofTimesBas(e, p_h, s_o, i, QUAD_DEFAULT, &quad_p);
			phgQuadDofTimesBas(e, p_h_newton, s_o, i, QUAD_DEFAULT, &quad_pnew);
			phgQuadDofTimesBas(e, s_o_l, s_o, i, QUAD_DEFAULT, &quad_sol);
			phgQuadDofTimesBas(e, div_uo, s_o, i, QUAD_DEFAULT, &quad_divuo);
			rhs[i] = stime * quad_qo + p_phil[0] * p_bol[0] * quad_sol - oil_cp * (quad_p - quad_pnew) - stime * quad_divuo;
		}
		for (i = 0; i < N; i++){
			phgSolverAddMatrixEntries(solver, 1, I+i, N, I, A[i]);
		}
		phgSolverAddRHSEntries(solver, N, I, rhs);
	}
}
static void
Solver_gaseqn_so(DOF *s_o, DOF *u_o, DOF *q_o, DOF *q_g, DOF *s_o_l, DOF *p_h, DOF *p_h0, DOF *p_h_newton, DOF *phi_l, DOF *b_o_l)
{
	GRID *g = s_o->g;
	SOLVER *solver;
	DOF *div_uo, *dot_phi, *dot_bo, *b_o, *phi;
	div_uo = phgDofDivergence(u_o, NULL, NULL, NULL);
	solver = phgSolverCreate(SOLVER_PCG, s_o, NULL);
	dot_bo = phgDofNew(g, DOF_P0, 1, "dot_bo", DofNoAction);
	dot_phi = phgDofNew(g, DOF_P0, 1, "dot_phi", DofNoAction);
	update_dot_bo(p_h, dot_bo);
	update_dot_phi(p_h, dot_phi);
	phi = phgDofNew(g, DOF_P0, 1, "phi", DofNoAction);
	update_phi(p_h, p_h0, phi);
	b_o = phgDofNew(g, DOF_P0, 1, "b_o", DofNoAction);
	update_bo(p_h, b_o);
	build_oileqn_so(solver, s_o, s_o_l, div_uo, q_o, p_h, p_h_newton, b_o, phi, dot_phi, dot_bo, phi_l, b_o_l);
	phgSolverSolve(solver, TRUE, s_o, NULL);
	DOF *b_g = phgDofNew(g, DOF_P0, 1, "b_g", DofNoAction);
	update_bg(p_h, b_g);
	DOF *krg = phgDofNew(g, DOF_P0, 1, "krg", DofNoAction);
	create_krg(p_h, krg);
	DOF *kro = phgDofNew(g, DOF_P0, 1, "kro", DofNoAction);
	create_kro(p_h, kro);
	DOF *mu_o = phgDofNew(g, DOF_P0, 1, "mu_o", DofNoAction);
	DOF *mu_g = phgDofNew(g, DOF_P0, 1, "mu_g", DofNoAction);
	update_muo(p_h, mu_o);
	update_mug(p_h, mu_g);
	gas_fluidity(q_o, q_g, kro, krg, b_o, b_g, mu_o, mu_g);
	phgDofFree(&b_g);
	phgDofFree(&krg);
	phgDofFree(&kro);
	phgDofFree(&mu_o);
	phgDofFree(&mu_g);
	phgDofFree(&div_uo);
	phgDofFree(&dot_bo); 
	phgDofFree(&dot_phi);
	phgDofFree(&phi);
	phgDofFree(&b_o);
	phgSolverDestroy(&solver);
}
#endif
static void
test_grid(DOF *p_h)
{
	GRID *g = p_h->g;
	SIMPLEX *e;
	ForAllElements(g, e){
		printf("e->region_mark = %d\n", e->region_mark);
	}
	exit(1);
}
int
main(int argc, char *argv[])
{
    INT mem_max = 10240;
    char *fn = "two_level.mesh";
    GRID *g;
    char vtkfile[1000];
    static int pre_refines = 0;
    phgOptionsRegisterInt("pre_refines", "Pre-refines", &pre_refines);
    phgInit(&argc, &argv);
    g = phgNewGrid(-1);
	phgPrintf("================================================\n");
	phgPrintf("\n          This program use SS method\n");
	phgPrintf("\n================================================\n");
	phgOptionsShowUsed();
    if (!phgImport(g, fn, FALSE))
	phgError(1, "can't read file \"%s\".\n", fn);
	
    phgRefineAllElements(g, pre_refines); 
		if (phgBalanceGrid(g, 1.2, 1, NULL, 0.))
	    phgPrintf("Repartition mesh, load imbalance: %lg\n",
				(double)g->lif);

    /*The pressure variable*/
    DOF *p_h;
    p_h = phgDofNew(g, DOF_P0, 1, "p_h", DofNoAction);
    phgDofSetDataByValue(p_h, PRESSURE0);
    DOF *p0_h;
    p0_h = phgDofNew(g, DOF_P0, 1, "p0_h", DofNoAction);
    phgDofSetDataByValue(p0_h, PRESSURE0);
    DOF *dp;
    dp = phgDofNew(g, DOF_P0, 1, "dp", DofNoAction);
	phgDofSetDataByValue(dp, 0);

    DOF *s_o;
    s_o = phgDofNew(g, DOF_P0, 1, "s_o", DofNoAction);
    phgDofSetDataByValue(s_o, SO0);
	DOF *ds;
	ds = phgDofNew(g, DOF_P0, 1, "ds", DofNoAction);
    phgDofSetDataByValue(ds, 0);
   /*The velocity variable*/
    DOF *u_o;
    u_o = phgDofNew(g, DOF_RT1, 1, "u_o", DofNoAction);
    DOF *u_g;
    u_g = phgDofNew(g, DOF_RT1, 1, "u_g", DofNoAction);
    /* RHS function */
    DOF *q_g;
    q_g = phgDofNew(g, DOF_P0, 1, "q_g", DofNoAction);
    DOF *q_o;			     
    q_o = phgDofNew(g, DOF_P0, 1, "q_o", DofNoAction);
    Well_init(q_o);
	Unit_Conversion();
	phgPrintf("the elements is :%d\n",DofGetDataCountGlobal(p_h));
	int flag = 0;
	while (ctime < T -1e-8 ){
		ptime = ctime;
		ctime += stime;
		phgPrintf("==================================================================\n");
		phgPrintf("current time layer: [%lf, %lf]\n", (double)ptime, (double)ctime);
		if (ctime > T){ 
			ctime = T;
			stime = T- ptime;
			phgPrintf("current time layer : [%lf, %lf]\n",(double)ptime, (double)ctime);
		}
	 	DOF *phi_l, *b_o_l, *Rs_l, *b_g_l, *s_o_l;
		phi_l = phgDofNew(g, DOF_P0, 1, "phi_l", DofNoAction);
		update_phi(p_h, p0_h, phi_l);
	 	b_o_l = phgDofNew(g, DOF_P0, 1, "b_o_l", DofNoAction);
		update_bo(p_h, b_o_l);
	 	b_g_l = phgDofNew(g, DOF_P0, 1, "b_o_l", DofNoAction);
		update_bg(p_h, b_g_l);
		s_o_l = phgDofCopy(s_o, NULL, NULL, NULL);
	 	Rs_l = phgDofNew(g, DOF_P0, 1, "Rs_l", DofNoAction);
		update_Rs(p_h, Rs_l);
		int count = 0;
		while (TRUE){
			count++;
			DOF *p_nk, *s_nk;;
			p_nk = phgDofCopy(p_h, NULL, NULL, NULL);
			s_nk = phgDofCopy(s_o, NULL, NULL, NULL);

			Solve_Pressure(u_o, dp, p_h, p0_h, q_o, ds, s_o, s_o_l, q_g, phi_l, b_o_l, b_g_l, Rs_l);
		//	Solver_gaseqn_so(s_o, u_o, q_o, q_g, s_o_l, p_h, p0_h, p_h_newton, phi_l, b_o_l);
			Solver_oileqn_so(ds, dp, s_o, u_o, q_o, s_o_l, p_h, p0_h, phi_l, b_o_l);
			phgDofAXPBY(1.0, dp, 1.0, &p_h);
			phgDofAXPBY(1.0, ds, 1.0, &s_o);

			FLOAT err_p = 0.0, norm_p = 0.0, TOL_p = 0.0;
			FLOAT err_s = 0.0, norm_s = 0.0, TOL_s = 0.0;

			err_p = L2(p_nk, p_h, 4);
			norm_p = (L2_norm(p_nk) > L2_norm(p_h))?L2_norm(p_nk):L2_norm(p_h);
			TOL_p = err_p / norm_p;
			err_s = L2(s_nk, s_o, 4);
			norm_s = (L2_norm(s_nk) > L2_norm(s_o))?L2_norm(s_nk):L2_norm(s_o);
			TOL_s = err_s / norm_s;

			phgDofFree(&s_nk);
			phgDofFree(&p_nk);
			if ((TOL_p < 1e-6 ) && (TOL_s < 1e-6)){
				phgPrintf("TOL_p:                    %le\n", TOL_p);
				phgPrintf("TOL_s:                    %le\n", TOL_s);
				phgPrintf("Non_ints:                 %d\n", count);
				break;
			}
		}
		/*Conseravation Test*/
		phgPrintf("====================Conservation Test======================\n");
		oil_conservation(p_h, p0_h, phi_l, b_o_l, s_o, s_o_l, q_o, u_o);
		gas_conservation(p_h, p0_h, phi_l, b_g_l, b_o_l, s_o, s_o_l, Rs_l, q_g);
		phgPrintf("====================End Of ConTest==========================\n");
		FLOAT pwf = 0;
		Well_Pressure(p_h, &pwf);
		phgPrintf("t = %lf, Pwf = %lf\n", ctime, pwf);
		phgDofFree(&b_o_l);
		phgDofFree(&b_g_l);
		phgDofFree(&phi_l);
		phgDofFree(&s_o_l);
		phgDofFree(&Rs_l);
#if 0
		MAP *map_p = phgMapCreate(p_h, NULL);
		VEC *y = phgMapCreateVec(map_p, 1);
 	  	phgMapDofToLocalData(map_p, 1, &p_h, y->data);
		phgVecDumpMATLAB(y, "p", "p.m");
		phgMapDestroy(&map_p);
		phgVecDestroy(&y);
		MAP *map_s = phgMapCreate(s_o, NULL);
		VEC *x = phgMapCreateVec(map_s, 1);
	 	phgMapDofToLocalData(map_s, 1, &s_o, x->data);
	  	phgVecDumpMATLAB(x, "s", "s.m");
		phgMapDestroy(&map_s);
		phgVecDestroy(&x);
		/*Create VTK files*/
		DOF *kro = phgDofNew(g, DOF_P0, 1, "kro", DofNoAction);
			create_kro(s_o, kro);
		DOF *krg = phgDofNew(g, DOF_P0, 1, "krg", DofNoAction);
			create_krg(s_o, krg);
		DOF *b_g = phgDofNew(g, DOF_P0, 1, "b_g", DofNoAction);
			update_bg(p_h, b_g);
		DOF *b_o = phgDofNew(g, DOF_P0, 1, "b_o", DofNoAction);
			update_bo(p_h, b_o);
		DOF *phi = phgDofNew(g, DOF_P0, 1, "phi", DofNoAction);
			update_phi(p_h, p0_h, phi);
		DOF *mu_o = phgDofNew(g, DOF_P0, 1, "mu_o", DofNoAction);
		DOF *mu_g = phgDofNew(g, DOF_P0, 1, "mu_g", DofNoAction);
			update_muo(p_h, mu_o);
			update_mug(p_h, mu_g);
		flag++;
		sprintf(vtkfile, "oil_gas_%03d.vtk", flag);
		phgExportVTK(g, vtkfile, p_h, s_o, q_o, q_g, u_o, kro, krg, b_g, b_o, phi, mu_o, mu_g, NULL);
		phgDofFree(&kro);
		phgDofFree(&krg);
		phgDofFree(&b_g);
		phgDofFree(&b_o);
		phgDofFree(&mu_o);
		phgDofFree(&mu_g);
		phgDofFree(&phi);
#endif
    }
    phgDofFree(&p_h);
    phgDofFree(&p0_h);
    phgDofFree(&u_o);
    phgDofFree(&u_g);
    phgDofFree(&s_o);
    phgDofFree(&q_o);
    phgDofFree(&q_g);
	phgDofFree(&dp);
	phgDofFree(&ds);
    phgFreeGrid(&g);
    phgFinalize();
    return 0;
}
