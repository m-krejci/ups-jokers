cmake_minimum_required(VERSION 3.25)

project(buildCMake)

# Build typ: Debug (pokud uživatel nenastaví jiný)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Ladicí flagy
set(CMAKE_C_FLAGS_DEBUG "-g -O0")

# výstupní adresář pro .exe
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(${PROJECT_NAME}
    main.c 
    server_manager.c 
    server_manager.h
    client_manager.c
    client_manager.h
    protocol.h 
    protocol.c
    config.h
    room_manager.c
    room_manager.h 
    game_manager.h
    game_manager.c
    logger.h
    logger.c
)