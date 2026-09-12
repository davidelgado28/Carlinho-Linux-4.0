#!/usr/bin/env bash

set -euo pipefail

CARLINHO_VERSION="${CARLINHO_VERSION:-1.0}"
WORK="${WORK:-/mnt/carlinho}"

if [ "$(id -u)" -ne 0 ]; then exec sudo bash "$0" "$@"; fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

command -v xorriso >/dev/null || {
  apt-get update -qq
  apt-get install -y --no-install-recommends \
    xorriso squashfs-tools rsync wget curl grub-pc-bin isolinux fdisk
}

mkdir -p "$WORK/tree" "$WORK/out"
cd "$WORK"
echo ">>> Diretório de trabalho:"; df -h "$WORK" | tail -n1

if [ -z "${ISO_URL:-}" ]; then
  BASE_URL="https://releases.ubuntu.com/24.04/"
  LISTING=$(curl -fsSL "$BASE_URL") || { echo "ERRO: falha ao acessar $BASE_URL"; exit 1; }
  ISO_NAME=$(printf '%s\n' "$LISTING" \
    | grep -oE 'ubuntu-24\.04(\.[0-9]+)?-desktop-amd64\.iso"' \
    | tr -d '"' | sort -uV | tail -n1 || true)
  [ -n "$ISO_NAME" ] || { echo "ERRO: nenhum ISO desktop amd64 listado em $BASE_URL"; exit 1; }
  ISO_URL="${BASE_URL}${ISO_NAME}"
fi
echo ">>> ISO-base: $ISO_URL"

if [ ! -f ubuntu.iso ]; then
  echo ">>> Baixando (~6 GB)..."
  wget --progress=dot:giga --tries=3 -O ubuntu.iso "$ISO_URL"
fi
echo ">>> Verificando SHA256..."
EXPECTED=$(curl -fsSL "${ISO_URL%/*}/SHA256SUMS" \
  | awk -v f="$(basename "$ISO_URL")" '$0 ~ f {print $1}')
if [ -n "$EXPECTED" ]; then
  echo "$EXPECTED  ubuntu.iso" | sha256sum -c -
else
  echo "AVISO: SHA256SUMS indisponível; seguindo sem verificação."
fi

MNT="$WORK/isomnt"; mkdir -p "$MNT"
mountpoint -q "$MNT" && umount -l "$MNT" || true
mount -o loop,ro ubuntu.iso "$MNT"

rsync -a --exclude='*.squashfs' --exclude='*.size' "$MNT"/ tree/
rm -f tree/boot.catalog tree/boot/boot.catalog   

SQUASHFS=$(find "$MNT/casper" -maxdepth 1 -name '*.squashfs' -printf '%s %f\n' \
           | sort -rn | head -n1 | cut -d' ' -f2-)
[ -n "$SQUASHFS" ] || { echo "ERRO: *.squashfs não encontrado em casper/"; exit 1; }
SIZE_NAME=$(cd "$MNT/casper" && ls -1 -- *.size 2>/dev/null | head -n1 || true)
MANIFEST=$(cd "$MNT/casper" && ls -1 -- *.manifest 2>/dev/null | grep -v -- '-remove' | head -n1 || true)
echo ">>> Rootfs: $SQUASHFS (size=$SIZE_NAME manifest=$MANIFEST)"

rm -rf root
unsquashfs -no-progress -d root "$MNT/casper/$SQUASHFS"
umount "$MNT"

mount --bind /dev     root/dev
mount --bind /dev/pts root/dev/pts
mount -t proc  proc   root/proc
mount -t sysfs sys    root/sys

rm -f root/etc/resolv.conf
cp /etc/resolv.conf root/etc/resolv.conf

cp "$SCRIPT_DIR/carlinho-chroot.sh" root/root/
WALLPAPER=$(ls "$SCRIPT_DIR"/wallpaper.{jpg,jpeg,png,JPG,JPEG,PNG} 2>/dev/null | head -n1 || true)
[ -n "$WALLPAPER" ] || { echo "ERRO: coloque a imagem como wallpaper.jpg na raiz do repo!"; exit 1; }
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

NEW_UUID=$(unsquashfs -s "tree/casper/$SQUASHFS" 2>/dev/null | grep -i uuid | awk '{print $NF}' || true)
if [ -n "$NEW_UUID" ] && [ -f tree/.disk/casper-uuid ]; then
  echo "$NEW_UUID" > tree/.disk/casper-uuid
fi
rm -rf root

echo "Carlinho Linux $CARLINHO_VERSION (baseado no Ubuntu 24.04 LTS)" > tree/.disk/info
sed -i 's/Ubuntu/Carlinho/g' tree/boot/grub/grub.cfg 2>/dev/null || true
sed -i 's/Ubuntu/Carlinho/g' tree/boot/grub/loopback.cfg 2>/dev/null || true

BIOS_IMG=""
for cand in boot/grub/bios.img boot/grub/i386-pc/eltorito.img isolinux/isolinux.bin; do
  if [ -f "tree/$cand" ]; then BIOS_IMG="$cand"; break; fi
done
[ -n "$BIOS_IMG" ] || { echo "ERRO: imagem de boot BIOS não encontrada"; find tree/boot -maxdepth 2 -type f; exit 1; }
echo ">>> Boot BIOS: $BIOS_IMG"

echo ">>> Extraindo partição EFI do ISO original..."
EFI_LINE=$(fdisk -l ubuntu.iso | grep -i 'EFI' | head -n1 || true)
[ -n "$EFI_LINE" ] || { echo "ERRO: partição EFI não encontrada no ISO base."; exit 1; }

if echo "$EFI_LINE" | awk '{print $2}' | grep -q '\*'; then
  EFI_START=$(echo "$EFI_LINE" | awk '{print $3}')
  EFI_SECTORS=$(echo "$EFI_LINE" | awk '{print $5}')
else
  EFI_START=$(echo "$EFI_LINE" | awk '{print $2}')
  EFI_SECTORS=$(echo "$EFI_LINE" | awk '{print $4}')
fi

echo ">>> EFI START=$EFI_START, SECTORS=$EFI_SECTORS"
dd if=ubuntu.iso of=efi.img bs=512 skip="$EFI_START" count="$EFI_SECTORS" status=none

if [[ "$BIOS_IMG" == isolinux/* ]]; then
  MBR_ARGS=(-isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin)
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
  -append_partition 2 28732ac11ff8d211ba4b00a0c93ec93b efi.img \
  -appended_part_as_gpt \
  -iso_mbr_part_type a2a0d0ebe5b9334487c068b6b72699c7 \
  --mbr-force-bootable \
  tree/

ls -lh "$OUT_ISO"
sha256sum "$OUT_ISO" | tee "$OUT_ISO.sha256"
echo ">>> Boot info da ISO gerada:"
xorriso -indev "$OUT_ISO" -report_el_torito as_mkisofs 2>&1 | head -25 || true
echo
echo ">>> PRONTO! Teste em VM antes de gravar:"
echo "    qemu-system-x86_64 -enable-kvm -m 4096 -cdrom $OUT_ISO -boot d"
