from PIL import Image
import sys
import numpy as np

from terrain_gen import *

lat, lon = 35, -90

GRID_SPACING = 100

def load_raster(path):
    blocks = np.frombuffer(open(sys.argv[1], "rb").read(), dtype=np.uint8)
    blocks = blocks.reshape(-1, 2048) # each block is 2048 bytes
    # blocks are 1792 bytes of data, headers are 22 bytes
    blocks = blocks[:, 22:22+1792].view(np.uint16)
    # east first in the files
    blocks = blocks.reshape(-1, 28, 32)

    eb = east_blocks(lat*1e7, lon*1e7, GRID_SPACING, "")

    blocks = blocks.reshape(-1, eb, 28, 32)
    # get rid of overlap
    blocks = blocks[..., :-4, :-4]

    # transpose to put easts first
    blocks = blocks.transpose(0, 2, 1, 3)
    # then de-tile
    blocks = blocks.reshape(-1, blocks.shape[2]*blocks.shape[3])

    return blocks

raster = load_raster(sys.argv[1])
im = Image.fromarray(raster)
im.save(sys.argv[2])
