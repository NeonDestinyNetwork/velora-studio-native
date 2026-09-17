cmake_minimum_required(VERSION 3.20)
project(VeloraStudioEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
    add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)
    set(SOURCES
        main.cpp
        D3D11Renderer.cpp
        D3D11Renderer.h
        CaptureEngine.cpp
        CaptureEngine.h
        AudioMixer.cpp
        AudioMixer.h
        HardwareEncoder.cpp
        HardwareEncoder.h
        OutputRouter.cpp
        OutputRouter.h
    )
    add_executable(VeloraStudioEngine WIN32 ${SOURCES})
    target_link_libraries(VeloraStudioEngine PRIVATE
        d3d11
        dxgi
        d3dcompiler
        ole32
        user32
        gdi32
        shell32
    )
else()
    # Linux Native Engine Build Target
    set(SOURCES
        main.cpp
        HardwareEncoder.cpp
        HardwareEncoder.h
        OutputRouter.cpp
        OutputRouter.h
    )
    add_executable(VeloraStudioEngine ${SOURCES})
    target_link_libraries(VeloraStudioEngine PRIVATE
        pthread
        dl
    )
endif()
