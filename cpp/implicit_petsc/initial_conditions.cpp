#include "initial_conditions.h"
#include "temperature_dependence.h"

PetscErrorCode FormInitialSolution(Vec U , Vec Temperature , void *ptr)
{
  AppCtx         *user=(AppCtx*)ptr;
  DM             da   =user->da;
  PetscErrorCode ierr;
  PetscInt       i,j,xs,ys,xm,ym,Mx,My;
  PetscScalar    **u , **T;
  PetscReal      hx,hy,x,y,r;
  PetscRandom rng;
  PetscReal value_rng;

  PetscFunctionBeginUser;
  DMDAGetInfo(da,PETSC_IGNORE,&Mx,&My,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);

  hx = (user->Lx)/(PetscReal)(Mx-1);
  hy = (user->Ly)/(PetscReal)(My-1);

  /* Get pointers to vector data */
  DMDAVecGetArray(da,U,&u);
  DMDAVecGetArray(da,Temperature,&T);

  /* Get local grid boundaries */
  DMDAGetCorners(da,&xs,&ys,NULL,&xm,&ym,NULL);

  /* Compute function over the locally owned part of the grid */
  PetscRandomCreate(PETSC_COMM_WORLD,&rng);
  PetscRandomSetType(rng,PETSCRAND48);

  // Interior
  for (j=ys; j<ys+ym; j++) {
    for (i=xs; i<xs+xm; i++) {
      PetscRandomGetValueReal(rng , &value_rng);
      u[j][i]     = 0.005 * ( 2.0 * value_rng - 1.0 );
      T[j][i]     = user->T_max * PetscExpReal( -0.5 * ( (j-32)*(j-32) + (i-32)*(i-32) ) / (32.0*32.0) );
      //eps_2[j][i] = 0.00013331972927932523 * PetscExpReal( -0.5 * ( (j-32)*(j-32) + (i-32)*(i-32) ) / (32.0*32.0) );
    } 
  }

  // Boundary
  PetscInt J, I;
  for (j=ys; j<ys+ym; j++) {
    for (i=xs; i<xs+xm; i++) {

      if ( (i <= 1) || (i >= Mx-2) || (j <= 1) || (j >= My-2) ) {
        
        if ( user->boundary == 0 ) {
          // Dirichlet
          u[j][i] = user->dirichlet_bc;
        }
        
        else if ( user->boundary == 1 ) {
          // Neumann: just fill ghost cells with random values, and explicitly change residual at the end
          PetscRandomGetValueReal(rng , &value_rng);
          u[j][i] = 0.005 * ( 2.0 * value_rng - 1.0 );
        }

        else if ( user->boundary == 2 ) {
          // Periodic
          I = i; J = j;
          if (i <= 1)
            I = (i-2) + Mx;
          else if (i >= Mx-2)
            I = i % (Mx-2);
          if (j <= 1)
            J = (j-2) + My;
          else if (j >= My-2)
            J = j % (My-2);
          u[j][i] = u[ J ][ I ];
        }
        
      }
        
    }
  }
  

  PetscRandomDestroy(&rng);

  /* Restore vectors */
  DMDAVecRestoreArray(da,U,&u);
  DMDAVecRestoreArray(da,Temperature,&T);

  // Compute temperature-dependent polymer limiters
  user->eps2_min = compute_eps2_from_chparams( user->X_max ,
                                               user->L_kuhn ,
                                               user->m ,
                                               user->L_omega );

  user->eps2_max = compute_eps2_from_chparams( user->X_min ,
                                               user->L_kuhn ,
                                               user->m ,
                                               user->L_omega );

  user->sigma_min = compute_sigma_from_chparams( user->X_max ,
                                                 user->L_kuhn ,
                                                 user->m ,
                                                 user->L_omega ,
                                                 user->N );

  user->sigma_max = compute_sigma_from_chparams( user->X_min ,
                                                 user->L_kuhn ,
                                                 user->m ,
                                                 user->L_omega ,
                                                 user->N );

  //PetscPrintf( PETSC_COMM_WORLD , "%5.2f\n" , (double)user->sigma_max );

  
  PetscFunctionReturn(0);
}
