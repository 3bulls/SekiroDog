add_executable(SekiroDog WIN32
    src/main.cpp
    src/Graphics.cpp
    src/SkinnedModel.cpp
    src/Environment.cpp
    src/Player.cpp
    src/ThirdPersonCamera.cpp
)

target_link_libraries(SekiroDog PRIVATE DirectXTK assimp::assimp d3d11 dxgi d3dcompiler)

# 着色器在运行时编译，放进工程里方便编辑（不参与 MSBuild 编译）
file(GLOB SEKIRO_SHADERS CONFIGURE_DEPENDS shaders/*.hlsl shaders/*.hlsli)
target_sources(SekiroDog PRIVATE ${SEKIRO_SHADERS})
set_source_files_properties(${SEKIRO_SHADERS} PROPERTIES HEADER_FILE_ONLY ON)
source_group("Shaders" FILES ${SEKIRO_SHADERS})

set_target_properties(SekiroDog PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

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
