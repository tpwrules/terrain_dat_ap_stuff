import sys
import struct

import numpy as np
from osgeo import gdal
gdal.UseExceptions()

from terrain_gen import *

def load_raster(path):
    blocks = np.frombuffer(open(sys.argv[1], "rb").read(), dtype=np.uint8)
    blocks = blocks.reshape(-1, 2048) # each block is 2048 bytes

    # take first block as gospel
    spacing, = struct.unpack("<H", blocks[0, 20:22])
    lon_deg, lat_deg = struct.unpack("<hb", blocks[0, 22+1792+4:22+1792+4+3])

    # blocks are 1792 bytes of data, headers are 22 bytes
    blocks = blocks[:, 22:22+1792].view(np.int16)

    # east first in the files
    eb = east_blocks(lat_deg*1e7, lon_deg*1e7, spacing, "4.1")
    blocks = blocks.reshape(-1, eb, 28, 32)

    # get rid of overlap
    blocks = blocks[..., :-4, :-4]

    # transpose to put easts first
    blocks = blocks.transpose(0, 2, 1, 3)
    # then de-tile
    raster = blocks.reshape(-1, blocks.shape[2]*blocks.shape[3])

    return raster, spacing, lat_deg, lon_deg

def write_tif(path, raster, spacing, lat_deg, lon_deg):
    ny, nx = raster.shape
    gtiff = gdal.GetDriverByName("GTiff")
    with gtiff.Create(path, nx, ny, bands=1, eType=gdal.GDT_Int16) as ds:
        # no rotation or translation, uniform spacing in X and Y, Y increases up
        # WARNING: this is possibly off by half a pixel
        ds.SetGeoTransform((0, spacing, 0, 0, 0, spacing))
        # hardcoded earth radius (from LOCATION_SCALING_FACTOR) and custom
        # projection, starting with 0, 0 at this tile
        pj = f"+proj=ardusinu +R=6378100 +lat_0={lat_deg} +lon_0={lon_deg}"
        ds.SetProjection(pj)
        # write actual file data
        ds.WriteArray(raster)

if __name__ == "__main__":
    write_tif(sys.argv[2], *load_raster(sys.argv[1]))
