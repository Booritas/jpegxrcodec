set(LIBRARY_NAME jxrcodec)

# add_subdirectory(codec)

set(SOURCE_FILES 
	${SOURCE_FILES}
	${CMAKE_CURRENT_SOURCE_DIR}/jxrcodec.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/jxrcodec.cpp
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/algo.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/api.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/app_resource.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/cr_parse.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/cr_parse_boxed.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/cw_emit.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/cw_emit_boxed.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/dll_resource.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/file.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/file.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/flags.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/init.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/io.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/jpegxr.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/jpegxr_pixelformat.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/jxr_priv.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/qp.tab.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/qp.tab.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/qp_lexor.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/r_parse.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/r_strip.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/r_tile_frequency.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/r_tile_spatial.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/ralloc.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/stdint_minimal.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/w_emit.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/w_strip.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/w_strip.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/w_tile_frequency.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/w_tile_spatial.c
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/w_tile_spatial.h
   	${CMAKE_CURRENT_SOURCE_DIR}/codec/x_strip.c
)

add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})

if(MSVC)
target_compile_definitions(${LIBRARY_NAME} PRIVATE YY_NO_UNISTD_H _CRT_SECURE_NO_WARNINGS MEMORY_OUTPUT)
endif()