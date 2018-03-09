#! /bin/bash

# Fixer les variables avec les chemins de vos fichiers
HDA="-hda pnl-tp.img"
HDB="-hdb myHome.img"
KERNEL=/tmp/linux-4.14.20/arch/x86/boot/bzImage

if [ -n "${KDB}" ]; then
    KGD_WAIT='kgdbwait'
fi

CMDLINE="root=/dev/sda1 rw vga=792 console=ttyS0 kgdboc=ttyS1 ${KGD_WAIT}"

FLAGS="--enable-kvm "

exec qemu-system-x86_64 ${FLAGS} \
     ${HDA} ${HDB} \
     -net user -net nic \
     -serial stdio \
     -boot c -m 2G \
     -kernel "${KERNEL}" -append "${CMDLINE}"

