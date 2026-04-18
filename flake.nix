{
  description = "cl-editor development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          # Build tools
          xmake
          clang
          gcc
          pkg-config
          python3
          luajit
        ];

        shellHook = ''
          export PKG_CONFIG_PATH="${pkgs.sdl3.dev}/lib/pkgconfig:$PKG_CONFIG_PATH"
        '';
      };
    };
}
