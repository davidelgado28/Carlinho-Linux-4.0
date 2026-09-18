#!/bin/bash
set -e

BUILD_DIR="/tmp/carlinho-build"
ISO_NAME="carlinho-os-$(date +%Y%m%d).iso"
WORK_DIR="$BUILD_DIR/work"
ISO_DIR="$BUILD_DIR/iso"

echo " Building Carlinho OS"

sudo rm -rf $BUILD_DIR
mkdir -p $WORK_DIR $ISO_DIR

echo "Installing base system"
sudo ./scripts/customize.sh $WORK_DIR

echo "Creating ISO structure"
sudo mkdir -p $ISO_DIR/{boot/grub,isolinux}
sudo cp $WORK_DIR/boot/vmlinuz* $ISO_DIR/boot/
sudo cp $WORK_DIR/boot/initrd.img* $ISO_DIR/boot/

echo "Creating squashfs"
sudo mksquashfs $WORK_DIR $ISO_DIR/carinlho-os.squashfs -comp xz -b 1024k
sudo cp /usr/lib/grub/i386-pc/moddep.lst $ISO_DIR/boot/grub/ 2>/dev/null || true
sudo cp /usr/lib/grub/x86_64-efi/moddep.lst $ISO_DIR/boot/grub/ 2>/dev/null || true

sudo tee $ISO_DIR/isolinux/isolinux.cfg > /dev/null << 'EOF'
UI menu.c32
PROMPT 0
TIMEOUT 100
MENU TITLE Carlinho OS

LABEL live
  MENU LABEL Boot Carlinho OS (Live)
  KERNEL /boot/vmlinuz
  INITRD /boot/initrd.img
  APPEND boot=casper live-media-path=/casper unionfs noprompt quiet splash --
EOF

sudo tee $ISO_DIR/boot/grub/grub.cfg > /dev/null << 'EOF'
set timeout=10
menuentry "Carlinho OS" {
    linux /boot/vmlinuz boot=casper live-media-path=/casper unionfs noprompt quiet splash --
    initrd /boot/initrd.img
}
EOF

echo "Creating ISO image..."
cd $ISO_DIR
sudo genisoimage -o ../../$ISO_NAME \
    -R -J -V "Carlinho OS" \
    -sysid LINUX \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    -b isolinux/isolinux.bin \
    -c isolinux/boot.cat \
    -input-charset utf-8 \
    .

echo "Build complete: $ISO_NAME"
ls -lh ../../$ISO_NAME
