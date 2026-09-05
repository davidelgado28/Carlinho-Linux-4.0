# Carlinho-Linux-4.0

A custom Linux distribution built specifically for developers, featuring automated building via GitHub Actions, essential programming tools, and a unique custom background.

## About the Creator

This project was created by **David Carlos Miranda Delgado**, a student in the Technical Course in Informatics at **IFSULDEMINAS - Campus Poços de Caldas**.

## Features

* **Development Ready:** Pre-configured with essential tools such as Git, Docker, Node.js, Python, Neovim, Zsh, and tmux.
* **Customized Environment:** Includes a unique developer background image and enforced desktop configuration.
* **Automated Builds:** Fully automated ISO generation pipeline using GitHub Actions.

## Repository Structure

```text
carlinho-linux/
├── .github/
│   └── workflows/
│       └── build.yml      # GitHub Actions workflow for automated ISO compilation
├── assets/
│   └── wallpaper.jpg      # Custom system background image
├── scripts/
│   └── build.sh           # Main build and chroot configuration script
└── README.md
