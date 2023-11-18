{
  description = "A very basic flake";
  inputs = {
    esp-idf = {
      url =
        "git+https://github.com/espressif/esp-idf.git?ref=refs/tags/v4.4.3&submodules=1";
      flake = false;
    };
    dream2nix = { url = "github:nix-community/dream2nix"; };
    nixpkgs.follows = "dream2nix/nixpkgs";
  };

  outputs = { self, nixpkgs, esp-idf, dream2nix }@inputs: let
    esp-idf = self.packages.x86_64-linux.esp-idf;
    in {

    devShells.x86_64-linux.default = nixpkgs.legacyPackages.x86_64-linux.mkShell {
      packages = [ esp-idf ];
      shellHook = ''
        export IDF_PATH="${esp-idf}"
        source ${esp-idf}/export.sh
      '';
    };
    packages.x86_64-linux = {
      esp-idf = inputs.dream2nix.lib.evalModules {
        packageSets.nixpkgs =
          inputs.dream2nix.inputs.nixpkgs.legacyPackages.x86_64-linux;
        modules = [
          ./nix/esp-idf/module.nix
          {
            paths = {
              projectRoot = ./.;
              projectRootFile = "flake.nix";
              package = ./nix/esp-idf;
            };
            deps = { inherit (inputs) esp-idf; };
          }
        ];
      };
      default = self.packages.x86_64-linux.esp-idf;
    };

  };
}
