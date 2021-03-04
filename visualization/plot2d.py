import numpy as np
import matplotlib.pyplot as plt
from matplotlib import animation

t0 = 0.00
tf = 1.00
calc_tsteps = 2

nx = 1024
ny = 1024

statefile = 'cpp/build/C_'

x     = np.arange(nx)
y     = np.arange(ny)
xx,yy = np.meshgrid(x,y)
xx = xx.T; yy = yy.T

tstep = 0.02
fig   = plt.figure(10,figsize=(8,8))
ax    = fig.gca()

for ts in range(calc_tsteps):
    plot_name = statefile + str(ts) + '.pdf'
    print("T=", ts*tstep + t0, ":", plot_name)

    w = np.genfromtxt(statefile + str(ts) + '.out' )
    w = w.reshape([nx, ny], order='C');

    ax.cla()
    contour = ax.contourf(xx, yy, w, 30, vmin=-0.4, vmax=0.4)
    ax.set_aspect('equal')
    fig.savefig(plot_name)
