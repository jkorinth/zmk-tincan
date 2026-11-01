{
  description = "zmk-tincan dev shell and lint checks";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems f;
      cSources = "src/*.c src/*.h include/tincan/*.h";
      nixSources = "*.nix";
    in
    {
      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            packages = [
              pkgs.clang-tools
              pkgs.markdownlint-cli2
              pkgs.nixfmt
              pkgs.gcc
              pkgs.theft
            ];
          };
        }
      );

      checks = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          clang-format = pkgs.runCommand "clang-format-check" { nativeBuildInputs = [ pkgs.clang-tools ]; } ''
            cd ${self}
            clang-format --dry-run --Werror ${cSources}
            touch $out
          '';
          markdownlint =
            pkgs.runCommand "markdownlint-check" { nativeBuildInputs = [ pkgs.markdownlint-cli2 ]; }
              ''
                cd ${self}
                markdownlint-cli2 "**/*.md" "#.direnv"
                touch $out
              '';
          nixfmt = pkgs.runCommand "nixfmt-check" { nativeBuildInputs = [ pkgs.nixfmt ]; } ''
            cd ${self}
            nixfmt --check ${nixSources}
            touch $out
          '';
          frame-test =
            pkgs.runCommand "frame-test"
              {
                nativeBuildInputs = [
                  pkgs.gcc
                  pkgs.theft
                ];
              }
              ''
                gcc -std=c11 -Wall -Wextra -Werror -o frame_test ${self}/tests/test_tincan_frame.c ${self}/src/tincan_frame.c -ltheft -lm
                ./frame_test
                touch $out
              '';
        }
      );
    };
}
