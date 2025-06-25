# OvercastC
OvercastC is a modern, high-performance compiler for the Overcast language, built in C++ to deliver blazing fast build times.

# Features
As of Overcast v1.0:
- [x] Easy-to-use CLI
- [x] Fast build times
- [x] Basic Control Flow (if & while)
- [x] Functions
- [x] Variables
- [x] Structs
- [x] Build System
- [x] Basic Module Work (packages exist, `use` statements not yet)

# Installation
To install OvercastC you can use the simple [oc-hub](https://github.com/Overcast-Framework/oc-hub) tool from the linked repository.
Just run:
``oc-hub install`` 
and let it do the work for you!
# Compilation
1. run ``git clone --recurse-submodules https://github.com/Overcast-Framework/OvercastC.git``
1. build the LLVM(in vendors) on Release with static libs (you can find the instructions on their repo)
1. generate the OvercastC project files using premake5 (run GenerateProjectFiles.bat for VS2022)
1. build it with your selected build system.

# Acknowledgements
OvercastC uses the following libraries:
+ [toml++](https://github.com/marzer/tomlplusplus)
+ [cxxopts](https://github.com/jarro2783/cxxopts)
+ [llvm](https://github.com/llvm/llvm-project)

OvercastC is licensed under the MIT license.
#
© 2025 Overcast Framework. All rights reserved.