{
  description = "A CD, vinyl and cassette music player for Linux";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        qt6 = pkgs.qt6;
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "spun";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.python3
            qt6.wrapQtAppsHook
          ];

          buildInputs = [
            qt6.qtbase
            qt6.qtdeclarative
            qt6.qtmultimedia
            qt6.qtsvg
            qt6.qtquick3d
            pkgs.taglib
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DBUILD_TESTING=OFF"
            "-DSPUN_ENABLE_3D=ON"
          ];

          installPhase = ''
            runHook preInstall
            install -Dm755 spun "$out/bin/spun"
            runHook postInstall
          '';

          meta = {
            description = "A CD-shaped music player for Linux, with local playback and Cider integration";
            homepage = "https://github.com/yappologistic/Spun";
            license = pkgs.lib.licenses.unfree; # PolyForm Noncommercial 1.0.0
            platforms = pkgs.lib.platforms.linux;
            mainProgram = "spun";
          };
        };

        devShells.default = pkgs.mkShell {
          packages = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.python3
            qt6.qtbase
            qt6.qtdeclarative
            qt6.qtmultimedia
            qt6.qtsvg
            qt6.qtquick3d
            pkgs.taglib
          ];

          shellHook = ''
            export QT_QPA_PLATFORM=xcb
          '';
        };
      }
    );
}
