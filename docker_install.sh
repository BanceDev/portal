#!/bin/sh

apt-get update
apt-get install -y sqlite3 libsqlite3-dev openssl libssl-dev libargon2-dev

PREMAKE_VERSION="5.0.0-beta2"
OS="linux"

echo "downloading premake $PREMAKE_VERSION"
wget -q https://github.com/premake/premake-core/releases/download/v${PREMAKE_VERSION}/premake-${PREMAKE_VERSION}-${OS}.tar.gz -O premake.tar.gz
echo "extracting premake"
tar -xzf premake.tar.gz
echo "installing premake"
mv premake5 example.so libluasocket.so /usr/bin
chmod +x /usr/bin/premake5
rm premake.tar.gz

premake5 gmake
make PortalServer
cp -rf ./.portal ~/

echo "====================="
echo "INSTALLATION FINISHED"
echo "====================="
