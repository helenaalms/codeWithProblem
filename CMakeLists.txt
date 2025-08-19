cmake_minimum_required(VERSION 3.29)
project(newww)

set(CMAKE_CXX_STANDARD 20)

add_executable(newww
        main.cpp
        circuit_component.h
        circuit_component.cpp
        circuit_manager.h
        circuit_manager.cpp
        command_process.h)
target_link_libraries(newww -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer)