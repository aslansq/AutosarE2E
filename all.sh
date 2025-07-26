rm -r build
mkdir build
cd build
cmake ..
make

ctest

./AutosarE2EDemo