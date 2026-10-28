# =====================================================================
#  BSP 层（板级层）：延时 + LED
#  仅 Src/ 参与编译，Inc/ 对依赖方公开；
#  ESP-IDF 的驱动组件只放进 PRIV_REQUIRES，不污染 App 层
# =====================================================================
# ESP-IDF 5.3 起 GPIO 驱动拆成独立组件 esp_driver_gpio；
# 更早的 5.x 没有该组件，回退到包含全部驱动的 driver 组件，保证旧版本也能编译。
if(EXISTS "$ENV{IDF_PATH}/components/esp_driver_gpio")
    set(BSP_GPIO_COMPONENT esp_driver_gpio)
else()
    set(BSP_GPIO_COMPONENT driver)
endif()

idf_component_register(
    SRCS        "Src/bsp_delay.c"
                "Src/bsp_led.c"
    INCLUDE_DIRS "Inc"
    PRIV_REQUIRES ${BSP_GPIO_COMPONENT}
                  esp_timer
)
