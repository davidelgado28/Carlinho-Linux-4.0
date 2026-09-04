#!/bin/bash
set -e

echo "=== Iniciando a preparação do ambiente de build ==="
mkdir -p output work/chroot

apt-get update
apt-get install -y debootstrap squashfs-tools genisoimage xorriso curl wget git

echo "=== Construindo o sistema base com Debootstrap ==="
debootstrap --arch=amd64 noble work/chroot http://archive.ubuntu.com/ubuntu/

mount --bind /dev work/chroot/dev
mount --bind /sys work/chroot/sys
mount --bind /proc work/chroot/proc

echo "=== Instalando ambiente de desenvolvimento e ferramentas essenciais ==="
cat << 'EOF' > work/chroot/tmp/install-tools.sh
export DEBIAN_FRONTEND=noninteractive

cat << 'REPOS' > /etc/apt/sources.list
deb http://archive.ubuntu.com/ubuntu/ noble main restricted universe multiverse
deb http://archive.ubuntu.com/ubuntu/ noble-updates main restricted universe multiverse
deb http://security.ubuntu.com/ubuntu noble-security main restricted universe multiverse
REPOS

apt-get update
apt-get install -y \
    ubuntu-desktop-minimal \
    build-essential \
    git \
    curl \
    wget \
    neovim \
    zsh \
    tmux \
    docker.io \
    nodejs \
    npm \
    python3 \
    python3-pip \
    gnome-tweaks \
    dconf-cli

apt-get clean
rm -rf /var/lib/apt/lists/*
EOF

chmod +x work/chroot/tmp/install-tools.sh
chroot work/chroot /tmp/install-tools.sh
rm work/chroot/tmp/install-tools.sh

echo "=== Configurando o Papel de Parede Único ==="
mkdir -p work/chroot/usr/share/backgrounds
mkdir -p work/chroot/usr/share/gnome-background-properties
cp assets/wallpaper.jpg work/chroot/usr/share/backgrounds/dev-wallpaper.jpg

cat << 'EOF' > work/chroot/usr/share/gnome-background-properties/dev-wallpaper.xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE wallpapers SYSTEM "gnome-wp-list.dtd">
<wallpapers>
  <wallpaper deleted="false">
    <name>C Custom Dev Wallpaper</name>
    <filename>/usr/share/backgrounds/dev-wallpaper.jpg</filename>
    <options>zoom</options>
    <shade_type>solid</shade_type>
    <pcolor>#000000</pcolor>
    <scolor>#000000</scolor>
  </wallpaper>
</wallpapers>
EOF

mkdir -p work/chroot/etc/dconf/profile
mkdir -p work/chroot/etc/dconf/db/local.d

cat << 'EOF' > work/chroot/etc/dconf/profile/user
user-db:user
system-db:local
EOF

cat << 'EOF' > work/chroot/etc/dconf/db/local.d/01-wallpaper
[org/gnome/desktop/background]
picture-uri='file:///usr/share/backgrounds/dev-wallpaper.jpg'
picture-uri-dark='file:///usr/share/backgrounds/dev-wallpaper.jpg'
picture-options='zoom'
EOF

chroot work/chroot dconf update
umount work/chroot/dev
umount work/chroot/sys
umount work/chroot/proc

echo "=== Gerando a Imagem ISO Final ==="
mksquashfs work/chroot output/filesystem.squashfs -comp xz -Xbcj x86
genisoimage -V "MeuLinux-Dev" -r -J -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o output/meu-linux-dev.iso work/chroot || true

echo "=== Processo concluído com sucesso! ISO gerada em output/ ==="
