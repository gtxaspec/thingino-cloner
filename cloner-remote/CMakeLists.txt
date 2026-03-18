# cloner-remote - daemon (Linux/macOS only, requires POSIX sockets)
if (WIN32)
    return()
endif()

add_executable(cloner-remote main.c)
target_include_directories(cloner-remote PRIVATE
    ${CMAKE_SOURCE_DIR}/libcloner/src
    ${CMAKE_SOURCE_DIR}/libcloner/src/ddr
    ${CMAKE_SOURCE_DIR}/libcloner/src/firmware
)
target_link_libraries(cloner-remote cloner z)
install(TARGETS cloner-remote DESTINATION bin)
