# bin/sh

CMAKE_FLAGS=''
CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING=OFF -DCMAKE_BUILD_TYPE=Debug'

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
#make -j14
make -j
popd

open ./build/Binaries/Dolphin.app
