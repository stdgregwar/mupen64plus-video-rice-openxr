cmake_minimum_required(VERSION 3.1)

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

project(mupen64plus-video-rice-oxr VERSION 0.0.1
	DESCRIPTION "CREAL's lighfield sample"
  LANGUAGES C CXX)

add_subdirectory(ext/OpenXR-SDK)

set(SRCDIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(SDL2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(png REQUIRED)

add_library(mupen64plus-video-rice-openxr SHARED
  ${SRCDIR}/liblinux/BMGImage.c
	${SRCDIR}/liblinux/BMGUtils.c
	${SRCDIR}/liblinux/bmp.c
	${SRCDIR}/liblinux/pngrw.c
  ${SRCDIR}/Blender.cpp
	${SRCDIR}/Combiner.cpp
	${SRCDIR}/Config.cpp
	${SRCDIR}/ConvertImage.cpp
	${SRCDIR}/ConvertImage16.cpp
	${SRCDIR}/Debugger.cpp
	${SRCDIR}/DeviceBuilder.cpp
	${SRCDIR}/FrameBuffer.cpp
	${SRCDIR}/GraphicsContext.cpp
	${SRCDIR}/OGLCombiner.cpp
	${SRCDIR}/OGLExtensions.cpp
	${SRCDIR}/OGLGraphicsContext.cpp
	${SRCDIR}/OGLRender.cpp
	${SRCDIR}/OGLRenderExt.cpp
	${SRCDIR}/OGLTexture.cpp
	${SRCDIR}/Render.cpp
	${SRCDIR}/RenderBase.cpp
	${SRCDIR}/RenderExt.cpp
	${SRCDIR}/RenderTexture.cpp
	${SRCDIR}/RSP_Parser.cpp
	${SRCDIR}/RSP_S2DEX.cpp
	${SRCDIR}/Texture.cpp
	${SRCDIR}/TextureFilters.cpp
	${SRCDIR}/TextureFilters_2xsai.cpp
	${SRCDIR}/TextureFilters_hq2x.cpp
	${SRCDIR}/TextureFilters_hq4x.cpp
	${SRCDIR}/TextureManager.cpp
	${SRCDIR}/VectorMath.cpp
	${SRCDIR}/Video.cpp
)

target_compile_definitions(mupen64plus-video-rice-openxr PUBLIC -DNO_ASM)

target_link_libraries(mupen64plus-video-rice-openxr PUBLIC SDL2::SDL2 png)
find_package(OpenGL)

if(WIN32)
  target_link_libraries(mupen64plus-video-rice-openxr PUBLIC OpenGL::GL)
  target_sources(mupen64plus-video-rice-openxr
    PUBLIC
    ${SRCDIR}/osal_dynamiclib_win32.c
    ${SRCDIR}/osal_files_win32.c
    )
else()
  target_link_libraries(mupen64plus-video-rice-openxr PUBLIC GL dl)
  target_sources(mupen64plus-video-rice-openxr
    PUBLIC
    ${SRCDIR}/osal_dynamiclib_unix.c
    ${SRCDIR}/osal_files_unix.c
    )
endif()

target_include_directories(mupen64plus-video-rice-openxr PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/ext/core/src/api)
