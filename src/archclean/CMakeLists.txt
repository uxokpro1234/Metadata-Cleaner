cmake_minimum_required(VERSION 3.20)

project(ArchClean
        VERSION 1.0.0
        DESCRIPTION "Native C++ JPEG and PNG metadata cleaner"
        LANGUAGES CXX
)

add_executable(archclean
        main.cpp
        jpeg.cpp
        png.cpp
)

target_compile_features(archclean PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(archclean PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(archclean PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()