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
        parser = pkgs.tree-sitter.buildGrammar {
          language = "foo";
          version = "0.42.0";
          src = ./tree-sitter-leg;
        };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.stdenv.hostPlatform.system}.default ];

          packages = with pkgs; [
            ninja
            meson
            clang-tools
            tree-sitter
            nodejs_24
          ];
        };
      });
    };
}
