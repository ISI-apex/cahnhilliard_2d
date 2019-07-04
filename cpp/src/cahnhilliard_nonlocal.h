#ifndef __CAHNHILLIARD_NONLOCAL_H__
#define __CAHNHILLIARD_NONLOCAL_H__

#include <vector>
#include "chparams.h"

void compute_ch_nonlocal(const std::vector<double> &c,
			 std::vector<double> &dcdt,
			 const double t,
			 CHparamsVector& chpV,
			 SimInfo& info);

void compute_ch_nonlocal_stationary_boundaries(const std::vector<double> &c,
					       std::vector<double> &dcdt,
					       const double t,
					       CHparamsVector& chpV,
					       SimInfo& info);

std::vector<double>& set_boundary_values_to_zero( std::vector<double> &dcdt ,
						  SimInfo& info );

std::vector<double>& apply_dirichlet_bc( std::vector<double>& c ,
					 SimInfo& info );

std::vector<double>& apply_neumann_bc( std::vector<double>& c ,
				       SimInfo& info );

double laplace_component(int i ,
                         const std::vector<double>& c ,
                         const std::vector<double>& u ,
                         const std::vector<double>& b );

CHparamsVector compute_chparams_using_temperature( CHparamsVector& chpV0 ,
						   SimInfo& info,
						   std::vector<double> T );


#endif