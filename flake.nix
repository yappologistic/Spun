{
  description = "A CD, vinyl and cassette music player for Linux";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    {
      self,
      nixpkgs,
    }:
    let
      forAllSystems =
        f:
        nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ] (
          system: f nixpkgs.legacyPackages.${system}
        );
    in
    {
      packages = forAllSystems (
        pkgs:
        let
          qt6 = pkgs.qt6;
          youtubeRuntime = pkgs.python3.withPackages (ps: [
            ps.ytmusicapi
            ps.yt-dlp
          ]);
        in
        {
          default = pkgs.stdenv.mkDerivation {
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

            qtWrapperArgs = [
              "--set-default SPUN_YOUTUBE_PYTHON ${youtubeRuntime}/bin/python3"
              "--set-default SPUN_YOUTUBE_HELPER ${placeholder "out"}/share/spun/youtube.py"
              "--prefix PATH : ${pkgs.lib.makeBinPath [ pkgs.nodejs ]}"
            ];

            desktopItems = [
              (pkgs.makeDesktopItem {
                name = "spun";
                desktopName = "Spun";
                comment = "Play local music or control Cider";
                exec = "spun %U";
                icon = "spun";
                categories = [
                  "AudioVideo"
                  "Audio"
                  "Player"
                ];
                terminal = false;
              })
            ];

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
        }
      );

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.python3
            pkgs.qt6.qtbase
            pkgs.qt6.qtdeclarative
            pkgs.qt6.qtmultimedia
            pkgs.qt6.qtsvg
            pkgs.qt6.qtwayland
            pkgs.qt6.qtquick3d
            pkgs.taglib
          ];
        };
      });
    };
}
