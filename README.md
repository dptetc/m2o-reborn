# m2o-reborn

## Building

0. Install cmake

```
windows: https://cmake.org/files/v3.9/cmake-3.9.0-win64-x64.msi
linux: using your package manager
macos: brew install cmake
```

1. Clone

```sh
$ git clone --recurse-submodules git@gitlab.com:mafia2online/m2o-reborn.git
```
2. Generate project

```
$ generate.sh.bat
```

3. Open the solution (Windows)

```
build/m2o-reborn.sln
```

3. Build the stuff (\*nix)

```
$ cd build && make -j4
```
