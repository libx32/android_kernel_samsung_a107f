#
# Makefile for the fingerprint device.
#

# Each configuration option enables a list of files.
#include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

#ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
#subdir-ccflags-y += -I$(srctree)/drivers/spi/trustzone/spi/Tlspi/inc
#	include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
#endif
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)


obj-$(CONFIG_EGIS_FINGERPRINT)	+= et520.o
