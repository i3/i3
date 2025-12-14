{
  description = "Build deps for i3";

  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = {
    flake-utils,
    nixpkgs,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
        with pkgs; {
          devShells.default = mkShell rec {
            buildInputs = [
              cairo
              clang
              iconv
              libev
              libstartup_notification
              libxcb
              libxcb-cursor
              libxcb-keysyms
              libxcb-util
              libxcb-wm
              libxkbcommon
              meson
              ninja
              pango
              pcre2
              pkg-config
              xcbutilxrm
              yajl
            ];

            LIBCLANG_PATH = pkgs.lib.makeLibraryPath [pkgs.llvmPackages_latest.libclang.lib];
            LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath buildInputs;
          };
        }
    );
}
