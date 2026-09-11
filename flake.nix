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
    flake-utils.lib.eachSystem [ "x86_64-linux" "aarch64-linux" ] (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        qt6 = pkgs.qt6;
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "spun";
          version = "0.1.0";

          src = pkgs.lib.cleanSource ./.;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.python3
            pkgs.copyDesktopItems
            qt6.wrapQtAppsHook
          ];

          buildInputs = [
            qt6.qtbase
            qt6.qtdeclarative
            qt6.qtmultimedia
            qt6.qtsvg
            qt6.qtwayland
            qt6.qtquick3d
            pkgs.taglib
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DBUILD_TESTING=OFF"
            "-DSPUN_ENABLE_3D=ON"
            "-DSPUN_DEMO_FILE=${placeholder "out"}/share/spun/First-Light.flac"
          ];

          installPhase = ''
            runHook preInstall
            install -Dm755 spun "$out/bin/spun"
            install -Dm644 ../assets/First-Light.flac "$out/share/spun/First-Light.flac"
            install -Dm644 ../assets/spun-icon.png "$out/share/icons/hicolor/512x512/apps/spun.png"
            install -Dm644 ../LICENSE "$out/share/licenses/spun/LICENSE"
            install -Dm644 ../NOTICE "$out/share/licenses/spun/NOTICE"
            runHook postInstall
          '';

          desktopItems = [ (pkgs.makeDesktopItem {
            name = "spun";
            desktopName = "Spun";
            comment = "Play local music or control Cider";
            exec = "spun %U";
            icon = "spun";
            categories = [ "AudioVideo" "Audio" "Player" ];
            terminal = false;
          }) ];

          meta = {
            description = "A CD-shaped music player for Linux, with local playback and Cider integration";
            homepage = "https://github.com/yappologistic/Spun";
            license = {
              fullName = "PolyForm Noncommercial 1.0.0";
              spdxId = "PolyForm-Noncommercial-1.0.0";
              url = "https://polyformproject.org/licenses/noncommercial/1.0.0/";
              free = false;
            };
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
            qt6.qtwayland
            qt6.qtquick3d
            pkgs.taglib
          ];
        };
      }
    );
}
