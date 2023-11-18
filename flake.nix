{
  description = "A very basic flake";
  inputs = {
    esp-idf = {
      url = "https://github.com/espressif/esp-idf/releases/download/v5.1.1/esp-idf-v5.1.1.zip";
      flake = false;
    };
    dream2nix = { url = "github:nix-community/dream2nix"; };
    nixpkgs.follows = "dream2nix/nixpkgs";
  };

  outputs = { self, nixpkgs, esp-idf, dream2nix }@inputs: let
    esp-idf = self.packages.x86_64-linux.esp-idf;
    in {
    packages.x86_64-linux = {
      esp-idf = inputs.dream2nix.lib.evalModules {
        packageSets.nixpkgs =
          inputs.dream2nix.inputs.nixpkgs.legacyPackages.x86_64-linux;
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
      fw = nixpkgs.legacyPackages.x86_64-linux.callPackage ./nix/fw.nix {
        shortRev = self.shortRev or "dirty";
        esp-idf = self.packages.x86_64-linux.esp-idf;
      };
      default = self.packages.x86_64-linux.fw;
    };

  };
}
