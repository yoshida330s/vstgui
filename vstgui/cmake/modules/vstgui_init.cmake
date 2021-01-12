cmake_minimum_required(VERSION 3.5)

##########################################################################################
if(UNIX AND NOT CMAKE_HOST_APPLE)
    set(LINUX TRUE CACHE INTERNAL "VSTGUI linux platform")
endif()

##########################################################################################
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES Debug Release ReleaseLTO)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

if(CMAKE_HOST_APPLE)
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 7)
        set(VSTGUI_LTO_COMPILER_FLAGS "-O3 -flto=thin")
    else()
        set(VSTGUI_LTO_COMPILER_FLAGS "-O3 -flto")
    endif()
  set(VSTGUI_LTO_LINKER_FLAGS "")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()
if(LINUX)
    set(VSTGUI_LTO_COMPILER_FLAGS "-O3 -flto")
    set(VSTGUI_LTO_LINKER_FLAGS "")
    if(VSTGUI_WARN_EVERYTHING)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()
if(MSVC)
    set(VSTGUI_COMPILE_DEFINITIONS_DEBUG "${VSTGUI_COMPILE_DEFINITIONS_DEBUG};_CRT_SECURE_NO_WARNINGS")
    set(VSTGUI_COMPILE_DEFINITIONS_RELEASE "${VSTGUI_COMPILE_DEFINITIONS_RELEASE};_CRT_SECURE_NO_WARNINGS")
    set(VSTGUI_LTO_COMPILER_FLAGS "/GL /MP")
    set(VSTGUI_LTO_LINKER_FLAGS "/LTCG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /IGNORE:4221")
endif()

##########################################################################################
set(VSTGUI_COMPILE_DEFINITIONS_DEBUG "VSTGUI_LIVE_EDITING;DEBUG")
set(VSTGUI_COMPILE_DEFINITIONS_RELEASE "NDEBUG;RELEASE")

set(VSTGUI_COMPILE_DEFINITIONS PRIVATE
    $<$<CONFIG:Debug>:${VSTGUI_COMPILE_DEFINITIONS_DEBUG}>
    $<$<CONFIG:Release>:${VSTGUI_COMPILE_DEFINITIONS_RELEASE}>
    $<$<CONFIG:ReleaseLTO>:${VSTGUI_COMPILE_DEFINITIONS_RELEASE}>
        CACHE INTERNAL "VSTGUI compile definitions"
)

##########################################################################################
set(CMAKE_CXX_FLAGS_RELEASELTO
    "${CMAKE_CXX_FLAGS_RELEASE} ${VSTGUI_LTO_COMPILER_FLAGS}"
)
set(CMAKE_EXE_LINKER_FLAGS_RELEASELTO
    "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${VSTGUI_LTO_LINKER_FLAGS}"
)
set(CMAKE_STATIC_LINKER_FLAGS_RELEASELTO
    "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} ${VSTGUI_LTO_LINKER_FLAGS}"
)
