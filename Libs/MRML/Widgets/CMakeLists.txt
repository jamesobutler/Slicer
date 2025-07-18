project(qMRMLWidgets)


#-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20.6...3.22.6 FATAL_ERROR)
#-----------------------------------------------------------------------------

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
if(NOT DEFINED BUILD_SHARED_LIBS)
  option(BUILD_SHARED_LIBS "Build with shared libraries." ON)
endif()

if(NOT DEFINED MRML_BUILD_QT_DESIGNER_PLUGINS)
  option(MRML_BUILD_QT_DESIGNER_PLUGINS "Build Qt designer plugins." ON)
endif()

include(CheckIncludeFiles)
check_include_files("stdint.h" HAVE_STDINT_H)

# CMake module
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${MRMLWidgets_SOURCE_DIR}/CMake")

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------


#
# CTK
#
set(CTK_LIBRARIES CTKVisualizationVTKWidgets)
if(Slicer_USE_PYTHONQT)
  list(APPEND CTK_LIBRARIES
    CTKScriptingPythonWidgets
    )
endif()
if(Slicer_USE_QtTesting)
  list(APPEND CTK_LIBRARIES
    CTKQtTesting
    )
endif()

# --------------------------------------------------------------------------
# Configure headers
# --------------------------------------------------------------------------
set(MRML_WIDGETS_HAVE_QT5 1)
if(NOT DEFINED MRML_WIDGETS_HAVE_WEBENGINE_SUPPORT)
  set(MRML_WIDGETS_HAVE_WEBENGINE_SUPPORT 1)
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/qMRMLWidgetsConfigure.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/qMRMLWidgetsConfigure.h
  )

#-----------------------------------------------------------------------------
# Sources
#
set(MRMLWidgets_SRCS
  qMRMLAbstractViewWidget.cxx
  qMRMLAbstractViewWidget.h
  qMRMLCheckableNodeComboBox.cxx
  qMRMLCheckableNodeComboBox.h
  qMRMLClipNodeWidget.cxx
  qMRMLClipNodeWidget.h
  qMRMLClipNodeDisplayWidget.cxx
  qMRMLClipNodeDisplayWidget.h
  qMRMLCollapsibleButton.cxx
  qMRMLCollapsibleButton.h
  qMRMLColorModel.cxx
  qMRMLColorModel.h
  qMRMLColorModel_p.h
  qMRMLColors.cxx
  qMRMLColors.h
  qMRMLColorTableComboBox.cxx
  qMRMLColorTableComboBox.h
  qMRMLCoordinatesWidget.cxx
  qMRMLCoordinatesWidget.h
  qMRMLDisplayNodeWidget.cxx
  qMRMLDisplayNodeWidget.h
  qMRMLDisplayNodeViewComboBox.cxx
  qMRMLDisplayNodeViewComboBox.h
  qMRMLLabelComboBox.h
  qMRMLLabelComboBox.cxx
  qMRMLEventBrokerConnection.cxx
  qMRMLEventBrokerConnection.h
  qMRMLEventBrokerWidget.cxx
  qMRMLEventBrokerWidget.h
  qMRMLEventLogger.cxx
  qMRMLEventLogger.h
  qMRMLEventLoggerWidget.cxx
  qMRMLEventLoggerWidget.h
  qMRMLItemDelegate.cxx
  qMRMLItemDelegate.h
  qMRMLLayoutManager.cxx
  qMRMLLayoutManager.h
  qMRMLLayoutManager_p.h
  qMRMLLayoutViewFactory.cxx
  qMRMLLayoutViewFactory.h
  qMRMLLayoutWidget.cxx
  qMRMLLayoutWidget.h
  qMRMLLinearTransformSlider.cxx
  qMRMLLinearTransformSlider.h
  qMRMLListWidget.cxx
  qMRMLListWidget.h
  qMRMLMatrixWidget.cxx
  qMRMLMatrixWidget.h
  qMRMLModelInfoWidget.cxx
  qMRMLModelInfoWidget.h
  qMRMLNavigationView.cxx
  qMRMLNavigationView.h
  qMRMLNodeAttributeTableView.cxx
  qMRMLNodeAttributeTableView.h
  qMRMLNodeAttributeTableWidget.cxx
  qMRMLNodeAttributeTableWidget.h
  qMRMLNodeComboBox.cxx
  qMRMLNodeComboBox.h
  qMRMLNodeComboBoxDelegate.cxx
  qMRMLNodeComboBoxDelegate.h
  qMRMLNodeComboBoxMenuDelegate.cxx
  qMRMLNodeComboBoxMenuDelegate.h
  qMRMLNodeComboBox_p.h
  qMRMLNodeFactory.cxx
  qMRMLNodeFactory.h
  qMRMLPlotWidget.cxx
  qMRMLPlotWidget.h
  qMRMLPlotView.cxx
  qMRMLPlotView_p.h
  qMRMLPlotView.h
  qMRMLPlotViewControllerWidget.cxx
  qMRMLPlotViewControllerWidget.h
  qMRMLPlotViewControllerWidget_p.h
  qMRMLRangeWidget.cxx
  qMRMLRangeWidget.h
  qMRMLROIWidget.cxx
  qMRMLROIWidget.h
  qMRMLScalarInvariantComboBox.cxx
  qMRMLScalarInvariantComboBox.h
  qMRMLScalarsDisplayWidget.cxx
  qMRMLScalarsDisplayWidget.h
  qMRMLSceneCategoryModel.cxx
  qMRMLSceneCategoryModel.h
  qMRMLSceneColorTableModel.cxx
  qMRMLSceneColorTableModel.h
  qMRMLSceneFactoryWidget.cxx
  qMRMLSceneFactoryWidget.h
  qMRMLSceneModel.cxx
  qMRMLSceneModel.h
  qMRMLSceneTransformModel.cxx
  qMRMLSceneTransformModel.h
  qMRMLSceneHierarchyModel.cxx
  qMRMLSceneHierarchyModel.h
  qMRMLSceneDisplayableModel.cxx
  qMRMLSceneDisplayableModel.h
  qMRMLScreenShotDialog.cxx
  qMRMLScreenShotDialog.h
  qMRMLSegmentSelectorWidget.cxx
  qMRMLSegmentSelectorWidget.h
  qMRMLSliceControllerWidget.cxx
  qMRMLSliceControllerWidget.h
  qMRMLSliceControllerWidget_p.h
  qMRMLSliceInformationWidget.cxx
  qMRMLSliceInformationWidget.h
  qMRMLSliceInformationWidget_p.h
  qMRMLSliceVerticalControllerWidget.cxx
  qMRMLSliceVerticalControllerWidget.h
  qMRMLSliceView.cxx
  qMRMLSliceView.h
  qMRMLSliceView_p.h
  qMRMLSliceWidget.cxx
  qMRMLSliceWidget.h
  qMRMLSliceWidget_p.h
  qMRMLSliderWidget.cxx
  qMRMLSliderWidget.h
  qMRMLSortFilterColorProxyModel.cxx
  qMRMLSortFilterColorProxyModel.h
  qMRMLSortFilterHierarchyProxyModel.cxx
  qMRMLSortFilterHierarchyProxyModel.h
  qMRMLSortFilterProxyModel.cxx
  qMRMLSortFilterProxyModel.h
  qMRMLSpinBox.cxx
  qMRMLSpinBox.h
  qMRMLTableModel.cxx
  qMRMLTableModel.h
  qMRMLTableView.cxx
  qMRMLTableView.h
  qMRMLTableView_p.h
  qMRMLTableViewControllerWidget.cxx
  qMRMLTableViewControllerWidget.h
  qMRMLTableViewControllerWidget_p.h
  qMRMLTableWidget.cxx
  qMRMLTableWidget.h
  qMRMLThreeDView.cxx
  qMRMLThreeDView.h
  qMRMLThreeDView_p.h
  qMRMLThreeDViewControllerWidget.cxx
  qMRMLThreeDViewControllerWidget.h
  qMRMLThreeDViewControllerWidget_p.h
  qMRMLThreeDViewInformationWidget.cxx
  qMRMLThreeDViewInformationWidget.h
  qMRMLThreeDViewInformationWidget_p.h
  qMRMLThreeDWidget.cxx
  qMRMLThreeDWidget.h
  qMRMLTransformSliders.cxx
  qMRMLTransformSliders.h
  qMRMLTreeView.cxx
  qMRMLTreeView.h
  qMRMLTreeView_p.h
  qMRMLUtils.cxx
  qMRMLUtils.h
  qMRMLViewControllerBar.cxx
  qMRMLViewControllerBar.h
  qMRMLViewControllerBar_p.h
  qMRMLVolumeInfoWidget.cxx
  qMRMLVolumeInfoWidget.h
  qMRMLVolumeThresholdWidget.cxx
  qMRMLVolumeThresholdWidget.h
  qMRMLVolumeWidget.cxx
  qMRMLVolumeWidget.h
  qMRMLWidget.cxx
  qMRMLWidget.h
  qMRMLWindowLevelWidget.cxx
  qMRMLWindowLevelWidget.h
  )

if(MRML_WIDGETS_HAVE_WEBENGINE_SUPPORT)
  list(APPEND MRMLWidgets_SRCS
    qMRMLExpandingWebView.cxx
    qMRMLExpandingWebView.h
    qMRMLExpandingWebView_p.h
    )
endif()

# Headers that should run through moc
set(MRMLWidgets_MOC_SRCS
  qMRMLAbstractViewWidget.h
  qMRMLCheckableNodeComboBox.h
  qMRMLClipNodeWidget.h
  qMRMLClipNodeDisplayWidget.h
  qMRMLCollapsibleButton.h
  qMRMLColorModel.h
  qMRMLColorTableComboBox.h
  qMRMLCoordinatesWidget.h
  qMRMLDisplayNodeViewComboBox.h
  qMRMLDisplayNodeWidget.h
  qMRMLEventBrokerConnection.h
  qMRMLEventBrokerWidget.h
  qMRMLEventLogger.h
  qMRMLEventLoggerWidget.h
  qMRMLItemDelegate.h
  qMRMLLabelComboBox.h
  qMRMLLayoutManager.h
  qMRMLLayoutManager_p.h
  qMRMLLayoutViewFactory.h
  qMRMLLayoutWidget.h
  qMRMLLinearTransformSlider.h
  qMRMLListWidget.h
  qMRMLMatrixWidget.h
  qMRMLModelInfoWidget.h
  qMRMLNavigationView.h
  qMRMLNodeAttributeTableView.h
  qMRMLNodeAttributeTableWidget.h
  qMRMLNodeComboBox.h
  qMRMLNodeComboBoxDelegate.h
  qMRMLNodeComboBoxMenuDelegate.h
  qMRMLNodeFactory.h
  qMRMLPlotWidget.h
  qMRMLPlotViewControllerWidget.h
  qMRMLPlotViewControllerWidget_p.h
  qMRMLPlotView_p.h
  qMRMLPlotView.h
  qMRMLRangeWidget.h
  qMRMLROIWidget.h
  qMRMLScalarInvariantComboBox.h
  qMRMLScalarsDisplayWidget.h
  qMRMLSceneCategoryModel.h
  qMRMLSceneColorTableModel.h
  qMRMLSceneFactoryWidget.h
  qMRMLSceneModel.h
  qMRMLSceneTransformModel.h
  qMRMLSceneHierarchyModel.h
  qMRMLSceneDisplayableModel.h
  qMRMLScreenShotDialog.h
  qMRMLSegmentSelectorWidget.h
  qMRMLSliceControllerWidget.h
  qMRMLSliceControllerWidget_p.h
  qMRMLSliceInformationWidget.h
  qMRMLSliceInformationWidget_p.h
  qMRMLSliceVerticalControllerWidget.h
  qMRMLSliceView.h
  qMRMLSliceView_p.h
  qMRMLSliceWidget.h
  qMRMLSliceWidget_p.h
  qMRMLSliderWidget.h
  qMRMLSortFilterColorProxyModel.h
  qMRMLSortFilterHierarchyProxyModel.h
  qMRMLSortFilterProxyModel.h
  qMRMLSpinBox.h
  qMRMLTableModel.h
  qMRMLTableView.h
  qMRMLTableView_p.h
  qMRMLTableViewControllerWidget.h
  qMRMLTableViewControllerWidget_p.h
  qMRMLTableWidget.h
  qMRMLThreeDView.h
  qMRMLThreeDView_p.h
  qMRMLThreeDViewControllerWidget.h
  qMRMLThreeDViewInformationWidget.h
  qMRMLThreeDViewInformationWidget_p.h
  qMRMLThreeDWidget.h
  qMRMLTransformSliders.h
  qMRMLTreeView.h
  qMRMLUtils.h
  qMRMLViewControllerBar.h
  qMRMLViewControllerBar_p.h
  qMRMLVolumeInfoWidget.h
  qMRMLVolumeThresholdWidget.h
  qMRMLVolumeWidget.h
  qMRMLVolumeWidget_p.h
  qMRMLWidget.h
  qMRMLWindowLevelWidget.h
  )

if(MRML_WIDGETS_HAVE_WEBENGINE_SUPPORT)
  list(APPEND MRMLWidgets_MOC_SRCS
    qMRMLExpandingWebView.h
    qMRMLExpandingWebView_p.h
    )
endif()

# UI files
set(MRMLWidgets_UI_SRCS
  Resources/UI/qMRMLClipNodeWidget.ui
  Resources/UI/qMRMLClipNodeDisplayWidget.ui
  Resources/UI/qMRMLDisplayNodeWidget.ui
  Resources/UI/qMRMLEventLoggerWidget.ui
  Resources/UI/qMRMLModelInfoWidget.ui
  Resources/UI/qMRMLNodeAttributeTableView.ui
  Resources/UI/qMRMLNodeAttributeTableWidget.ui
  Resources/UI/qMRMLPlotViewControllerWidget.ui
  Resources/UI/qMRMLROIWidget.ui
  Resources/UI/qMRMLScalarsDisplayWidget.ui
  Resources/UI/qMRMLSceneFactoryWidget.ui
  Resources/UI/qMRMLScreenShotDialog.ui
  Resources/UI/qMRMLSegmentSelectorWidget.ui
  Resources/UI/qMRMLSliceControllerWidget.ui
  Resources/UI/qMRMLSliceInformationWidget.ui
  Resources/UI/qMRMLSliceVerticalControllerWidget.ui
  Resources/UI/qMRMLSliceWidget.ui
  Resources/UI/qMRMLTableViewControllerWidget.ui
  Resources/UI/qMRMLThreeDViewControllerWidget.ui
  Resources/UI/qMRMLThreeDViewInformationWidget.ui
  Resources/UI/qMRMLTransformSliders.ui
  Resources/UI/qMRMLVolumeInfoWidget.ui
  Resources/UI/qMRMLVolumeThresholdWidget.ui
  Resources/UI/qMRMLWindowLevelWidget.ui
  )

set(MRMLWidgets_QRC_SRCS
  Resources/qMRMLWidgets.qrc
  )

if(Slicer_USE_QtTesting)
  list(APPEND MRMLWidgets_SRCS
    qMRMLCheckableNodeComboBoxEventPlayer.cpp
    qMRMLCheckableNodeComboBoxEventPlayer.h
    qMRMLNodeComboBoxEventPlayer.cpp
    qMRMLNodeComboBoxEventPlayer.h
    qMRMLNodeComboBoxEventTranslator.cpp
    qMRMLNodeComboBoxEventTranslator.h
    qMRMLTreeViewEventPlayer.cpp
    qMRMLTreeViewEventPlayer.h
    qMRMLTreeViewEventTranslator.cpp
    qMRMLTreeViewEventTranslator.h
    )

  list(APPEND MRMLWidgets_MOC_SRCS
    qMRMLCheckableNodeComboBoxEventPlayer.h
    qMRMLNodeComboBoxEventPlayer.h
    qMRMLNodeComboBoxEventTranslator.h
    qMRMLTreeViewEventPlayer.h
    qMRMLTreeViewEventTranslator.h
    )
endif()

  set(_moc_options OPTIONS -DMRML_WIDGETS_HAVE_QT5)
  if(MRML_WIDGETS_HAVE_WEBENGINE_SUPPORT)
    list(APPEND _moc_options -DMRML_WIDGETS_HAVE_WEBENGINE_SUPPORT)
  endif()
  QT5_WRAP_CPP(MRMLWidgets_MOC_CXX ${MRMLWidgets_MOC_SRCS} ${_moc_options})
  QT5_WRAP_UI(MRMLWidgets_UI_CXX ${MRMLWidgets_UI_SRCS})
  QT5_ADD_RESOURCES(MRMLWidgets_QRC_CXX ${MRMLWidgets_QRC_SRCS})

source_group("Resources" FILES
  ${MRMLWidgets_UI_SRCS}
  ${MRMLWidgets_QRC_SRCS}
  )

source_group("Generated" FILES
  ${MRMLWidgets_MOC_CXX}
  ${MRMLWidgets_UI_CXX}
  ${MRMLWidgets_QRC_CXX}
  )

# --------------------------------------------------------------------------
# Include dirs
# --------------------------------------------------------------------------
set(include_dirs
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${MRMLCore_INCLUDE_DIRS}
  ${MRMLLogic_INCLUDE_DIRS}
  ${MRMLDisplayableManager_INCLUDE_DIRS}
  )
include_directories(${include_dirs})

# --------------------------------------------------------------------------
# Install headers
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_INSTALL_NO_DEVELOPMENT)
  set(${PROJECT_NAME}_INSTALL_NO_DEVELOPMENT ON)
endif()
if(NOT ${PROJECT_NAME}_INSTALL_NO_DEVELOPMENT)
  file(GLOB headers "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
  install(
    FILES ${headers}
    DESTINATION include/${PROJECT_NAME} COMPONENT Development)
endif()

  # --------------------------------------------------------------------------
  # Translation
  # --------------------------------------------------------------------------
  if(Slicer_BUILD_I18N_SUPPORT)
    set(TS_DIR
      "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Translations/"
    )

    include(SlicerMacroTranslation)
    SlicerMacroTranslation(
      SRCS ${MRMLWidgets_SRCS}
      UI_SRCS ${MRMLWidgets_UI_SRCS}
      TS_DIR ${TS_DIR}
      TS_BASEFILENAME ${PROJECT_NAME}
      TS_LANGUAGES ${Slicer_LANGUAGES}
      QM_OUTPUT_DIR_VAR QM_OUTPUT_DIR
      QM_OUTPUT_FILES_VAR QM_OUTPUT_FILES
      )

    set_property(GLOBAL APPEND PROPERTY Slicer_QM_OUTPUT_DIRS ${QM_OUTPUT_DIR})
  else()
    set(QM_OUTPUT_FILES )
  endif()

# --------------------------------------------------------------------------
# Build library
# --------------------------------------------------------------------------
set(lib_name ${PROJECT_NAME})
set(targets_to_export ${lib_name})

add_library(${lib_name}
  ${MRMLWidgets_SRCS}
  ${MRMLWidgets_MOC_CXX}
  ${MRMLWidgets_UI_CXX}
  ${MRMLWidgets_QRC_CXX}
  ${QM_OUTPUT_FILES}
  )

set(MRMLWidgets_LIBRARIES
  ${QT_LIBRARIES}
  ${CTK_LIBRARIES}
  MRMLLogic
  MRMLDisplayableManager
  VTK::RenderingQt
  )

target_link_libraries(${lib_name} ${MRMLWidgets_LIBRARIES})

# Apply user-defined properties to the library target.
if(Slicer_LIBRARY_PROPERTIES)
  set_target_properties(${lib_name} PROPERTIES ${Slicer_LIBRARY_PROPERTIES})
endif()

# --------------------------------------------------------------------------
# Folder
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_FOLDER)
  set(${PROJECT_NAME}_FOLDER ${PROJECT_NAME})
endif()
if(NOT "${${PROJECT_NAME}_FOLDER}" STREQUAL "")
  set_target_properties(${lib_name} PROPERTIES FOLDER ${${PROJECT_NAME}_FOLDER})
endif()

# --------------------------------------------------------------------------
# PythonQt wrapping
# --------------------------------------------------------------------------
if(MRMLWidgets_WRAP_PYTHON)
  ctkMacroBuildLibWrapper(
    NAMESPACE "org.slicer.libs"
    TARGET ${lib_name}
    SRCS "${MRMLWidgets_SRCS}"
    INSTALL_BIN_DIR ${Slicer_INSTALL_BIN_DIR}
    INSTALL_LIB_DIR ${Slicer_INSTALL_LIB_DIR}
    )
  # Folder
  if(NOT "${${PROJECT_NAME}_FOLDER}" STREQUAL "")
    set_target_properties(${lib_name}PythonQt PROPERTIES FOLDER ${${PROJECT_NAME}_FOLDER})
  endif()
endif()

# --------------------------------------------------------------------------
# Export target
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_EXPORT_FILE)
  set(${PROJECT_NAME}_EXPORT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake)
endif()
export(TARGETS ${targets_to_export} APPEND FILE ${${PROJECT_NAME}_EXPORT_FILE})

# --------------------------------------------------------------------------
# Install library
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_INSTALL_BIN_DIR)
  set(${PROJECT_NAME}_INSTALL_BIN_DIR bin)
endif()
if(NOT DEFINED ${PROJECT_NAME}_INSTALL_LIB_DIR)
  set(${PROJECT_NAME}_INSTALL_LIB_DIR lib/${PROJECT_NAME})
endif()

install(TARGETS ${lib_name}
  RUNTIME DESTINATION ${${PROJECT_NAME}_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${${PROJECT_NAME}_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${${PROJECT_NAME}_INSTALL_LIB_DIR} COMPONENT Development
  )

# --------------------------------------------------------------------------
# Testing
# --------------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()

# --------------------------------------------------------------------------
# Qt Designer Plugins
# --------------------------------------------------------------------------
if(MRML_BUILD_QT_DESIGNER_PLUGINS)
  add_subdirectory(DesignerPlugins)
endif()

# --------------------------------------------------------------------------
# Set INCLUDE_DIRS variable
# --------------------------------------------------------------------------
set(${PROJECT_NAME}_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
  CACHE INTERNAL "${PROJECT_NAME} include dirs" FORCE)
