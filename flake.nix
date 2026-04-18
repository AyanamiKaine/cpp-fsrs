{
  description = "cpp-fsrs development environment";

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
          xmake
          clang
          gcc
          pkg-config
          python3            # for running py-fsrs as a test oracle
          luajit             # required by xmake
          catch2_3           # fallback if xmake package fetch fails
          nlohmann_json      # fallback if xmake package fetch fails
        ];
      };
    };
}
