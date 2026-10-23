/* This file is part of mcdnsa.
 * 
 * Copyright (C) 2005,2006,2007,2008 Swiss Tropical Institute
 * 
 * mcdnsa is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* This file should contain the headers of all routines that we write in the C
 * program.
 */

/* We also include library headers here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gsl/gsl_matrix.h>

#include <vector>

double calc_init_mosq_emerge_rate(
    std::vector<double>& mosq_emerge_rate_vector, // output vector
    int thetap,                                   // $\theta_p$: daysInYear
    int tau,                                      // $\tau$: mosqRestDuration
    int thetas,                                   // $\theta_s$: EIPDuration
    int n,                                        // $n$: nHostTypes
    int m,                                        // $m$: nMalHostTypes
    const std::vector<double>& ni,                // $N_i$: popSize (length n)
    const std::vector<double>& alphai,            // $\alpha_i$: hostAvailabilityRate	(length n)
    double muv_a,                                 // $\mu_{vA}: mosqSeekingDeathRate
    double thetad,                                // $\theta_d$: mosqSeekingDuration
    const std::vector<double>& p_bi,              // $P_{B_i}$: mosqProbBiting (length n)
    const std::vector<double>& p_ci,              // $P_{C_i}$: mosqProbFindRestSite (length n)
    const std::vector<double>& p_di,              // $P_{D_i}$: mosqProbResting (length n)
    const std::vector<double>& p_ei,              // $P_{E_i}$: mosqProbOvipositing -- assumed not affected by nhh
    const std::vector<double>& kvi_init,          // Kvi (size n * thetap)
    const std::vector<double>& sv_init            // Sv (length n)
);

void calc_upsilon(std::vector<gsl_matrix*>& upsilon, double& pa, double& p_ai, int thetap, int eta, int mt, int tau,
                  int thetas, int n, int m, const double* ni, const double* alphai, double muv_a, double thetad,
                  const double* p_bi, const double* p_ci, const double* p_di, const double* p_ei,
                  const gsl_matrix* kvi);

void calc_sv_jacobian(gsl_matrix* j, const std::vector<gsl_matrix*>& upsilon, gsl_matrix* inv1_xtp, int eta, int mt,
                      int thetap);

void calc_psts(double& sumkplus, double* sumklplus, int thetas, int tau, double pa, double pdf);

void func_x(gsl_matrix* x, const std::vector<gsl_matrix*>& upsilon, int t, int s, int n);

double calc_spectral_radius(gsl_matrix* a, int n);

void calc_inv1minus_a(gsl_matrix* inv1_a, gsl_matrix* a, int n);
