# ==============================================================================
# Tank Maze Game - CMake 构建配置
# ==============================================================================
cmake_minimum_required(VERSION 3.16)
project(CS101AFinalProj VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# 基础配置
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置默认构建类型为 Release（如果未指定）
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ------------------------------------------------------------------------------
# RPATH 配置（让可执行文件能找到动态库）
# ------------------------------------------------------------------------------
if(APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
elseif(UNIX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# ------------------------------------------------------------------------------
# 第三方依赖：SFML 3.0.2
# ------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(sfml
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 3.0.2
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(sfml)

# ------------------------------------------------------------------------------
# 源文件和头文件
# ------------------------------------------------------------------------------
set(SOURCES
  # Core
  src/core/main.cpp
  src/core/Game.cpp
  # Entities
  src/entities/Tank.cpp
  src/entities/Bullet.cpp
  src/entities/HealthBar.cpp
  src/entities/Enemy.cpp
  # World
  src/world/Maze.cpp
  src/world/MazeGenerator.cpp
  # Systems
  src/systems/CollisionSystem.cpp
  src/systems/AudioManager.cpp
  # Network
  src/network/NetworkManager.cpp
  src/network/MultiplayerHandler.cpp
)

set(HEADERS
  # Core
  src/include/core/Game.hpp
  # Entities
  src/include/entities/Tank.hpp
  src/include/entities/Bullet.hpp
  src/include/entities/HealthBar.hpp
  src/include/entities/Enemy.hpp
  # World
  src/include/world/Maze.hpp
  src/include/world/MazeGenerator.hpp
  # Systems
  src/include/systems/CollisionSystem.hpp
  src/include/systems/AudioManager.hpp
  # Network
  src/include/network/NetworkManager.hpp
  src/include/network/MultiplayerHandler.hpp
  # UI
  src/include/ui/UIHelper.hpp
  src/include/ui/RoundedRectangle.hpp
  # Utils
  src/include/utils/Utils.hpp
)

# ------------------------------------------------------------------------------
# 可执行文件配置
# ------------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})


target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/include
  ${CMAKE_SOURCE_DIR}/src/include/core
  ${CMAKE_SOURCE_DIR}/src/include/entities
  ${CMAKE_SOURCE_DIR}/src/include/world
  ${CMAKE_SOURCE_DIR}/src/include/systems
  ${CMAKE_SOURCE_DIR}/src/include/network
  ${CMAKE_SOURCE_DIR}/src/include/ui
  ${CMAKE_SOURCE_DIR}/src/include/utils
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  SFML::Graphics
  SFML::Network
  SFML::Audio
)



# macOS: 链接 CoreFoundation 框架（用于获取 bundle 路径）
if(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation")
endif()

# ------------------------------------------------------------------------------
# Windows: 在 Release 配置下隐藏控制台窗口（MSVC -> /SUBSYSTEM:WINDOWS, MinGW/GCC -> -mwindows）
# 这不会修改源代码，只改变链接子系统（Debug 保持有控制台便于调试）
# ------------------------------------------------------------------------------
if(MSVC)
  target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>)
  # 保持使用标准 main() 作为入口点，避免需要 WinMain
  target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:-mwindows>)
endif()

# ------------------------------------------------------------------------------
# 应用程序图标配置
# ------------------------------------------------------------------------------
set(APP_ICON_PNG "${CMAKE_SOURCE_DIR}/tank_assets/icon.png")

if(APPLE)
  # macOS: 将 PNG 转换为 ICNS 格式
  set(APP_ICON_ICNS "${CMAKE_BINARY_DIR}/icon.icns")
  set(ICONSET_DIR "${CMAKE_BINARY_DIR}/icon.iconset")

  # 创建 iconset 并生成 icns 文件
  add_custom_command(
    OUTPUT ${APP_ICON_ICNS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ICONSET_DIR}
    COMMAND sips -z 16 16 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_16x16.png
    COMMAND sips -z 32 32 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_16x16@2x.png
    COMMAND sips -z 32 32 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_32x32.png
    COMMAND sips -z 64 64 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_32x32@2x.png
    COMMAND sips -z 128 128 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_128x128.png
    COMMAND sips -z 256 256 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_128x128@2x.png
    COMMAND sips -z 256 256 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_256x256.png
    COMMAND sips -z 512 512 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_256x256@2x.png
    COMMAND sips -z 512 512 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_512x512.png
    COMMAND sips -z 1024 1024 ${APP_ICON_PNG} --out ${ICONSET_DIR}/icon_512x512@2x.png
    COMMAND iconutil -c icns ${ICONSET_DIR} -o ${APP_ICON_ICNS}
    DEPENDS ${APP_ICON_PNG}
    COMMENT "Generating macOS icon from PNG..."
  )

  # 添加图标为自定义 target
  add_custom_target(GenerateAppIcon DEPENDS ${APP_ICON_ICNS})

  # 设置图标文件属性
  set_source_files_properties(${APP_ICON_ICNS} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
  )

elseif(WIN32)
  # Windows: 需要 .ico 文件和资源文件
  set(APP_ICON_ICO "${CMAKE_SOURCE_DIR}/resources/windows/icon.ico")

  # 生成一个包含绝对路径的 RC 文件到构建目录，以确保资源编译器能找到 ico
  set(APP_RC_IN "${CMAKE_SOURCE_DIR}/resources/windows/app.rc.in")
  set(APP_RC_FILE "${CMAKE_BINARY_DIR}/app_generated.rc")

  if(EXISTS ${APP_ICON_ICO} AND EXISTS ${APP_RC_IN})
    configure_file(${APP_RC_IN} ${APP_RC_FILE} @ONLY)
  else()
    if(NOT EXISTS ${APP_ICON_ICO})
      message(WARNING "Windows icon file not found: ${APP_ICON_ICO}")
      message(WARNING "Please convert tank_assets/icon.png to resources/windows/icon.ico")
    endif()
    if(NOT EXISTS ${APP_RC_IN})
      message(WARNING "app.rc.in template not found: ${APP_RC_IN}")
    endif()
  endif()
endif()

# ------------------------------------------------------------------------------
# 平台特定配置
# ------------------------------------------------------------------------------
if(APPLE)
  # macOS: 创建 .app 包
  set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "Tank Maze Game"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Tank Maze Game"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.cs101a.tankmaze"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    MACOSX_BUNDLE_ICON_FILE "icon.icns"
  )

  # 添加图标依赖
  add_dependencies(${PROJECT_NAME} GenerateAppIcon)

  # 复制资源文件到 app bundle
  set(RESOURCE_DIR "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/tank_assets" "${RESOURCE_DIR}/tank_assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/music_assets" "${RESOURCE_DIR}/music_assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${APP_ICON_ICNS}" "${RESOURCE_DIR}/icon.icns"
    COMMENT "Copying resources to app bundle..."
  )

elseif(WIN32)
  # Windows: 添加生成的 RC 文件以设置图标（如果存在）
  if(EXISTS ${APP_RC_FILE})
    target_sources(${PROJECT_NAME} PRIVATE ${APP_RC_FILE})
  endif()

  # Windows: 复制资源文件
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/tank_assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/tank_assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/music_assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/music_assets"
    COMMENT "Copying resources to output directory..."
  )
endif()

# ------------------------------------------------------------------------------
# CPack 打包配置
# ------------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "TankMazeGame")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "CS101A")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tank Maze Game - A tank battle game in a maze")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TankMazeGame")

# 只打包 Runtime 组件，排除开发文件和头文件
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};Runtime;/")

if(APPLE)
  # macOS: 生成 DMG 安装包
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "Tank Maze Game")
  set(CPACK_DMG_FORMAT "UDBZ") # 压缩格式
  set(CPACK_PACKAGE_ICON "${CMAKE_BINARY_DIR}/icon.icns")

  # 安装 .app bundle
  install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    COMPONENT Runtime
  )

elseif(WIN32)
  # Windows: 生成 ZIP 或 NSIS 安装包
  set(CPACK_GENERATOR "ZIP")

  install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION .
    COMPONENT Runtime
  )
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/tank_assets" DESTINATION . COMPONENT Runtime)
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/music_assets" DESTINATION . COMPONENT Runtime)
endif()

include(CPack)
