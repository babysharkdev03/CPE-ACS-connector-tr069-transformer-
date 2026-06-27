include $(TOPDIR)/rules.mk

PKG_NAME:=tr69d
PKG_VERSION:=0.2.4
PKG_RELEASE:=8
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/tr69d
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Mock TR-069 CPE daemon with Lua Transformer
  DEPENDS:=+libcurl +libxml2 +libstdcpp +libpthread +liblua +lua +ubus +libubus +libubox
endef

define Package/tr69d/description
 A C++17 mock CWMP client. TR-181 access is routed through a Lua/UCI Transformer.
endef

TR69D_PROJECT_ROOT?=

define Build/Prepare
	$(call Build/Prepare/Default)
	$(if $(TR69D_PROJECT_ROOT),,$(error TR69D_PROJECT_ROOT is required))
	$(CP) $(TR69D_PROJECT_ROOT)/CMakeLists.txt $(PKG_BUILD_DIR)/
	$(CP) $(TR69D_PROJECT_ROOT)/src $(PKG_BUILD_DIR)/
	$(CP) $(TR69D_PROJECT_ROOT)/include $(PKG_BUILD_DIR)/
endef

CMAKE_OPTIONS += -DBUILD_TESTING=OFF -DTR69_REQUIRE_UBUS=ON

define Package/tr69d/conffiles
/etc/config/tr69
/etc/transformer/maps/Device.map
endef

define Package/tr69d/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/tr69d $(1)/usr/bin/tr69d
	$(INSTALL_BIN) $(TR69D_PROJECT_ROOT)/openwrt/files/usr/bin/trg $(1)/usr/bin/trg
	$(INSTALL_BIN) $(TR69D_PROJECT_ROOT)/openwrt/files/usr/bin/trs $(1)/usr/bin/trs
	$(INSTALL_DIR) $(1)/usr/lib/tr69d
	$(INSTALL_DATA) $(TR69D_PROJECT_ROOT)/transformer/transformer.lua $(1)/usr/lib/tr69d/transformer.lua
	$(INSTALL_DATA) $(TR69D_PROJECT_ROOT)/transformer/cli.lua $(1)/usr/lib/tr69d/cli.lua
	$(INSTALL_DIR) $(1)/etc/transformer/maps
	$(INSTALL_CONF) $(TR69D_PROJECT_ROOT)/transformer/maps/Device.map $(1)/etc/transformer/maps/Device.map
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) $(TR69D_PROJECT_ROOT)/openwrt/files/etc/config/tr69 $(1)/etc/config/tr69
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(TR69D_PROJECT_ROOT)/openwrt/files/etc/init.d/tr69 $(1)/etc/init.d/tr69
endef

$(eval $(call BuildPackage,tr69d))
