# cloner-remote - daemon for remote USB cloner access over TCP

add_executable(cloner-remote main.c)
target_include_directories(cloner-remote PRIVATE
    ${CMAKE_SOURCE_DIR}/libcloner/src
    ${CMAKE_SOURCE_DIR}/libcloner/src/ddr
    ${CMAKE_SOURCE_DIR}/libcloner/src/firmware
)
if (WIN32)
    target_link_libraries(cloner-remote cloner ws2_32)
else()
    target_link_libraries(cloner-remote cloner z)
endif()
install(TARGETS cloner-remote DESTINATION bin)
