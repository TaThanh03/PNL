#! /bin/bash

# Fixer les variables avec les chemins de vos fichiers
HDA=~/workplace/PNL/images/pnl-tp.img 
HDB=${HOME}/myHome.img 

exec qemu-system-x86_64 -hda "${HDA}" -hdb "${HDB}" -m 2G -net nic -net user -boot c

