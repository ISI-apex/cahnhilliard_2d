#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <omp.h>
#include "run_ch_solver.h"

static inline int min(int a, int b)
{
  return a < b ? a : b;
}

int main()
{

  // ***************************************************************
  // Example driver program for 2D modified Cahn-Hilliard
  // No thermal dependence
  // No polymer property dependence
  // No thermal dynamics
  // Scalar coefficients
  // ***************************************************************

  CHparamsScalar chparams;
  SimInfo info;
  
  // *********  Inputs  ***********
  info.nx               = 1024;
  info.ny               = 1024;
  info.dx               = 1.0 / info.nx;
  info.dy               = 1.0 / info.ny;
  info.t0               = 0.0;
  info.bc               = "periodic";
  info.rhs_type         = "ch_non_thermal";

  double eps_2          = pow( 0.01 ,2 );
  chparams.eps_2        = eps_2;
  chparams.b            = eps_2 / info.dx / info.dx;
  chparams.u            = eps_2 / info.dx / info.dx;
  chparams.sigma        = eps_2 / info.dx / info.dx / info.dx / info.dx / 200.0;
  chparams.m            = 0.0;
  chparams.sigma_noise  = 0.0;
  
  int n_tsteps        = 25;
  int calced_tsteps   = 2; // timesteps actually computed (no effect on sim)
  double n_dt         = 300.0;
  // ******************************

  // Compute linear timescales
  double dt_biharm  = (info.dx * info.dx * info.dx * info.dx) / chparams.eps_2;
  double dt_diff    = info.dx * info.dx / chparams.u;
  double dt_lin     = 1.0 / chparams.sigma;

  // Setup temporal checkpointing
  double tf         = n_dt * dt_biharm;
  double dt_check   = tf / n_tsteps;


  std::cout << "Biharmonic timescale dt_biharm = " << dt_biharm << std::endl;
  std::cout << "Diffusion timescale dt_diff = " << dt_diff/dt_biharm << " dt_biharm" << std::endl;
  std::cout << "Linear timescale dt_lin = " << dt_lin/dt_biharm << " dt_biharm" << std::endl;

  // Run solver
  for (int i=0; i < min(n_tsteps, calced_tsteps); i++) {
    info.t0 = i * dt_check;
    info.tf = (i+1) * dt_check;
    std::cout << "t0 = " << info.t0/dt_biharm << " dt_biharm , tf = " << info.tf/dt_biharm << " dt_biharm" << std::endl;
    run_ch_solver(chparams , info);
  }
    
}
