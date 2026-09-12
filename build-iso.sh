#!/usr/bin/env bash

set -euo pipefail

ISO_URL="${ISO_URL:-https://releases.ubuntu.com/24.04/ubuntu-24.04.2-desktop-amd64.iso}"
CARLINHO_VERSION="${CARLINHO_VERSION:-1.0}"
WORK="${WORK:-/mnt/carlinho}"

if [ "$(id -u)" -ne 0 ]; then exec sudo bash "$0" "$@"; fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

command -v xorriso >/dev/null || {
  apt-get update -qq
  apt-get install -y --no-install-recommends \
    xorriso squashfs-tools rsync wget grub-pc-bin isolinux
}

mkdir -p "$WORK/tree" "$WORK/out"
cd "$WORK"
echo ">>> Diretório de trabalho: $WORK"

if [ ! -f ubuntu.iso ]; then
  echo ">>> Baixando ISO base..."
  wget -q --show-progress -O ubuntu.iso "$ISO_URL"
fi

echo ">>> Extraindo..."
MNT="$WORK/isomnt"; mkdir -p "$MNT"
mountpoint -q "$MNT" && umount -l "$MNT" || true
mount -o loop,ro ubuntu.iso "$MNT"

rsync -a --exclude='*.squashfs' --exclude='*.size' "$MNT"/ tree/

SQUASHFS=$(find "$MNT/casper" -maxdepth 1 -name '*.squashfs' -printf '%s %f\n' \
           | sort -rn | head -n1 | cut -d' ' -f2-)
SIZE_NAME=$(cd "$MNT/casper" && ls -1 -- *.size 2>/dev/null | head -n1 || true)
MANIFEST=$(cd "$MNT/casper" && ls -1 -- *.manifest 2>/dev/null | grep -v -- '-remove' | head -n1 || true)
[ -n "$SQUASHFS" ] || { echo "ERRO: *.squashfs não encontrado em casper/"; exit 1; }
echo ">>> Rootfs principal: $SQUASHFS (size=$SIZE_NAME manifest=$MANIFEST)"

rm -rf root
unsquashfs -no-progress -d root "$MNT/casper/$SQUASHFS"
umount "$MNT"

echo ">>> Carlinho-ficando o rootfs..."
mount --bind /dev     root/dev
mount --bind /dev/pts root/dev/pts
mount -t proc  proc   root/proc
mount -t sysfs sys    root/sys

rm -f root/etc/resolv.conf
cp /etc/resolv.conf root/etc/resolv.conf

cp "$SCRIPT_DIR/carlinho-chroot.sh" root/root/
WALLPAPER=$(ls "$SCRIPT_DIR"/wallpaper.{jpg,jpeg,png,JPG,JPEG,PNG} 2>/dev/null | head -n1 || true)
[ -n "$WALLPAPER" ] || { echo "ERRO: coloque a imagem de fundo como wallpaper.jpg na raiz do repo!"; exit 1; }
cp "$WALLPAPER" root/root/

chroot root /bin/bash /root/carlinho-chroot.sh

rm -f root/root/carlinho-chroot.sh root/root/wallpaper.* root/root/.bash_history
rm -f root/etc/resolv.conf
ln -s ../run/systemd/resolve/stub-resolv.conf root/etc/resolv.conf
for m in dev/pts dev proc sys; do umount -l "root/$m" 2>/dev/null || true; done

echo ">>> Gerando squashfs (zstd)... [10–20 min]"
NEW_SIZE=$(du -sx --block-size=1 root | cut -f1)

if [ -n "$MANIFEST" ]; then
  chroot root dpkg-query -W --showformat='${Package} ${Version}\n' \
    > "tree/casper/$MANIFEST" || true
fi

mksquashfs root "tree/casper/$SQUASHFS" -comp zstd -b 1M -noappend -no-recovery
if [ -n "$SIZE_NAME" ]; then echo "$NEW_SIZE" > "tree/casper/$SIZE_NAME"; fi
rm -rf root

echo "Carlinho Linux $CARLINHO_VERSION (baseado no Ubuntu 24.04 LTS)" > tree/.disk/info
sed -i 's/Ubuntu/Carlinho/g' tree/boot/grub/grub.cfg 2>/dev/null || true
sed -i 's/Ubuntu/Carlinho/g' tree/boot/grub/loopback.cfg 2>/dev/null || true

echo ">>> Montando a ISO final"
if   [ -f tree/boot/grub/bios.img ];            then BIOS_IMG=boot/grub/bios.img
elif [ -f tree/boot/grub/i386-pc/eltorito.img ]; then BIOS_IMG=boot/grub/i386-pc/eltorito.img
elif [ -f tree/isolinux/isolinux.bin ];          then BIOS_IMG=isolinux/isolinux.bin
else
  echo "ERRO: imagem de boot BIOS não reconhecida. Estrutura de boot/ e EFI/:"
  find tree/boot tree/EFI -type f 2>/dev/null | head -50
  exit 1
fi
EFI_IMG=$(find tree/EFI -type f -iname 'efiboot.img' | head -n1)
[ -n "$EFI_IMG" ] || { echo "ERRO: efiboot.img não encontrado"; exit 1; }
echo ">>> Boot BIOS: $BIOS_IMG | Boot EFI: $EFI_IMG"

if [[ "$BIOS_IMG" == isolinux/* ]]; then
  MBR_ARGS=(-isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin -isohybrid-gpt-basdat)
else
  MBR_ARGS=(--grub2-mbr /usr/lib/grub/i386-pc/boot_hybrid.img)
fi

VOLID="CARLINHO_${CARLINHO_VERSION//./_}_AMD64"
OUT_ISO="$WORK/out/carlinho-$CARLINHO_VERSION-amd64.iso"

xorriso -as mkisofs -r \
  -V "$VOLID" \
  -o "$OUT_ISO" \
  -J -joliet-long -l -iso-level 3 \
  -partition_offset 16 \
  "${MBR_ARGS[@]}" \
  -b "$BIOS_IMG" -c boot.catalog \
  -no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info \
  -eltorito-alt-boot \
  -e '--interval:appended_partition_2:::' \
  -no-emul-boot \
  -append_partition 2 28732ac11ff8d211ba4b00a0c93ec93b "$EFI_IMG" \
  -appended_part_as_gpt \
  -iso_mbr_part_type a2a0d0ebe5b9334487c068b6b72699c7 \
  --mbr-force-bootable \
  tree/

echo ">>> ISO gerada:"
ls -lh "$OUT_ISO"
sha256sum "$OUT_ISO" | tee "$OUT_ISO.sha256"
echo ">>> Inspeção de boot (para conferência):"
fdisk -l "$OUT_ISO" 2>&1 || true
xorriso -indev "$OUT_ISO" -report_el_torito as_mkisofs 2>&1 | head -20 || true
echo
echo ">>> PRONTO! Teste em VM antes de gravar:"
echo "    qemu-system-x86_64 -enable-kvm -m 4096 -cdrom $OUT_ISO -boot d"
