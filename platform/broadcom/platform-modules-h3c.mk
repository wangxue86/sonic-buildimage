# h3c Platform modules


H3C_S6850_48Y8C_W1_PLATFORM_MODULE_VERSION=1.0
export H3C_S6850_48Y8C_W1_PLATFORM_MODULE_VERSION

H3C_S6850_48Y8C_W1_PLATFORM_MODULE = platform-modules-h3c-s6850-48y8c-w1_$(H3C_S6850_48Y8C_W1_PLATFORM_MODULE_VERSION)_amd64.deb
$(H3C_S6850_48Y8C_W1_PLATFORM_MODULE)_SRC_PATH = $(PLATFORM_PATH)/sonic-platform-modules-h3c
$(H3C_S6850_48Y8C_W1_PLATFORM_MODULE)_DEPENDS += $(LINUX_HEADERS) $(LINUX_HEADERS_COMMON)
$(H3C_S6850_48Y8C_W1_PLATFORM_MODULE)_PLATFORM = x86_64-h3c_s6850-48y8c-w1-r0
SONIC_DPKG_DEBS += $(H3C_S6850_48Y8C_W1_PLATFORM_MODULE)

SONIC_STRETCH_DEBS += $(H3C_S6850_48Y8C_W1_PLATFORM_MODULE)
