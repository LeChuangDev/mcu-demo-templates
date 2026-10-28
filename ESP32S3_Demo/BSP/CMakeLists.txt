# =====================================================================
#  BSP 层（板级层）：延时 + LED
#  仅 Src/ 参与编译，Inc/ 对依赖方公开；
#  ESP-IDF 的驱动组件只放进 PRIV_REQUIRES，不污染 App 层
# =====================================================================
idf_component_register(
    SRCS        "Src/bsp_delay.c"
                "Src/bsp_led.c"
    INCLUDE_DIRS "Inc"
    PRIV_REQUIRES esp_driver_gpio
                  esp_timer
)
