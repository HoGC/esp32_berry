file(GLOB berry_srcs
          "port/*.c"
          "berry/src/*.c")

idf_component_register(SRCS ${berry_srcs}
                    INCLUDE_DIRS "port" "generate" "berry/src")
