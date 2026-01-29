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

#### build

1. `cmakeConfigurePhase`
2. `buildPhase`
3. `LD_LIBRARY_PATH=.:"$LD_LIBRARY_PATH" apps/gdalinfo --formats`
