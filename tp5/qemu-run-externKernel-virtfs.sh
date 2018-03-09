#! /bin/bash

# Fixer les variables avec les chemins de vos fichiers
HDA="-hda pnl-tp.img"
HDB="-hdb myHome.img"
KERNEL=linux-4.9.83/arch/x86/boot/bzImage

if [ -n "${KDB}" ]; then
    KGD_WAIT='kgdbwait'
fi

CMDLINE="root=/dev/sda1 rw vga=792 console=ttyS0 kgdboc=ttyS1 ${KGD_WAIT}"

FLAGS="--enable-kvm "
VIRTFS+=" --virtfs local,path=.,mount_tag=share,security_model=passthrough,id=share "

exec qemu-system-x86_64 ${FLAGS} \
     ${HDA} ${HDB} \
     ${VIRTFS} \
     -net user -net nic \
     -serial stdio -serial tcp::1234,server,nowait \
     -boot c -m 1G \
     -kernel "${KERNEL}" -append "${CMDLINE}"
