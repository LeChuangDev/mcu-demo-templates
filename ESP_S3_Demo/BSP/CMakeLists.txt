# =====================================================================
#  BSP 层（板级层）：延时 + LED + PWM
#  仅 Src/ 参与编译，Inc/ 对依赖方公开；Src/bsp_config.h 是板级配置文件
#  ESP-IDF 的驱动组件只放进 PRIV_REQUIRES，不污染 App 层
# =====================================================================
# ESP-IDF 5.3 起 GPIO / LEDC 驱动拆成独立组件；
# 更早的 5.x 没有这些组件，回退到包含全部驱动的 driver 组件，保证旧版本也能编译。
if(EXISTS "$ENV{IDF_PATH}/components/esp_driver_gpio")
    set(BSP_GPIO_COMPONENT esp_driver_gpio)
else()
    set(BSP_GPIO_COMPONENT driver)
endif()

if(EXISTS "$ENV{IDF_PATH}/components/esp_driver_ledc")
    set(BSP_LEDC_COMPONENT esp_driver_ledc)
else()
    set(BSP_LEDC_COMPONENT driver)
endif()

set(BSP_DRIVER_COMPONENTS ${BSP_GPIO_COMPONENT} ${BSP_LEDC_COMPONENT})
list(REMOVE_DUPLICATES BSP_DRIVER_COMPONENTS)

idf_component_register(
    SRCS        "Src/bsp_delay.c"
                "Src/bsp_led.c"
                "Src/bsp_pwm.c"
    INCLUDE_DIRS "Inc"
    PRIV_REQUIRES ${BSP_DRIVER_COMPONENTS}
                  esp_timer
                  log
)
