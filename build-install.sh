#/bin/sh
PKG=hp-ins-1.0-1
apt install libevdev-dev
gcc -Wall `pkg-config libevdev --cflags` -o hp_ins hp_ins.c `pkg-config libevdev --libs`

mkdir -p ./pkg2/$PKG/usr/local/bin
mkdir -p ./pkg2/$PKG/etc/systemd/system

cp hp_ins         ./pkg2/$PKG/usr/local/bin
cp hp_ins.sh      ./pkg2/$PKG/usr/local/bin
cp hp_ins.service ./pkg2/$PKG/etc/systemd/system
