BITS 32

section .rodata
align 4
global desktop_wallpaper
desktop_wallpaper:
    incbin "assets/carlinho-webos.rgb565"
