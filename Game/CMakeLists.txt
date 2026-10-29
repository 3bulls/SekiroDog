add_executable(SekiroDog WIN32
    src/main.cpp
    src/Graphics.cpp
    src/SkinnedModel.cpp
    src/Environment.cpp
    src/Player.cpp
    src/ThirdPersonCamera.cpp
)

target_link_libraries(SekiroDog PRIVATE DirectXTK assimp::assimp d3d11 dxgi d3dcompiler)

# 开发阶段直接从工程目录读取 Assets 和 shaders
target_compile_definitions(SekiroDog PRIVATE
    SEKIRO_ROOT="${CMAKE_SOURCE_DIR}"
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

if (MSVC)
    target_compile_options(SekiroDog PRIVATE /W4 /utf-8 /permissive-)
endif()
