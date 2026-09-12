#!/usr/bin/env bash

set -eu
export DEBIAN_FRONTEND=noninteractive
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

printf '#!/bin/sh\nexit 101\n' > /usr/sbin/policy-rc.d
chmod +x /usr/sbin/policy-rc.d
trap 'rm -f /usr/sbin/policy-rc.d' EXIT

WALL=""
for f in /root/wallpaper.{jpg,jpeg,JPG,JPEG,png,PNG}; do
  [ -f "$f" ] && WALL="$f"
done
[ -z "$WALL" ] && { echo "ERRO: wallpaper ausente no chroot"; exit 1; }
EXT="${WALL##*.}"; EXT="${EXT,,}"

apt-get update
apt-get install -y gnupg wget

wget -qO- https://packages.microsoft.com/keys/microsoft.asc \
  | gpg --dearmor > /usr/share/keyrings/microsoft-archive-keyring.gpg
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/microsoft-archive-keyring.gpg] https://packages.microsoft.com/repos/code stable main" \
  > /etc/apt/sources.list.d/vscode.list

dpkg --add-architecture i386
apt-get update

apt-get install -y \
  build-essential gcc g++ gdb make cmake pkg-config git \
  python3 python-is-python3 python3-pip python3-venv python3-dev \
  manpages-dev bash-completion curl file desktop-file-utils

apt-get install -y code                                              
apt-get install -y --install-recommends wine wine32 wine64 winbind winetricks  
apt-get install -y \
  nautilus gvfs-backends file-roller \
  gnome-terminal gnome-system-monitor \
  gnome-disk-utility hardinfo dconf-cli

PURGE=""
for p in gnome-text-editor gnome-calculator gnome-calendar gnome-contacts \
         gnome-characters gnome-clocks gnome-font-viewer gnome-logs \
         gnome-maps gnome-weather gnome-music totem yelp baobab snapshot \
         simple-scan deja-dup remmina transmission-gtk rhythmbox shotwell \
         cheese libreoffice-core gnome-mines gnome-sudoku gnome-mahjongg \
         aisleriot hitori; do
  if dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q 'install ok installed'; then
    PURGE="$PURGE $p"
  fi
done
[ -n "$PURGE" ] && apt-get purge -y $PURGE
apt-get -y autoremove --purge

mkdir -p /usr/share/backgrounds
install -m 0644 "$WALL" "/usr/share/backgrounds/carlinho-wallpaper.$EXT"
find /usr/share/backgrounds -type f ! -name "carlinho-wallpaper.$EXT" -delete
ln -sf "carlinho-wallpaper.$EXT" /usr/share/backgrounds/warty-final-ubuntu.png

mkdir -p /etc/dconf/profile /etc/dconf/db/carlinho.d/locks
cat > /etc/dconf/profile/user <<EOF
user-db:user
system-db:carlinho
system-db:distro
system-db:local
EOF
cat > /etc/dconf/db/carlinho.d/00-carlinho <<EOF
[org/gnome/desktop/background]
picture-uri='file:///usr/share/backgrounds/carlinho-wallpaper.$EXT'
picture-uri-dark='file:///usr/share/backgrounds/carlinho-wallpaper.$EXT'
picture-options='zoom'
primary-color='#000000'
secondary-color='#000000'

[org/gnome/desktop/screensaver]
picture-uri='file:///usr/share/backgrounds/carlinho-wallpaper.$EXT'

[org/gnome/desktop/interface]
color-scheme='prefer-dark'
gtk-theme='Yaru-dark'

[org/gnome/shell]
favorite-apps=['firefox_firefox.desktop', 'code.desktop', 'org.gnome.Nautilus.desktop', 'org.gnome.Terminal.desktop']

[org/gnome/settings-daemon/plugins/media-keys]
custom-keybindings=['/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/']

[org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0]
name='Monitor do Sistema'
command='gnome-system-monitor'
binding=['<Primary><Shift>Escape']
EOF
cat > /etc/dconf/db/carlinho.d/locks/00-carlinho <<EOF
/org/gnome/desktop/background/picture-uri
/org/gnome/desktop/background/picture-uri-dark
/org/gnome/desktop/background/picture-options
/org/gnome/desktop/background/primary-color
/org/gnome/desktop/background/secondary-color
/org/gnome/shell/favorite-apps
EOF
dconf update

cat > /etc/xdg/mimeapps.list <<EOF
[Default Applications]
application/x-ms-dos-executable=wine.desktop
application/x-msdos-windows-executable=wine.desktop
application/x-msi=wine.desktop
text/plain=code.desktop
text/x-python=code.desktop
text/x-csrc=code.desktop
text/x-chdr=code.desktop
text/x-c++src=code.desktop
text/x-c++hdr=code.desktop
text/x-makefile=code.desktop
text/x-shellscript=code.desktop
text/markdown=code.desktop
application/json=code.desktop
EOF
update-desktop-database /usr/share/applications || true

cat > /etc/os-release <<EOF
PRETTY_NAME="Carlinho Linux 1.0"
NAME="Carlinho Linux"
VERSION_ID="1.0"
VERSION="1.0"
VERSION_CODENAME=carlinho
ID=carlinho
ID_LIKE="ubuntu debian"
HOME_URL="https://carlinho.example/"
UBUNTU_CODENAME=noble
EOF
cat > /etc/lsb-release <<EOF
DISTRIB_ID=Carlinho
DISTRIB_RELEASE=1.0
DISTRIB_CODENAME=carlinho
DISTRIB_DESCRIPTION="Carlinho Linux 1.0"
EOF
echo carlinho > /etc/hostname
sed -i 's/^\(127\.0\.1\.1[[:space:]]\+\)ubuntu/\1carlinho/' /etc/hosts || true
sed -i 's/^GRUB_DISTRIBUTOR=.*/GRUB_DISTRIBUTOR="Carlinho"/' /etc/default/grub || true
printf 'Carlinho Linux 1.0 \\n \\l\n' > /etc/issue

cat > /usr/local/bin/carlinho <<'EOF'
#!/bin/bash
printf '  \033[41;97m  C  \033[0m  Carlinho Linux 1.0\n\n'
echo "  Ferramentas  : python3 · gcc / g++ · code · wine (.exe)"
echo "  Atalho       : Ctrl+Shift+Esc → Monitor do Sistema"
echo "  Executar .exe: wine programa.exe   (ou duplo clique)"
EOF
chmod +x /usr/local/bin/carlinho

cat > /etc/profile.d/carlinho.sh <<'EOF'
if [ -n "${BASH_VERSION:-}" ] && [ -n "${PS1:-}" ]; then
  printf '\033[41;97m C \033[0m \033[1mCarlinho Linux\033[0m — digite \033[1mcarlinho\033[0m para dicas.\n'
fi
EOF

apt-get clean
rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /root/.cache 2>/dev/null || true
find /var/log -type f -exec truncate -s 0 {} + 2>/dev/null || true
echo ">>> chroot Carlinho OK"
