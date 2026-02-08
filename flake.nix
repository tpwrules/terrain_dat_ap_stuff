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

    pkgs = nixpkgs.legacyPackages."${system}";

    # or, if you need to add an overlay:
    # pkgs = import nixpkgs {
    #   inherit system;
    #   overlays = [
    #     (import ./nix/overlay.nix)
    #   ];
    # };

    proj = pkgs.proj.overrideAttrs (old: {
      patches = (old.patches or []) ++ [
        ./proj/add_ardusinu.patch
      ];

      postPatch = (old.postPatch or "") + ''
        # cat to avoid migrating bad permissions
        cat ${./proj/ardusinu.cpp} > src/projections/ardusinu.cpp
      '';

      doCheck = false;
    });

    # a text file containing the paths to the flake inputs in order to stop
    # them from being garbage collected
    pleaseKeepMyInputs = pkgs.writeTextDir "bin/.please-keep-my-inputs"
      (builtins.concatStringsSep " " (builtins.attrValues inputs));
  in {
    devShell."${system}" = let
      shellInputs = pkgs.symlinkJoin { # needed for replaceDependencies
        name = "shellInputs";
        paths = [
          # needed for version info and such
          pkgs.git

          # used for SITL (console and map modules must be manually loaded)
          pkgs.mavproxy

          pkgs.gdal
          pkgs.proj
          pkgs.qgis

          (pkgs.python3.withPackages (p: [
            p.numpy
            p.matplotlib
            p.pyproj
            p.fastcrc
          ]))

          pleaseKeepMyInputs
        ];
      };

      newShellInputs = pkgs.replaceDependencies {
        drv = shellInputs;
        replacements = [{
          oldDependency = pkgs.proj;
          newDependency = proj;
        }];
      };
    in pkgs.mkShellNoCC {
        buildInputs = [ newShellInputs ];

        shellHook = ''
          export EDITOR=nano
        '';
    };

    packages."${system}" = {
      proj = proj;
    };
  };
}
