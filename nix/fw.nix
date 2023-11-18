{ stdenv, esp-idf, shortRev }:

stdenv.mkDerivation (o: {
  pname = "firmware-controller-fw";
  version = "0.1.0-${shortRev}";
  src = ../fw;

  nativeBuildInputs = [ esp-idf ];
  IDF_PATH = "${esp-idf}";
  IDF_PYTHON_ENV_PATH = "${esp-idf.pyEnv}";
  IDF_TOOLS_PATH="${esp-idf}/tool";
  IDF_PYTHON_CHECK_CONSTRAINTS=0;
  phases = "unpackPhase buildPhase installPhase fixupPhase";

  dontStrip = true;
  dontFixup = true;
  dontPatchELF = true;

  buildPhase = ''
    echo '${o.version}' > version.txt
    source ${esp-idf}/export.sh
    idf.py build
  '';
  shellHook = ''
    source $IDF_PATH/export.sh
  '';
})
