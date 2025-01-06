cmake_minimum_required(VERSION 3.30.2)

project(mikktspace)

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        ./mikktspace.c
)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ./include)