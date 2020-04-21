# stm32xx-hal-master
Common code for stm32xx boards
# Stm32 hal wrapper library

Stm32 hal wrapper library based on official stm32xx Cube software.
Used to be a part for boot32 firmware, which is located here : https://github.com/cornway/stm327xx_iap

Can be built with Keil MDK and arm-gcc; only by boot32 root Makefile or as boot32 Keil target.\
Now it is not a stand-alone library and requires some stuff from other repos.\
Now it supports only stm32f769 Discovery board.

Note: Contains patched Cube library.\
Patches used to cover :\
Gamepad via usb, not all models work.\
HDMI monitors, with EDID data read-back.\
Some graphic stuff, such as scaling with DMA2D.
