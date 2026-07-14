{
  description = "lexer generator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
    in
    {
      packages = forAllSystems (pkgs: {
        default = pkgs.stdenv.mkDerivation {
          pname = "lexer-gen";
          version = "1.0.0";

          src = self;

          nativeBuildInputs = with pkgs; [
            meson
            ninja
          ];

          mesonBuildType = "release";
        };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.system}.default ];

          packages = with pkgs; [
            ninja
            meson
            clang-tools
          ];
        };
      });
    };
}
