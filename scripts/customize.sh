#!/bin/bash
set -e

WORK_DIR="${1:-/tmp/carlinho-build/work}"
MOUNT_DIR="/mnt/carlinho-chroot"

echo "Customizing Carlinho OS"

sudo debootstrap jammy $WORK_DIR http://archive.ubuntu.com/ubuntu/
sudo mount --bind /dev $MOUNT_DIR/dev
sudo mount --bind /proc $MOUNT_DIR/proc
sudo mount --bind /sys $MOUNT_DIR/sys
sudo mount --bind /dev/pts $MOUNT_DIR/dev/pts

sudo tee $MOUNT_DIR/tmp/customize.sh > /dev/null << 'CUSTOMIZE_EOF'
#!/bin/bash
set -e

export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get upgrade -y
apt-get install -y xubuntu-desktop xfce4 xfce4-goodies lightdm

apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    python3 \
    python3-pip \
    git \
    curl \
    wget

wget -q https://packages.microsoft.com/keys/microsoft.asc -O- | apt-key add -
add-apt-repository "deb [arch=amd64] https://packages.microsoft.com/repos/vscode stable main"
apt-get update
apt-get install -y code

apt-get install -y firefox

apt-get install -y thunar gnome-system-monitor

apt-get install -y \
    terminator \
    vim \
    htop \
    net-tools \
    unzip \
    zip

mkdir -p /usr/share/backgrounds/carlinho
cp /tmp/wallpaper.jpg /usr/share/backgrounds/carlinho/wallpaper.jpg

mkdir -p /etc/xdg/xfce4/xfconf/xfce-perchannel-xml
cat > /etc/xdg/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfce4-desktop" version="1.0">
  <property name="backdrop" type="empty">
    <property name="screen0" type="empty">
      <property name="monitor0" type="empty">
        <property name="workspace0" type="empty">
          <property name="color-style" type="int" value="0"/>
          <property name="image-style" type="int" value="5"/>
          <property name="last-image" type="string" value="/usr/share/backgrounds/carlinho/wallpaper.jpg"/>
        </property>
      </property>
    </property>
  </property>
</channel>
EOF

chmod 444 /etc/xdg/xfce4/xfconf/xfce-perchannel-xml/xfce4-desktop.xml

useradd -m -s /bin/bash carlinho
echo "carlinho:carlinho" | chpasswd
usermod -aG sudo carlinho

mkdir -p /etc/lightdm/lightdm.conf.d
cat > /etc/lightdm/lightdm.conf.d/autologin.conf << 'EOF'
[Seat:*]
autologin-user=carlinho
autologin-user-timeout=0
EOF

mkdir -p /home/carlinho/.config/Code/User
cp /tmp/vscode-settings.json /home/carlinho/.config/Code/User/settings.json
chown -R carlinho:carlinho /home/carlinho/.config

cp /tmp/dotfiles/.bashrc /home/carlinho/.bashrc
chown carlinho:carlinho /home/carlinho/.bashrc

apt-get clean
rm -rf /var/lib/apt/lists/*
rm -rf /tmp/*
CUSTOMIZE_EOF

sudo cp config/wallpaper.jpg $MOUNT_DIR/tmp/
sudo cp config/vscode-settings.json $MOUNT_DIR/tmp/
sudo mkdir -p $MOUNT_DIR/tmp/dotfiles
sudo cp config/dotfiles/.bashrc $MOUNT_DIR/tmp/dotfiles/
sudo chroot $MOUNT_DIR /tmp/customize.sh
sudo umount $MOUNT_DIR/dev/pts
sudo umount $MOUNT_DIR/dev
sudo umount $MOUNT_DIR/proc
sudo umount $MOUNT_DIR/sys

echo "Customization complete!"
