cmake_minimum_required(VERSION 3.30.2)

project(tiny_ktx)

add_library(tiny_ktx INTERFACE)
target_include_directories(tiny_ktx SYSTEM INTERFACE ./include)