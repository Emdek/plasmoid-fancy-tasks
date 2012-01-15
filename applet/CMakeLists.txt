project(fancytasks)

find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   )

if(X11_Xcomposite_FOUND AND X11_Xrender_FOUND AND X11_Xfixes_FOUND)
    set(FANCYTASKS_HAVE_COMPOSITING 1)
endif(X11_Xcomposite_FOUND AND X11_Xrender_FOUND AND X11_Xfixes_FOUND)

set(fancytasks_SRCS Applet.cpp Icon.cpp Light.cpp Separator.cpp DropZone.cpp Menu.cpp Task.cpp Launcher.cpp Job.cpp Configuration.cpp ActionDelegate.cpp)

add_subdirectory(locale)

kde4_add_ui_files(fancytasks_SRCS ui/general.ui ui/appearance.ui ui/arrangement.ui ui/actions.ui ui/findApplication.ui ui/group.ui)
kde4_add_plugin(plasma_applet_fancytasks ${fancytasks_SRCS})
target_link_libraries(plasma_applet_fancytasks
                      ${KDE4_PLASMA_LIBS}
                      ${KDE4_KDEUI_LIBS}
                      ${KDE4_KUTILS_LIBS}
                      ${KDE4_KIO_LIBS}
                      taskmanager
                      )

if(X11_FOUND)
   target_link_libraries(plasma_applet_fancytasks
                         ${X11_LIBRARIES}
                         )
endif(X11_FOUND)

if(FANCYTASKS_HAVE_COMPOSITING)
   target_link_libraries(plasma_applet_fancytasks
                         ${X11_Xcomposite_LIB}
                         ${X11_Xrender_LIB}
                         ${X11_Xfixes_LIB}
                         )
endif(FANCYTASKS_HAVE_COMPOSITING)

configure_file(config-fancytasks.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-fancytasks.h)

install(TARGETS plasma_applet_fancytasks
        DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES plasma-applet-fancytasks.desktop
        DESTINATION ${SERVICES_INSTALL_DIR})

install(FILES theme/fancytasks.svgz
        DESTINATION ${DATA_INSTALL_DIR}/desktoptheme/default/widgets/)
