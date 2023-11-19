{
  description = "Fan controller";
  inputs = {
    esp-idf = {
      url = "https://github.com/espressif/esp-idf/releases/download/v5.1.1/esp-idf-v5.1.1.zip";
      flake = false;
    };
    dream2nix.url = "github:nix-community/dream2nix";
    nixpkgs.follows = "dream2nix/nixpkgs";
  };

  outputs = { self, ... }@inputs:
    let
      system = "x86_64-linux";
      nixpkgs = inputs.nixpkgs.legacyPackages.${system};
    in {
      devShells.${system}.kicad = nixpkgs.mkShell {
        packages = [ nixpkgs.kicad ];
      };
      packages.${system} = {
        kicad-unstable = nixpkgs.callPackage ./nix/kicad-unstable.nix {};
        tovrmlx3d = nixpkgs.callPackage ./nix/tovrmlx3d.nix {};
        esp-idf = inputs.dream2nix.lib.evalModules {
          packageSets = { inherit nixpkgs; };
          modules = [
            ./nix/esp-idf/module.nix
            {
              version = "5.1.1";
              paths = {
                projectRoot = ./.;
                projectRootFile = "flake.nix";
                package = ./nix/esp-idf;
              };
              deps = { inherit (inputs) esp-idf; };
            }
          ];
        };
        fw = nixpkgs.callPackage ./nix/fw.nix {
          shortRev = self.shortRev or "dirty";
          esp-idf = self.packages.${system}.esp-idf;
        };
        hw-1 = nixpkgs.runCommand "fancontroller-hw-1" {
          nativeBuildInputs = with nixpkgs; [ kicad-small zip ];
        } ''
          mkdir $out
          export HOME=/tmp
          kicad-cli sch export pdf ${./hw/1}/fancontroller.kicad_sch
          mkdir grb
          kicad-cli pcb export gerbers ${./hw/1}/fancontroller.kicad_pcb -o grb/
          kicad-cli pcb export drill ${./hw/1}/fancontroller.kicad_pcb -o grb/
          zip fancontroller.zip grb/*
          mv fancontroller.zip fancontroller.pdf $out/
        '';
        hw-1-3d = nixpkgs.runCommand "fancontroller-3d-1" {
          nativeBuildInputs = with self.packages.${system}; [ kicad-unstable tovrmlx3d nixpkgs.which ];
        } ''
          mkdir -p $out/shapes3D
          export HOME=/tmp
          source <(grep "export KICAD7" $(which kicad))
          kicad-cli pcb export vrml ${./hw/1}/fancontroller.kicad_pcb -o fancontroller.wrl --units in --models-dir shapes3D/ --models-relative
          tovrmlx3d --encoding xml --no-x3d-extensions fancontroller.wrl > fancontroller.x3d
          for f in shapes3D/*.wrl; do
            tovrmlx3d --encoding xml --no-x3d-extensions "$f" > "''${f%.wrl}.x3d"
          done
          sed -i 's/\.wrl/\.x3d/g' fancontroller.x3d
          mv fancontroller.x3d $out
          cp ${./nix/render-3d.html} $out/index.html
          mv shapes3D/*.x3d $out/shapes3D
        '';
        default = self.packages.${system}.fw;
      };
    };
}
