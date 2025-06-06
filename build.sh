#!/usr/bin/fish

rm -rf build/
mkdir build
cd build
cmake ../src -DCMAKE_C_FLAGS="-Wno-incompatible-pointer-types" -DCMAKE_CXX_FLAGS="-Wno-incompatible-pointer-types" -DCMAKE_POLICY_VERSION_MINIMUM=3.5
echo "Starting to build please ensure you have more than 4GB ram free!"
cmake --build . --config Release --parallel
cp misc/icon.png build/main/misc/
