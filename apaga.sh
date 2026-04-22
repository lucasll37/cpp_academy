#!/usr/bin/env bash
set -e

PREFIX=$HOME/.local

echo "🔧 Instalando dependências..."
sudo apt update
sudo apt install -y \
    git cmake build-essential \
    clang llvm \
    libzmq3-dev libcurl4-openssl-dev \
    nlohmann-json3-dev

mkdir -p ~/build && cd ~/build

# xeus
echo "📦 Instalando xeus..."
git clone https://github.com/jupyter-xeus/xeus.git
cd xeus
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PREFIX
make -j$(nproc)
make install
cd ~/build

# cling
echo "📦 Instalando cling..."
git clone https://github.com/root-project/cling.git
cd cling
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PREFIX
make -j$(nproc)
make install
cd ~/build

# xeus-cling
echo "📦 Instalando xeus-cling..."
git clone https://github.com/jupyter-xeus/xeus-cling.git
cd xeus-cling
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PREFIX
make -j$(nproc)
make install

echo "🔗 Registrando kernel..."
$PREFIX/bin/xcpp --install

echo "✅ Concluído!"