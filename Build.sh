#!/usr/bin/env bash

buildtype=""

PS3="Select build type:"
options=("Debug" "Release")
select opt in "${options[@]}"

do
  case $opt in
    "Debug")
      buildtype="-DCMAKE_BUILD_TYPE=Debug"
      break
      ;;
    "Release")
      buildtype="-DCMAKE_BUILD_TYPE=Release"
      break
      ;;
    *) echo "invalid option $REPLY";;
  esac
done

cmake -S . -B ./build "${buildtype}"
(cd build && make -j)