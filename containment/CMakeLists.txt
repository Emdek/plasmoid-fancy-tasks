project(fancypanel)

find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   )

set(fancypanel_SRCS Panel.cpp)

add_subdirectory(locale)

kde4_add_plugin(plasma_containment_fancypanel ${fancypanel_SRCS})
target_link_libraries(plasma_containment_fancypanel
                      ${KDE4_PLASMA_LIBS}
                      ${KDE4_KIO_LIBS}
                      )

install(TARGETS plasma_containment_fancypanel
        DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES plasma-containment-fancypanel.desktop
        DESTINATION ${SERVICES_INSTALL_DIR})
