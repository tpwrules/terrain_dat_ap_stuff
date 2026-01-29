# HOW TO USE:
# 1. make a `profiles/` directory in your repo and add to .gitignore
# 2. nix develop --profile profiles/dev
# 3. done!
#
# the shell is completely safe from garbage collection and evaluates instantly
# due to Nix's native caching. if you want logs during build, add `-L` to 
# `nix develop`.
{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs }: let
    inputs = { inherit nixpkgs; };
    system = "x86_64-linux";

    # or, if you need to add an overlay:
    pkgs = import nixpkgs {
      inherit system;
      overlays = [
        (final: prev: {
          proj = prev.proj.overrideAttrs (old: {
            patches = (old.patches or []) ++ [
              ./proj/add_ardusinu.patch
            ];

            postPatch = (old.postPatch or "") + ''
              # cat to avoid migrating bad permissions
              cat ${./proj/ardusinu.cpp} > src/projections/ardusinu.cpp
            '';

            doCheck = false;
          });

          gdal = (prev.gdal.override {
            useMinimalFeatures = true; # less building and dependency fetching
          }).overrideAttrs (old: {
            doInstallCheck = false;
          });
        })
      ];
    };

    # a text file containing the paths to the flake inputs in order to stop
    # them from being garbage collected
    pleaseKeepMyInputs = pkgs.writeTextDir "bin/.please-keep-my-inputs"
      (builtins.concatStringsSep " " (builtins.attrValues inputs));
  in {
    devShell."${system}" = pkgs.mkShellNoCC {
      buildInputs = [
        # needed for version info and such
        pkgs.git

        # used for SITL (console and map modules must be manually loaded)
        pkgs.mavproxy

        pkgs.gdal
        pkgs.proj

        (pkgs.python3.withPackages (p: [
          p.numpy
          p.matplotlib
          p.pyproj
          p.fastcrc
        ]))

        pleaseKeepMyInputs
      ];
    };

    packages."${system}" = {
      inherit (pkgs) proj;
    };

    shellHook = ''
      export EDITOR=nano
    '';
  };
}
