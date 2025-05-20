{ stdenv, fetchurl, SDL2, libvorbis, libogg, libpng, which }:

stdenv.mkDerivation rec {
  pname = "megazeux";
  version = "2.92f";

  src = fetchurl {
    name = "${pname}-${version}.tar.xz";
    url = "https://www.digitalmzx.com/download.php?latest=src&ver=${version}";
    sha256 = "1g1r6dkf27b6pz85m0ykglrrm97yymfc45licw42rav4s0gar6r3";
  };

  configureScript = "./config.sh";

  dontAddPrefix = true;

  # Platform will need to by dynamic for Darwin
  preConfigure = ''
    configureFlagsArray+=(
      --platform unix
      --prefix $prefix
      --sysconfdir $out/etc/
      --gamesdir $out/bin
      --bindir $out/lib/megazeux/${version}/
      --enable-release
    )
  '';

  nativeBuildInputs = [
    SDL2
    libvorbis
    libogg
    libpng
    which
  ];
  
  enableParallelBuilding = true;

  meta = with stdenv.lib; {
    description = "A Game Creation System (GCS)";
    longDescription = "MegaZeux is a Game Creation System (GCS) originally developed in 1994 by Alexis Janson and still maintained today.\n";
    homepage = "https://www.digitalmzx.com/";
    license = licenses.gpl2;
  };
}
