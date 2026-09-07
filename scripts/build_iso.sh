#!/usr/bin/env bash
set -e

export DEBIAN_FRONTEND=noninteractive

BUILD_DIR="/tmp/carlinho_build"
CHROOT_DIR="${BUILD_DIR}/chroot"
ISO_DIR="${BUILD_DIR}/iso"

echo "=== [1/7] Preparando diretórios de compilação ==="
rm -rf "${BUILD_DIR}"
mkdir -p "${CHROOT_DIR}" "${ISO_DIR}/live" "${ISO_DIR}/boot/grub"

echo "=== [2/7] Instalando sistema base (Debian Bookworm) ==="
debootstrap --arch=amd64 --variant=minbase bookworm "${CHROOT_DIR}" http://deb.debian.org/debian/

echo "=== [3/7] Configurando o ambiente Chroot e Repositórios ==="
mount --bind /dev "${CHROOT_DIR}/dev"
mount --bind /run "${CHROOT_DIR}/run"
mount -t proc proc "${CHROOT_DIR}/proc"
mount -t sysfs sysfs "${CHROOT_DIR}/sys"

cat << 'EOF' > "${CHROOT_DIR}/etc/apt/sources.list"
deb http://deb.debian.org/debian bookworm main contrib non-free non-free-firmware
deb http://deb.debian.org/debian-security bookworm-security main contrib non-free non-free-firmware
deb http://deb.debian.org/debian bookworm-updates main contrib non-free non-free-firmware
EOF

chroot "${CHROOT_DIR}" dpkg --add-architecture i386
chroot "${CHROOT_DIR}" apt-get update

echo "=== [4/7] Instalando Kernel, XFCE4, Firefox, Wine e Ferramentas Dev ==="
chroot "${CHROOT_DIR}" apt-get install -y --no-install-recommends \
    linux-image-amd64 live-boot systemd-sysv \
    xfce4 xfce4-goodies lightdm desktop-base \
    firefox-esr \
    wine wine32 wine64 winetricks mime-support \
    build-essential gcc g++ make git curl wget \
    python3 python3-pip python3-pil \
    php-cli php-fpm sqlite3 mariadb-server \
    sudo nano thunar-archive-plugin

echo "[+] Instalando Visual Studio Code"
curl -sSL "https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-x64" -o "${CHROOT_DIR}/tmp/vscode.deb"
chroot "${CHROOT_DIR}" apt-get install -y /tmp/vscode.deb
rm -f "${CHROOT_DIR}/tmp/vscode.deb"

echo "=== [5/7] Configurando Associação Automática de Arquivos .EXE ==="
cat << 'EOF' > "${CHROOT_DIR}/usr/share/applications/defaults.list"
[Default Applications]
application/x-ms-dos-executable=wine.desktop
application/x-msdownload=wine.desktop
application/exe=wine.desktop
application/x-exe=wine.desktop
application/dos-exe=wine.desktop
application/x-msi=wine.desktop
x-scheme-handler/http=firefox-esr.desktop
x-scheme-handler/https=firefox-esr.desktop
EOF

echo "=== [6/7] Criando Usuário e Configurando Interface do Sistema ==="
chroot "${CHROOT_DIR}" useradd -m -s /bin/bash carlinho
echo "carlinho:carlinho" | chroot "${CHROOT_DIR}" chpasswd
chroot "${CHROOT_DIR}" usermod -aG sudo carlinho

mkdir -p "${CHROOT_DIR}/etc/lightdm/lightdm.conf.d"
cat << 'EOF' > "${CHROOT_DIR}/etc/lightdm/lightdm.conf.d/autologin.conf"
[Seat:*]
autologin-user=carlinho
autologin-user-timeout=0
EOF

mkdir -p "${CHROOT_DIR}/usr/share/backgrounds"
cp scripts/generate_wallpaper.py "${CHROOT_DIR}/tmp/gen_bg.py"
chroot "${CHROOT_DIR}" python3 /tmp/gen_bg.py
mv "${CHROOT_DIR}/tmp/carlinho_bg.png" "${CHROOT_DIR}/usr/share/backgrounds/carlinho_bg.png"
rm -f "${CHROOT_DIR}/tmp/gen_bg.py"

mkdir -p "${CHROOT_DIR}/etc/skel/.config/xfce4/xfconf/xfce-perchannel-xml"
cat << 'EOF' > "${CHROOT_DIR}/etc/skel/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml"
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-desktop" version="1.0">
  <property name="backdrop" type="empty">
    <property name="screen0" type="empty">
      <property name="monitor0" type="empty">
        <property name="image-path" type="string" value="/usr/share/backgrounds/carlinho_bg.png"/>
        <property name="image-style" type="int" value="5"/>
      </property>
    </property>
  </property>
</channel>
EOF

cp -r "${CHROOT_DIR}/etc/skel/.config" "${CHROOT_DIR}/home/carlinho/"
chroot "${CHROOT_DIR}" chown -R carlinho:carlinho /home/carlinho/

chroot "${CHROOT_DIR}" apt-get clean
rm -rf "${CHROOT_DIR}/tmp/"*

umount "${CHROOT_DIR}/proc"
umount "${CHROOT_DIR}/sys"
umount "${CHROOT_DIR}/dev"
umount "${CHROOT_DIR}/run"

echo "=== [7/7] Gerando Imagem SquashFS e Compilando ISO Bootável ==="
mksquashfs "${CHROOT_DIR}" "${ISO_DIR}/live/filesystem.squashfs" -e boot

cp "${CHROOT_DIR}/boot/vmlinuz-"* "${ISO_DIR}/live/vmlinuz"
cp "${CHROOT_DIR}/boot/initrd.img-"* "${ISO_DIR}/live/initrd"

cat << 'EOF' > "${ISO_DIR}/boot/grub/grub.cfg"
set default=0
set timeout=5

menuentry "Carlinho OS Dev (Live Environment)" {
    linux /live/vmlinuz boot=live quiet splash
    initrd /live/initrd
}
EOF

grub-mkrescue -o carlinho-dev-os.iso "${ISO_DIR}"

echo "================================================================="
echo " ISO GERADA COM SUCESSO: carlinho-dev-os.iso"
echo "================================================================="
