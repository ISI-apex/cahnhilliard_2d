import numpy as np
import matplotlib.pyplot as plt
import sys,os
import h5py

class PetscSettings:

    def __init__(self):
        self.tf          = None
        self.dt          = None
        self.T0_filename = None
        self.nx          = None
        self.ny          = None

def parse_inputs_from_petscfile( petscfile ):
    
    settings = PetscSettings()
    
    settings.T0_filename = 'initial_temperature.dat'
    with open( petscfile ) as f:
        petscsettings = f.readlines()
        for line in petscsettings:
            try:
                k,v = line.split(' ')
            except:
                k = None; v = None
            if   k == '-t_final':
                settings.tf = float(v.split('\n')[0])
            elif k == '-dt_check':
                settings.dt = float(v.split('\n')[0])
            elif k == '-initial_temperature_file':
                settings.T0_filename = v.split('\n')[0]
            elif k == '-da_grid_x':
                settings.nx = int(v.split('\n')[0])
            elif k == '-da_grid_y':
                settings.ny = int(v.split('\n')[0])
            
    return settings

def write_temperature_laser_parameters_for_petsc_solver( T_amp , T_x , T_y , T_sigma , filename ):

    outlist = [ T_amp , T_x , T_y , T_sigma ]
    np.savetxt( filename , outlist , fmt='%.4f' )

    return

def generate_quarter_circle_laser_path( amp , nx , ny , num_changes ):

    T_amp       = amp * np.ones( num_changes )
    th          = np.linspace( 0.75 * np.pi , 1.75*np.pi , num_changes )
    T_x         = nx * ( 1 + 0.75 * np.cos( th ) )
    T_y         = ny * ( 1 + 0.75 * np.sin( th ) )
    sigma_temp  = 128./2.
    T_sigma     = sigma_temp * np.ones( num_changes )
    
    return T_amp , T_x , T_y , T_sigma

def generate_const_global_temperature( amp , nx , ny , num_changes ):

    T_amp   = amp   * np.ones( num_changes )
    T_x     = nx//2 * np.ones( num_changes )
    T_y     = ny//2 * np.ones( num_changes )
    T_sigma = nx*ny * np.ones( num_changes )
    
    return T_amp , T_x , T_y , T_sigma

def main():

    settings = parse_inputs_from_petscfile( 'petscrc.dat' )

    # Set temporal profile for parameter values
    num_changes                 = int( settings.tf / settings.dt )
    T_amp , T_x , T_y , T_sigma = generate_quarter_circle_laser_path( 1.0 , settings.nx , settings.ny , num_changes )
    #T_amp , T_x , T_y , T_sigma = generate_const_global_temperature( 0.5 , settings.nx , settings.ny , num_changes )

    # Write initial temperature field to disk for petsc
    initial_T      = 1.0 * np.ones( settings.nx * settings.ny )
    initial_T_file = 'initial_temperature.ascii'
    np.savetxt( initial_T_file , initial_T , fmt='%1.8f' )
    os.system( './preprocess petscrc.dat ' + initial_T_file )
    
    # Write the initial solution field to disk for petsc
    initial_U      = 0.005 * ( 2.0 * np.random.uniform(0,1,settings.nx*settings.ny) - 1.0 )
    initial_U_file = 'initial_soln.ascii'
    np.savetxt( initial_U_file , initial_U , fmt='%1.8f' )
    os.system( './preprocess petscrc.dat ' + initial_U_file )

    # Run solver
    count = 0
    os.system( './run_jfnk.sh &' )

    # Check filesystem for indication from solver that it is waiting for next m value
    while True:
        timestamp        = '_{:0.4f}.bin'.format( (count+1) * settings.dt )
        timestamp_final  = '_{:0.4f}.bin'.format( settings.tf )
        petsc_done = os.path.exists( 'complete' + timestamp )
        sim_done   = os.path.exists( 'c' + timestamp_final )
        if sim_done:
            print("DRIVER PROGRAM DONE")
            break
        else:
            if petsc_done:
                write_temperature_laser_parameters_for_petsc_solver( T_amp[count] ,
                                                                     T_x[count] ,
                                                                     T_y[count] ,
                                                                     T_sigma[count],
                                                                     'T' + timestamp )
                count += 1
            
if __name__ == '__main__':
    main()
