cmake_minimum_required(VERSION 3.10)

# set(COMPONENT_ADD_INCLUDEDIRS csrc)
file(GLOB COMPONENT_SRCS ${CMAKE_CURRENT_LIST_DIR}/csrc/*.c)
file(GLOB COMPONENT_HDRS ${CMAKE_CURRENT_LIST_DIR}/csrc/*.h)

# if(COMMAND idf_component_register)
#     idf_component_register(SRCS "${COMPONENT_SRCS}" INCLUDE_DIRS csrc)
#     return()
# endif()

project(u8g2)

add_library(u8g2 ${COMPONENT_SRCS})
target_include_directories(u8g2 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/csrc)
target_sources(u8g2 PRIVATE ${COMPONENT_SRCS})

# install(TARGETS u8g2
#         EXPORT u8g2-targets
#         ARCHIVE DESTINATION lib)

# install(FILES ${COMPONENT_HDRS}
#         DESTINATION include/u8g2)

# install(EXPORT u8g2-targets
#         DESTINATION lib/cmake/u8g2)

# install(FILES ${u8g2_SOURCE_DIR}/pkg/u8g2-config.cmake
#         DESTINATION lib/cmake/u8g2)
