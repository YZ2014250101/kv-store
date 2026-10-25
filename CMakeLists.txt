cmake_minimum_required(VERSION 3.10.0)
project(kv-store VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_LIST
    kvstore.cpp
    skiplist.cpp
    main.cpp
)

add_executable(main ${SRC_LIST})

