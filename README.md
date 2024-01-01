slock - simple screen locker (fork)
===================================
A bloated fork of suckless [slock](https://tools.suckless.org/slock) with lots of customizations.


This fork merges the following patches: 
- [slock-alternate-colors](https://tools.suckless.org/slock/patches/alternate-colors/)
- [slock-blur_pixelated_screen](https://tools.suckless.org/slock/patches/blur-pixelated-screen/)
- [slock-capscolor](https://tools.suckless.org/slock/patches/capscolor/)
- [slock-control-clear](https://tools.suckless.org/slock/patches/control-clear/)
- [slock-dpms](https://tools.suckless.org/slock/patches/dpms/)
- [slock-dwmlogoandblurscreen](https://tools.suckless.org/slock/patches/dwmlogoandblurscreen/)
- [slock-failure-command](https://tools.suckless.org/slock/patches/failure-command/)
- [slock-mediakeys](https://tools.suckless.org/slock/patches/mediakeys/)
- [slock-message-xft](https://github.com/nathanielevan/slock/blob/master/slock-message-xft-20210315-ae681c5.patch)
- [slock-quickcancel](https://tools.suckless.org/slock/patches/quickcancel/)

And adds the following other changes:
- **lock icon** instead of the dwm logo (configurable)
- **quick cancel** using any keyboard key
- **configuration file**; using libconfig to loads configuration from `$XDG_CONFIG_HOME/slock/slock.cfg` if it exists


Building and installing
-----------------------

1. clone this repository locally on your machine
2. Install libconfig from your package manager
3. run `make clean && sudo make install` from within the repository folder
4. copy and edit the config file: `cp /etc/slock/slock.cfg $XDG_CONFIG_HOME/slock/slock.cfg`

Previews
--------





