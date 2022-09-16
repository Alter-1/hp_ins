#/bin/sh
systemctl stop hp_ins
cp -a ./etc /
cp -a ./usr /
systemctl daemon-reload
if [ "x$1" != "x" ] ; then 
  echo kdb_event=$1 > /etc/hp_ins.conf
fi
systemctl start hp_ins
systemctl enable hp_ins
