#/bin/sh
systemctl stop hp_ins
systemctl disable hp_ins
rm /etc/systemd/system/hp_ins.service
rm /usr/local/bin/hp_ins*
