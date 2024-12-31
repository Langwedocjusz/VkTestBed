cmake_minimum_required(VERSION 3.30.2)

project(tinyexr)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        ./miniz/miniz.h
        ./miniz/miniz.c
)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ./include)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ./miniz)