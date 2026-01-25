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

lonv = -90

lle7, m = create_degree_proj(85, lonv, 100, "")

lle7 = np.asarray(lle7)
m = np.asarray(m)

ca = CRS.from_epsg(4326)
cb = CRS.from_proj4(f"+proj=sinu +R=6378100 +lon_0={lonv+0.5}")
t = Transformer.from_crs(ca, cb)

vx, vy = t.transform(lle7[:, 0]/1e7, lle7[:, 1]/1e7)

plt.scatter(vx, vy, s=1)
plt.show()
