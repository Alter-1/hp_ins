#!/bin/sh

kbd_event=/dev/input/event4
if [ -f /etc/hp_ins.conf ] ; then
  . /etc/hp_ins.conf
fi

/usr/local/bin/hp_ins $kbd_event
