### build proj 

#### just do it (without making source mods easy)
1. `nix build .#proj -L`

#### setup

1. `nix develop .#proj`
2. cd outside of repo and make temporary dir (or reuse existing one)
3. `unpackPhase` (if new dir)
4. `cd source`
5. `patchPhase`
6. `ln -sf /path/to/repo/proj/ardusinu.cpp src/projections/ardusinu.cpp`

#### build

1. `cmakeConfigurePhase`
2. `buildPhase`
3. `bin/proj +proj=ardusinu +R=6378100 -r -s +lat_0=42 +lon_0=35`

### build gdal

#### just do it (without making source mods easy)
1. `nix build .#gdal -L`

#### setup

1. `nix develop .#gdal`
2. cd outside of repo and make temporary dir (or reuse existing one)
3. `unpackPhase` (if new dir)
4. `cd source`
5. `patchPhase`
6. `ln -sf /path/to/repo/gdal/apdatdataset.cpp frmts/apdat/apdatdataset.cpp`

#### build

1. `cmakeConfigurePhase`
2. `buildPhase`
3. `LD_LIBRARY_PATH=.:"$LD_LIBRARY_PATH" apps/gdalinfo --formats`

### do things

1. build GTI from .dat zip file: `gdaltindex na_dat_z.gti.gpkg -t_srs epsg:4326 /vsizip/North_America.zip/` (ignore file selection errors)
2. build overview for fast viewing: `gdaladdo -ro --config GDAL_NUM_THREADS=6 --config GDAL_CACHEMAX=32768 --config COMPRESS_OVERVIEW=ZSTD --config ZSTD_LEVEL_OVERVIEW=1 na_dat_z.gti.gpkg 4 16 64 256 1024` (takes several minutes)
