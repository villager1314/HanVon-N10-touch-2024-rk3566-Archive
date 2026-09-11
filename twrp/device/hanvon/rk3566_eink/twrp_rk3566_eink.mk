$(call inherit-product, $(SRC_TARGET_DIR)/product/base.mk)
$(call inherit-product, vendor/twrp/config/common.mk)
$(call inherit-product, device/hanvon/rk3566_eink/device.mk)

PRODUCT_DEVICE := rk3566_eink
PRODUCT_NAME := twrp_rk3566_eink
PRODUCT_BRAND := HANVON
PRODUCT_MODEL := N10Touch
PRODUCT_MANUFACTURER := HANVON
PRODUCT_RELEASE_NAME := rk3566_eink
PRODUCT_USE_DYNAMIC_PARTITIONS := true
