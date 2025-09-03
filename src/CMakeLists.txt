# Title: gVtCalc Project
# Author: Gino Francesco Bogo

add_library(gLIB OBJECT
    "gb_calc.c"
    "gb_utils.c"
    "gb_vt.c"
)

add_executable(gvtcalc
    "main.c"
)

target_link_libraries(gvtcalc m pthread gLIB)
