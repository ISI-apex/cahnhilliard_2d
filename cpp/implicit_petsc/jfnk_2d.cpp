
static char help[] = "JFNK implicit solver for 2D CH with PETSc \n";

#include <petscdm.h>
#include <petscdmda.h>
#include <petscts.h>
#include <petscvec.h>
#include <petscviewer.h>
#include <petscsys.h>
#include <petscdmcomposite.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include "utils_ch_implicit.h"
#include "boundary_conditions.h"
#include "initial_conditions.h"
#include "rhs_implicit.h"
#include "petsc_event_handling.h"
#include "temperature_dependence.h"

int main(int argc,char **argv) {

  PetscInitialize(&argc,&argv,argv[1],help);

  TS             ts;                   /* nonlinear solver */
  Vec            u,c,T,r,r_c,r_T;      /* solution, residual vectors */
  Vec            U_c , U_T;
  Mat            J,Jmf = NULL;         /* Jacobian matrices */
  PetscErrorCode ierr;
  DM             da_c , da_T , pack;
  PetscReal      dt;
  SNES           snes;
  KSP            ksp;

  AppCtx         user = parse_petsc_options();

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create distributed array (DMDA) to manage parallel grid and vectors
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */  

  // DM for concentration c
  DMDACreate2d(PETSC_COMM_WORLD, 
               DM_BOUNDARY_GHOSTED, DM_BOUNDARY_GHOSTED,    // type of boundary nodes
               DMDA_STENCIL_BOX,                // type of stencil
               11,11,                           // global dimns of array
               PETSC_DECIDE,PETSC_DECIDE,       // #procs in each dimn
               1,                               // DOF per node
               2,                               // Stencil width
               NULL,NULL,&da_c);
  DMSetFromOptions(da_c);
  DMSetOptionsPrefix(da_c,"c_");
  DMSetUp(da_c);

  // Get process ownership ranges so that you can link different physics with the same indices
  const PetscInt *lxc , *lyc;
  PetscInt *lxT , *lyT;
  PetscInt sizes_x , sizes_y;
  PetscInt nx , ny;
  DMDAGetOwnershipRanges( da_c , &lxc , &lyc , 0 );
  DMDAGetInfo( da_c , NULL, &nx,&ny,NULL, &sizes_x,&sizes_y,NULL, NULL,NULL,NULL,NULL,NULL,NULL );
  PetscMalloc1( sizes_x , &lxT );
  PetscMalloc1( sizes_y , &lyT );
  PetscMemcpy( lxT , lxc , sizes_x*sizeof(*lxc) );
  PetscMemcpy( lyT , lyc , sizes_y*sizeof(*lyc) );
  //lxT[0] -= 2;
  //lyT[0] -= 2;
  for (int i=0; i<sizes_x ; i++) {
    PetscPrintf( PETSC_COMM_WORLD , "%d , " , (int)lxT[i] );
  }
  PetscPrintf( PETSC_COMM_WORLD , "\n" );
  for (int i=0; i<sizes_y ; i++) {
    PetscPrintf( PETSC_COMM_WORLD , "%d , " , (int)lyT[i] );
  }

  PetscPrintf( PETSC_COMM_WORLD , "nx , ny = %d , %d\n" , (int)nx , (int)ny );
  PetscPrintf( PETSC_COMM_WORLD , "sizes_x , sizes_y = %d , %d\n" , (int)sizes_x , (int)sizes_y );

  // DM for temperature T
  DMDACreate2d(PETSC_COMM_WORLD, 
               DM_BOUNDARY_GHOSTED, DM_BOUNDARY_GHOSTED,    // type of boundary nodes
               DMDA_STENCIL_BOX,                // type of stencil
               nx,ny,                           // global dimns of array
               sizes_x,sizes_y,                 // #procs in each dimn
               1,                               // DOF per node
               2,                               // Stencil width
               lxT,lyT,&da_T);
  // DMDACreate2d(PETSC_COMM_WORLD, 
  //              DM_BOUNDARY_GHOSTED, DM_BOUNDARY_GHOSTED,    // type of boundary nodes
  //              DMDA_STENCIL_BOX,                // type of stencil
  //              nx,ny,                           // global dimns of array
  //              PETSC_DECIDE,PETSC_DECIDE,       // #procs in each dimn
  //              1,                               // DOF per node
  //              2,                               // Stencil width
  //              NULL,NULL,&da_T);
  DMSetFromOptions(da_T);
  DMSetOptionsPrefix(da_T,"T_");
  DMSetUp(da_T);
  
  PetscFree(lxT);
  PetscFree(lyT);

  // Pack the DMs together into a single composite manager
  DMCompositeCreate( PETSC_COMM_WORLD , &pack );
  DMSetOptionsPrefix( pack , "pack_" );
  DMCompositeAddDM( pack , da_c );
  DMCompositeAddDM( pack , da_T );
  DMDASetFieldName( da_c , 0 , "c" );
  DMDASetFieldName( da_T , 0 , "T" );
  DMSetFromOptions( pack );
  
  // Rescale value of L_omega to match the user-specified domain size
  PetscScalar L_domain = sqrtf( user.Lx * user.Ly );
  user.L_omega        *= L_domain;

  user.da_c = da_c;
  user.da_T = da_T;
  user.pack = pack;
  
  /*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Extract global vectors from DMDA;
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  DMCreateGlobalVector( da_c , &c );
  DMCreateGlobalVector( da_T , &T );
  DMCreateGlobalVector( pack , &u );
  VecDuplicate( c , &r_c ); // Residual used for CH-only calculations
  VecDuplicate( T , &r_T ); // Residual used for thermal-only calculations
  VecDuplicate( u , &r );   // Residual used for coupled calculations
  VecDuplicate(c,&user.eps_2);
  VecDuplicate(c,&user.sigma);
  VecDuplicate(c,&user.temperature);
  VecDuplicate(c,&user.X);
  
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  TSCreate(PETSC_COMM_WORLD,&ts);
  TSSetProblemType(ts,TS_NONLINEAR);
  TSSetMaxTime(ts,user.t_final);
  TSSetExactFinalTime(ts,TS_EXACTFINALTIME_STEPOVER);
  dt   = 0.005;
  TSSetTimeStep(ts,dt);

  // Initial solution
  FormInitialSolution( u , &user );
  
  // Thermal CH parameters
  compute_eps2_and_sigma_from_temperature( &user , u );
  
  /*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Set options based on type of physics
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  // Unpack the stuff you need
  DMCompositeGetAccess( pack , u , &U_c , &U_T );
  
  if (user.physics == 0) {
    // CH only
  
    // TS
    TSSetDM( ts , da_c );
    TSSetIFunction( ts , r_c , FormIFunction_CH , &user );
    TSSetSolution( ts , U_c );

    // SNES
    DMSetMatType( da_c , MATAIJ );
    DMCreateMatrix( da_c , &J );
    TSGetSNES( ts , &snes );
    MatCreateSNESMF( snes , &Jmf );
    SNESSetJacobian( snes , Jmf , J , SNESComputeJacobianDefaultColor , 0 );
    
  }
  else if (user.physics == 1) {
    // Thermal only

    // TS
    TSSetDM( ts , da_T );
    TSSetIFunction( ts , r_T , FormIFunction_thermal , &user );
    TSSetSolution( ts , U_T );

    // SNES
    DMSetMatType( da_T , MATAIJ );
    DMCreateMatrix( da_T , &J );
    TSGetSNES( ts , &snes );
    MatCreateSNESMF( snes , &Jmf );
    SNESSetJacobian( snes , Jmf , J , SNESComputeJacobianDefaultColor , 0 );
    
  }
  else if (user.physics == 2) {
    // Coupled CH + thermal

    // TS
    TSSetDM( ts , pack );
    TSSetIFunction( ts , r , FormIFunction_CH_coupled , &user );
    TSSetSolution( ts , u );

    // SNES
    DMSetMatType( pack , MATAIJ );
    DMCreateMatrix( pack , &J );
    TSGetSNES( ts , &snes );
    MatCreateSNESMF( snes , &Jmf );
    SNESSetJacobian( snes , Jmf , J , SNESComputeJacobianDefaultColor , 0 );
    
  }
  
  // User-options
  TSSetFromOptions(ts);
  SNESSetFromOptions(snes);
  SNESGetKSP(snes,&ksp);
  KSPSetFromOptions(ksp);
  PetscOptionsView( NULL , PETSC_VIEWER_STDOUT_WORLD );  
  
  // Setup event handling
  PetscInt       direction[2];
  PetscBool      terminate[2];
  direction[0] = 1; direction[1] = 1;
  terminate[0] = PETSC_FALSE; terminate[1] = PETSC_FALSE;
  TSSetEventHandler( ts , 2 , direction , terminate , EventFunction , PostEventFunction_ResetTemperatureGaussianProfile , (void*)&user );
  
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve nonlinear system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  const std::string initial_soln = "c_" + std::to_string( 0.0 ).substr(0,6) + ".bin";
  PetscPrintf( PETSC_COMM_WORLD , "Logging initial solution at t = 0 seconds\n" );
  log_solution( U_c , initial_soln );

  if (user.physics == 0)
    TSSolve( ts , U_c );
  else if (user.physics == 1)
    TSSolve( ts , U_T);
  else if (user.physics == 2)
    TSSolve( ts , u );

  const std::string final_soln = "c_" + std::to_string( user.t_final ).substr(0,6) + ".bin";
  PetscPrintf( PETSC_COMM_WORLD , "Logging final solution at t = %5.4f seconds\n" , (double)user.t_final );
  log_solution( U_c , final_soln );
  
  PetscPrintf( PETSC_COMM_WORLD , "SIMULATION DONE\n\n" );
  
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  DMCompositeRestoreAccess( pack , u , &U_c , &U_T );
  MatDestroy(&J);
  MatDestroy(&Jmf);
  VecDestroy(&u);
  VecDestroy(&c);
  VecDestroy(&r_c);
  VecDestroy(&r_T);
  VecDestroy(&r);
  VecDestroy(&user.eps_2);
  VecDestroy(&user.sigma);
  VecDestroy(&user.temperature);
  VecDestroy(&user.X);
  TSDestroy(&ts);
  DMDestroy(&da_c);
  DMDestroy(&da_T);

  PetscFinalize();
  return ierr;
}
