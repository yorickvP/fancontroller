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
        inputs = [ nixpkgs.kicad ];
      };
      packages.${system} = {
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
        default = self.packages.${system}.fw;
      };
    };
}
