#!/bin/sh

# Function to install packages using apt (Debian/Ubuntu)
install_with_apt() {
  sudo apt-get update
  sudo apt-get install -y mesa-utils libglfw3-dev sqlite3 libsqlite3-dev
}

# Function to install packages using yum (Red Hat/CentOS)
install_with_yum() {
  sudo yum install -y glx-utils glfw-devel sqlite sqlite-devel
}

# New package manager used in Fedora
install_with_dnf() {
  sudo dnf install -y glx-utils glfw-devel sqlite sqlite-devel
}

# Function to install packages using pacman (Arch Linux)
install_with_pacman() {
  sudo pacman -Sy --noconfirm glxinfo glfw sqlite
}

if [ -f /etc/arch-release ]; then
  install_with_pacman
elif [ -f /etc/debian_version ]; then
  install_with_apt
elif [ -f /etc/redhat-release ] || [ -f /etc/centos-release ]; then
  if command -v dnf >/dev/null 2>&1; then
    install_with_dnf
  else
    install_with_yum
  fi
else
  echo "Your linux distro is not supported currently."
  echo "You need to manually install those packages: exiftool, jq, glfw"
fi

if ! pkg-config cglm; then
  echo "cglm not found on the system"
  echo "building cglm"
  git clone https://github.com/recp/cglm
  cd cglm
  mkdir build
  cd build
  cmake ..
  make -j$(nproc)
  sudo make install
  cd ../../
  rm -rf cglm
fi

if ! pkg-config libclipboard; then
  echo "libclipboard not found on the system"
  echo "building libclipboard"
  git clone https://github.com/jtanx/libclipboard
  cd libclipboard
  cmake .
  make -j$(nproc)
  sudo install
  cd ..
  rm -rf libclipboard
fi

PREMAKE_VERSION="5.0.0-beta2"
OS="linux"

echo "downloading premake $PREMAKE_VERSION"
wget -q https://github.com/premake/premake-core/releases/download/v${PREMAKE_VERSION}/premake-${PREMAKE_VERSION}-${OS}.tar.gz -O premake.tar.gz
echo "extracting premake"
tar -xzf premake.tar.gz
echo "installing premake"
sudo mv premake5 example.so libluasocket.so /usr/bin
sudo chmod +x /usr/bin/premake5
rm premake.tar.gz

premake5 gmake
make
cp -rf ./.portal ~/

echo "====================="
echo "INSTALLATION FINISHED"
echo "====================="
