cmake_minimum_required( VERSION 3.16 )

project( sandbox )

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )


set( CLIENT_SOURCES
  src/main.cpp

)

#
# linux build 
#
if ( (NOT DEFINED EMSCRIPTEN) AND UNIX )

  # pull libraries from the system
  # find_package( PkgConfig REQUIRED )
  # pkg_check_modules( GLFW REQUIRED IMPORTED_TARGET glfw3 )
  # add_executable( app ${GAME_SOURCES} src/platform/desktop.cpp )
  # target_link_libraries( app PRIVATE imgui cjson cgltf glad cglm stb PkgConfig::GLFW )
  # add_custom_target( run COMMAND app DEPENDS app WORKING_DIRECTORY ${CMAKE_PROJECT_DIR} )

endif()

add_library( common INTERFACE )

#
# windows build
#
if ( (NOT DEFINED EMSCRIPTEN) AND WIN32 )

  # pull prebuilt libraries
  add_library( raylib STATIC IMPORTED )
  set_target_properties(raylib PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/libs/raylib/lib/raylib.lib)
  target_include_directories(raylib INTERFACE libs/raylib/include)

  add_executable( app ${CLIENT_SOURCES} )
  set_target_properties(app PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
  target_link_libraries( app PRIVATE common raylib opengl32 gdi32 winmm )

  # add_executable( server ${SERVER_SOURCES} )
  # set_target_properties( server PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
  # target_link_libraries( server PRIVATE common ws2_32 )

endif()

#
# emscripten build
#
if ( DEFINED EMSCRIPTEN )

  # pull prebuilt libraries
  add_library( raylib STATIC IMPORTED )
  set_target_properties( raylib PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/libs/raylib/lib/libraylib.a )
  target_include_directories( raylib INTERFACE libs/raylib/include )

  add_executable( app ${CLIENT_SOURCES} )
  target_link_libraries( app PRIVATE raylib )

  #set_target_properties( app PROPERTIES LINK_FLAGS "-sMIN_WEBGL_VERSION=2 -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s GL_ENABLE_GET_PROC_ADDRESS=1 --shell-file ${PROJECT_SOURCE_DIR}/shell.html --embed-file ../res@/" )
  set_target_properties( app PROPERTIES LINK_FLAGS "-sMIN_WEBGL_VERSION=2 -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY -s GL_ENABLE_GET_PROC_ADDRESS=1 --shell-file ${PROJECT_SOURCE_DIR}/shell.html" )
  set( CMAKE_EXECUTABLE_SUFFIX ".html" )

endif()

# common build flags
target_include_directories( common INTERFACE src )
target_compile_features( common INTERFACE cxx_std_20 )
target_compile_definitions( common INTERFACE "RELEASE=$<CONFIG:Release>" )

