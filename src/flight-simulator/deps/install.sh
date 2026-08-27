set -x
set -e

echo "Building mixr dependencies in debug mode"
conan create ./jsbsim  --build=missing --settings=build_type=Debug
conan create ./openrti --build=missing --settings=build_type=Debug

echo "Building mixr dependencies in release mode"
conan create ./jsbsim  --build=missing --settings=build_type=Release
conan create ./openrti --build=missing --settings=build_type=Release

git clone --depth=1 https://github.com/JSBSim-Team/jsbsim ./jsbsim/data