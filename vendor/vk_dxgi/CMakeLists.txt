cmake_minimum_required(VERSION 3.30.2)

project(vk_dxgi)

add_library(vk_dxgi INTERFACE)
target_include_directories(vk_dxgi SYSTEM INTERFACE ./include)