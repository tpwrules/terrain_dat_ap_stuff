from terrain_gen import *

import numpy as np
import matplotlib.pyplot as plt

from pyproj import CRS, Transformer

def create_degree_proj(lat, lon, grid_spacing, format):
    '''create data file for one degree lat/lon'''
    lat_int = int(math.floor(lat))
    lon_int = int(math.floor((lon)))

    latlon_e7_vals = []
    meter_vals = []

    blocknum = -1

    while True:
        blocknum += 1
        (lat_e7, lon_e7) = pos_from_file_offset(lat_int, lon_int, blocknum * IO_BLOCK_SIZE, grid_spacing, format)
        if lat_e7*1.0e-7 - lat_int >= 1.0:
            break
        lat = lat_e7 * 1.0e-7
        lon = lon_e7 * 1.0e-7
        grid = GridBlock(lat_int, lon_int, lat, lon, grid_spacing, format)
        if grid.blocknum() != blocknum:
            print("what??")
            continue
        for gx in range(TERRAIN_GRID_BLOCK_SIZE_X):
            for gy in range(TERRAIN_GRID_BLOCK_SIZE_Y):
                #lat_e7, lon_e7 = add_offset(lat*1.0e7, lon*1.0e7, gx*grid_spacing, gy*grid_spacing, format)
                
                # this is a lot closer to what AP does in calculate_grid_info
                mx = (grid.grid_idx_x*TERRAIN_GRID_BLOCK_SPACING_X+gx)*grid_spacing
                my = (grid.grid_idx_y*TERRAIN_GRID_BLOCK_SPACING_Y+gy)*grid_spacing

                lat2, lon2 = add_offset(lat_int*1e7, lon_int*1e7, mx, my, format)

                #error_vals.append((abs(lat_e7-lat2), abs(lon_e7-lon2)))

                latlon_e7_vals.append((lat2, lon2))
                meter_vals.append((mx, my))

    return latlon_e7_vals, meter_vals

# https://stackoverflow.com/a/57923405
def polyfit2d(x, y, z, kx=3, ky=3, order=None):
    '''
    Two dimensional polynomial fitting by least squares.
    Fits the functional form f(x,y) = z.

    Notes
    -----
    Resultant fit can be plotted with:
    np.polynomial.polynomial.polygrid2d(x, y, soln.reshape((kx+1, ky+1)))

    Parameters
    ----------
    x, y: array-like, 1d
        x and y coordinates.
    z: np.ndarray, 2d
        Surface to fit.
    kx, ky: int, default is 3
        Polynomial order in x and y, respectively.
    order: int or None, default is None
        If None, all coefficients up to maxiumum kx, ky, ie. up to and including x^kx*y^ky, are considered.
        If int, coefficients up to a maximum of kx+ky <= order are considered.

    Returns
    -------
    Return paramters from np.linalg.lstsq.

    soln: np.ndarray
        Array of polynomial coefficients.
    residuals: np.ndarray
    rank: int
    s: np.ndarray

    '''

    # # grid coords
    # x, y = np.meshgrid(x, y)
    # coefficient array, up to x^kx, y^ky
    coeffs = np.ones((kx+1, ky+1))

    # solve array
    a = np.zeros((coeffs.size, x.size))

    # for each coefficient produce array x^i, y^j
    for index, (j, i) in enumerate(np.ndindex(coeffs.shape)):
        # do not include powers greater than order
        if order is not None and i + j > order:
            arr = np.zeros_like(x)
        else:
            arr = coeffs[i, j] * x**i * y**j
        a[index] = arr.ravel()

    # do leastsq fitting and return leastsq result
    return np.linalg.lstsq(a.T, np.ravel(z), rcond=None)

lonv = -90

lle7, m = create_degree_proj(85, lonv, 100, "")

lle7 = np.asarray(lle7)
m = np.asarray(m)

ca = CRS.from_epsg(4326)
cb = CRS.from_proj4(f"+proj=sinu +R=6378100 +lon_0={lonv+0.5}")
t = Transformer.from_crs(ca, cb)

vx, vy = t.transform(lle7[:, 0]/1e7, lle7[:, 1]/1e7)

vxy = np.stack((vx, vy), axis=-1)
print(vxy.shape)

# plt.scatter(vx, vy, s=1)
# plt.show()

# breakpoint()

sk = 4

apy = m[:, 0][::sk] - m[0, 0]
apx = m[:, 1][::sk] - m[0, 1]

vx = vxy[::sk, 0] - vxy[0, 0]
vy = vxy[::sk, 1] - vxy[0, 1]

# plt.scatter(apx, apy, s=1)
# plt.figure()
# plt.scatter(vx, vy, s=1)
# plt.figure()
# plt.show()

# print(vx.shape, m[:, 1][::sk].shape, vx[::sk].T.shape)

kx = 1
ky = 1

x, residuals, rank, s = polyfit2d(apx, apy, vx, kx=kx, ky=ky)

print(residuals, rank, s)

y, residuals, rank, s = polyfit2d(apx, apy, vy, kx=kx, ky=ky)

print(x)
print(y)

fx = np.polynomial.polynomial.polyval2d(apy, apx, x.reshape(ky+1, kx+1))
fy = np.polynomial.polynomial.polyval2d(apy, apx, y.reshape(ky+1, kx+1))
print(fy)

# plt.scatter(apx, vx)
# plt.figure()
# plt.scatter(apy, vy)
# plt.figure()

plt.scatter(vx, vy, s=5)
plt.scatter(fx, fy, s=5)

plt.show()
