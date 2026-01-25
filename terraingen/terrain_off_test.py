from terrain_gen import add_offset, get_distance_NE_e7

fmt = "" # it's not pre-4.1

import random
import math

import numpy as np

r = random.Random(69420)

# // grids start on integer degrees. This makes storing terrain data
# // on the SD card a bit easier
# info.lat_degrees = (loc.lat<0?(loc.lat-9999999L):loc.lat) / (10*1000*1000L);
# info.lon_degrees = (loc.lng<0?(loc.lng-9999999L):loc.lng) / (10*1000*1000L);

# // create reference position for this rounded degree position
# Location ref;
# ref.lat = info.lat_degrees*10*1000*1000L;
# ref.lng = info.lon_degrees*10*1000*1000L;

# // find offset from reference
# const Vector2f offset = ref.get_distance_NE(loc);

# // get indices in terms of grid_spacing elements
# uint32_t idx_x = offset.x / grid_spacing;
# uint32_t idx_y = offset.y / grid_spacing;

# // find indexes into 32*28 grids for this degree reference. Note
# // the use of TERRAIN_GRID_BLOCK_SPACING_{X,Y} which gives a one square
# // overlap between grids
# info.grid_idx_x = idx_x / TERRAIN_GRID_BLOCK_SPACING_X;
# info.grid_idx_y = idx_y / TERRAIN_GRID_BLOCK_SPACING_Y;

# // find the indices within the 32*28 grid
# info.idx_x = idx_x % TERRAIN_GRID_BLOCK_SPACING_X;
# info.idx_y = idx_y % TERRAIN_GRID_BLOCK_SPACING_Y;

# // find the fraction (0..1) within the square
# info.frac_x = (offset.x - idx_x * grid_spacing) / grid_spacing;
# info.frac_y = (offset.y - idx_y * grid_spacing) / grid_spacing;

# // calculate lat/lon of SW corner of 32*28 grid_block
# ref.offset(info.grid_idx_x * TERRAIN_GRID_BLOCK_SPACING_X * (float)grid_spacing,
#            info.grid_idx_y * TERRAIN_GRID_BLOCK_SPACING_Y * (float)grid_spacing);
# info.grid_lat = ref.lat;
# info.grid_lon = ref.lng;

# ASSERT_RANGE(info.idx_x,0,TERRAIN_GRID_BLOCK_SPACING_X-1);
# ASSERT_RANGE(info.idx_y,0,TERRAIN_GRID_BLOCK_SPACING_Y-1);
# ASSERT_RANGE(info.frac_x,0,1);
# ASSERT_RANGE(info.frac_y,0,1);

errors = []
for _ in range(100_000):
    lat_1e7 = r.randrange(int(-90e7), int(90e7))
    lon_1e7 = r.randrange(int(-180e7), int(180e7))

    # compute reference as the degree value as floor
    lat_deg = lat_1e7//1e7
    lon_deg = lon_1e7//1e7

    # get distance from ref pos to our actual pos, in meters
    north_m, east_m = get_distance_NE_e7(
        lat_deg*1e7, lon_deg*1e7, lat_1e7, lon_1e7, fmt)

    # in an actual application we'd truncate to the grid size but this is just
    # about the distances

    # convert it back to the original lat and lon as if it were the grid ref
    # corner
    lat_again_1e7, lon_again_1e7 = add_offset(lat_deg*1e7, lon_deg*1e7,
        north_m, east_m, fmt)

    errors.append((abs(lat_1e7-lat_again_1e7), abs(lon_1e7-lon_again_1e7)))

errors = np.asarray(errors)
print(errors.max(axis=0))
