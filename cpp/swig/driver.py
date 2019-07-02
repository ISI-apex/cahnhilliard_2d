
import numpy as np
import matplotlib.pyplot as plt
from scipy import misc
import cahnhilliard as ch

def generate_gaussian_field(xx,yy,A,xy0,sigma):
    return A * np.exp( -0.5 * ((xx-xy0[0])**2 + (yy-xy0[1])**2) / sigma**2 )

def generate_square_field(xx,yy,A,xy0,sigma):
    zz    = np.zeros_like(xx)
    zz[ (xx > xy0[0]-sigma/2) & (xx < xy0[0]+sigma/2) & (yy > xy0[1]-sigma/2) & (yy < xy0[1]+sigma/2) ] = A
    return zz
    
def convert_temperature_to_ch_coeffs(tt,eps_range,sigma_range,temp_range):
    deps2_dtemp  = (eps_range[1]   - eps_range[0])   / (temp_range[1] - temp_range[0])
    dsigma_dtemp = (sigma_range[1] - sigma_range[0]) / (temp_range[1] - temp_range[0])
    eps2         = ch.DoubleVector( deps2_dtemp  * (tt.ravel() - temp_range[0]) + eps_range[0]    )
    sigma        = ch.DoubleVector( dsigma_dtemp * (tt.ravel() - temp_range[0]) + sigma_range[0]  )
    return eps2 , sigma

chparams          = ch.CHparamsVector();
info              = ch.SimInfo();

# *********** INPUTS ***********
info.t0       = 0.0;
info.nx       = 128;
info.dx       = 1./info.nx;

nx_ref        = 128;
dx_ref        = 1./nx_ref

eps_2        = 0.05**2
sigma        = eps_2 / dx_ref**4 / 200.
b            = eps_2 / dx_ref**2
u            = eps_2 / dx_ref**2
m            = 0.0;
sigma_noise  = 0.0;
DT           = 0.1*eps_2 / dx_ref**2

# Set up grid for spatial-field quantities
nx                = int(info.nx)
xx,yy             = np.meshgrid( np.arange(0,1,1/info.nx), np.arange(0,1,1/info.nx) )

chparams.eps_2        = ch.DoubleVector(eps_2  * np.ones(nx**2))
chparams.b            = ch.DoubleVector(b      * np.ones(nx**2))
chparams.u            = ch.DoubleVector(u      * np.ones(nx**2))
chparams.sigma        = ch.DoubleVector(sigma  * np.ones(nx**2))
chparams.m            = ch.DoubleVector(m      * np.ones(nx**2))
chparams.DT           = ch.DoubleVector(DT      * np.ones(nx**2))
chparams.sigma_noise  = sigma_noise
chparams.temperature_dependence = True
chparams.eps2_min     = 1./10*eps_2
chparams.eps2_max     =     1*eps_2
chparams.sigma_min    = 1./10*sigma
chparams.sigma_max    =    1*sigma
chparams.T_min        = 0.0
chparams.T_max        = 1.0

n_dt = 1500
# ******************************

# Define timescales
biharm_dt         = (info.dx**4) / np.max(chparams.eps_2)
diff_dt           = (info.dx**2) / np.max( [np.max(chparams.u) , np.max(chparams.b)] )
lin_dt            = 1.0 / np.max(chparams.sigma)
biharm_dt_ref     = (dx_ref**4) / np.max(chparams.eps_2)
diff_dt_ref       = (dx_ref**2) / np.max( [np.max(chparams.u) , np.max(chparams.b)] )
lin_dt_ref        = 1.0 / np.max(chparams.sigma)

n_tsteps          = 150
t                 = np.linspace(0 , n_dt * biharm_dt_ref , n_tsteps+1)
chparams.dt_check = t[1]-t[0]

# Define control profile for temperature
A               = 100./dx_ref * np.ones(n_tsteps)
A[25:50]        = -100./dx_ref
A[50:]          = 0
xy0             = np.vstack( [0.5 + 0*np.linspace(0,2.0,n_tsteps) , 0.5*np.ones(n_tsteps)] ).T
sigma_temp   = nx/10 * info.dx
#eps_range    = [1./10*eps_2 , 1*eps_2]
#sigma_range  = [1./10*sigma , 1*sigma]
#temp_range   = [0 , np.max(A)]

# Run solver
print( 'Biharmonic timescale dt_biharm = ' , biharm_dt )
print( 'Diffusion timescale dt_diff = ' , diff_dt , ' = ' , diff_dt/biharm_dt , ' dt_biharm')
print( 'Linear timescale dt_lin = ' , lin_dt , ' = ' , lin_dt/biharm_dt , ' dt_biharm')
print( 'Sampling interval = ' , chparams.dt_check / biharm_dt , ' dt_biharm' )

for i in range(n_tsteps):
    info.t0        = t[i]
    info.tf        = t[i+1]
    tt             = generate_square_field(xx,yy,A[i],xy0[i],sigma_temp)
    chparams.f_T   = ch.DoubleVector( tt.ravel() )
    rhs            = ch.CahnHilliard2DRHS_thermal(chparams , info)
    print( 't0 = ', t[i]/biharm_dt, ' dt_biharm , tf = ', t[i+1]/biharm_dt, ' dt_biharm')
    ch.run_ch_vector_thermal(chparams,info,rhs);
