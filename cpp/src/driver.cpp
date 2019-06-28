#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <omp.h>
#include <boost/numeric/odeint.hpp>
#include "cahnhilliard.h"
#include "run_ch_solver.hpp"

int main()
{

  CHparamsScalar chparams;
  SimInfo info;
  
  // *********  Inputs  ***********
  info.nx             = 128;
  info.dx             = 1.0 / info.nx;
  info.t0             = 0.0;

  chparams.eps_2        = pow( 0.01 ,2 );
  chparams.b            = chparams.eps_2 / info.dx / info.dx;
  chparams.u            = chparams.eps_2 / info.dx / info.dx;
  chparams.sigma        = chparams.eps_2 / info.dx / info.dx / info.dx / info.dx / 200.0;
  chparams.m            = 0.5;
  chparams.sigma_noise  = 10.0;
  
  int n_tsteps        = 25;
  double n_dt         = 300.0;
  // ******************************

  double dt_biharm  = (info.dx * info.dx * info.dx * info.dx) / chparams.eps_2;
  double tf         = n_dt * dt_biharm;
  info.dt_check     = tf / n_tsteps;

  double dt_diff    = info.dx * info.dx / chparams.u;
  double dt_lin     = 1.0 / chparams.sigma;

  std::cout << "Biharmonic timescale dt_biharm = " << dt_biharm << std::endl;
  std::cout << "Diffusion timescale dt_diff = " << dt_diff/dt_biharm << " dt_biharm" << std::endl;
  std::cout << "Linear timescale dt_lin = " << dt_lin/dt_biharm << " dt_biharm" << std::endl;

  for (int i=0; i<n_tsteps; i++) {
    info.t0 = i * info.dt_check;
    info.tf = (i+1) * info.dt_check;
    std::cout << "t0 = " << info.t0/dt_biharm << " dt_biharm , tf = " << info.tf/dt_biharm << " dt_biharm" << std::endl;
    run_ch_solver<CHparamsScalar>(chparams , info);
  }
    
}
