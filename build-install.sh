#/bin/sh
apt install libevdev-dev
gcc -Wall `pkg-config libevdev --cflags` -o hp_ins hp_ins.c `pkg-config libevdev --libs`
cp hp_ins /usr/local/bin
cp hp_ins.service /etc/systemd/system
systemctl start hp_ins
systemctl enable hp_ins
