# =====================================================================
#  main 组件（App 层）：工程入口 app_main()
#
#  为什么这层叫 main 而不是 App：
#  ESP-IDF 强制入口组件位于工程根的 main/ 目录（见
#  $IDF_PATH/tools/cmake/project.cmake 里对 "${PROJECT_DIR}/main" 的检查），
#  所以它对应 ST407_Demo 的 App/ 层，只是目录名由 IDF 规定。
#
#  REQUIRES BSP 会自动带入板级层（BSP/Inc）的头文件路径与依赖
# =====================================================================
idf_component_register(
    SRCS        "main.c"
    INCLUDE_DIRS "."
    REQUIRES    BSP
)
