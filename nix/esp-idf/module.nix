{ config, lib, dream2nix, packageSets, ... }:
let
  src = config.deps.esp-idf;
  platform = "linux-amd64";

  tools = (builtins.fromJSON (builtins.readFile "${src}/tools/tools.json")).tools;

  toDownload = tool:
    let
      version = lib.findFirst ({ status, ... }: status == "recommended") null
        tool.versions;
      download = version."${platform}" or null;
    in if download != null then
      config.deps.fetchurl { inherit (download) url sha256; }
    else
      null;

  toolsTars = (builtins.filter (x: x != null) (builtins.map toDownload tools));


in
{
  imports = [ dream2nix.modules.dream2nix.pip ];

  name = "esp-idf";
  version = "4.4.3";

  mkDerivation = {
    src = config.deps.esp-idf;
    buildInputs = with config.deps; [
      libcpp
      python2Insecure
      libusb1
      zlib
    ];
    nativeBuildInputs = with config.deps; [
      autoPatchelfHook
    ];
    propagatedBuildInputs = with config.deps; [ config.public.pyEnv git perl cmake ];
    buildPhase = ''
      sed \
        -e '/^gdbgui/d' \
        -e '/^kconfiglib/c kconfiglib' \
        -e '/^construct/c construct' \
        -i requirements.txt
      echo v$version > version.txt
      export IDF_TOOLS_PATH=$out/tool
      mkdir -p $out/tool/dist
      ${ # symlink all tools
        lib.concatMapStringsSep "\n" (tool: ''
          ln -s ${tool} $out/tool/dist/$(echo ${tool} | cut -d'-' -f 2-)
        '') toolsTars
      } 
      patchShebangs .
      python3 ./tools/idf_tools.py install
      # remove .tar.gz dependencies
      rm -rf $out/tool/dist
    '';
    installPhase = ''
      cp -r ./. $out/
      mkdir $out/bin
      ln -s $out/tool/tools/*/*/*/bin/* $out/bin/
    '';
  };

  buildPythonPackage.format = "other";

  deps = { nixpkgs, ... }: {
    python2Insecure = nixpkgs.python2.overrideAttrs (o: { meta.insecure = false; });
    libcpp = nixpkgs.stdenv.cc.cc.lib;
    inherit (nixpkgs) libusb1 zlib autoPatchelfHook git perl fetchurl cmake;
  };
  env.dontUseCmakeConfigure = true;

  pip = {
    pypiSnapshotDate = "2023-11-18";
    #requirementsFiles = [ "${config.deps.esp-idf}/requirements.txt" ];
    flattenDependencies = true;
    requirementsList = [
      "setuptools>=21"
      "click>=7.0"
      "pyserial>=3.3"
      "future>=0.15.2"
      "cryptography>=2.1.4"
      "--only-binary" "cryptography"
      "pyparsing>=2.0.3,<2.4.0"
      "pyelftools>=0.22"
      "idf-component-manager~=1.0"
      #"gdbgui==0.13.2.0"
      "python-socketio<5"
      "jinja2<3.1"
      "itsdangerous<2.1"
      "kconfiglib==13.7.1"
      "reedsolo>=1.5.3,<=1.5.4"
      "bitstring>=3.1.6"
      "ecdsa>=0.16.0"
      "construct==2.10.54"
    ];
  };
}
