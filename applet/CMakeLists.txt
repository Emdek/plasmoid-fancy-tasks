project(fancytasks)

find_package(KDE4 REQUIRED)
include(KDE4Defaults)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

set(fancytasks_SRCS Applet.cpp Icon.cpp Light.cpp Separator.cpp DropZone.cpp Task.cpp Launcher.cpp LauncherProperties.cpp RuleDelegate.cpp Job.cpp Menu.cpp Configuration.cpp ActionDelegate.cpp TriggerDelegate.cpp FindApplicationDialog.cpp)

add_subdirectory(locale)

kde4_add_ui_files(fancytasks_SRCS ui/general.ui ui/appearance.ui ui/arrangement.ui ui/actions.ui ui/findApplication.ui ui/launcherRules.ui ui/group.ui)
kde4_add_plugin(plasma_applet_fancytasks ${fancytasks_SRCS})

target_link_libraries(plasma_applet_fancytasks
	${X11_LIBRARIES}
	${KDE4_PLASMA_LIBS}
	${KDE4_KDEUI_LIBS}
	${KDE4_KUTILS_LIBS}
	${KDE4_KIO_LIBS}
	processcore
	taskmanager
	)

install(TARGETS plasma_applet_fancytasks
	DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES plasma-applet-fancytasks.desktop
	DESTINATION ${SERVICES_INSTALL_DIR})

install(FILES theme/fancytasks.svgz
	DESTINATION ${DATA_INSTALL_DIR}/desktoptheme/default/widgets/)
