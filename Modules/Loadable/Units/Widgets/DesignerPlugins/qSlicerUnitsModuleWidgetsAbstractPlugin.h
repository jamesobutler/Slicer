/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Johan Andruejol, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

#ifndef qSlicerUnitsModuleWidgetsAbstractPlugin_h
#define qSlicerUnitsModuleWidgetsAbstractPlugin_h

#include <QtGlobal>
#include <QtUiPlugin/QDesignerCustomWidgetInterface>

#include "qSlicerUnitsModuleWidgetsPluginsExport.h"

class Q_SLICER_MODULE_UNITS_WIDGETS_PLUGINS_EXPORT qSlicerUnitsModuleWidgetsAbstractPlugin : public QDesignerCustomWidgetInterface
{
  Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetInterface")
  Q_INTERFACES(QDesignerCustomWidgetInterface);

public:
  qSlicerUnitsModuleWidgetsAbstractPlugin();
  // Don't reimplement this method.
  QString group() const override;
  // You can reimplement these methods
  QIcon icon() const override;
  QString toolTip() const override;
  QString whatsThis() const override;
};

#endif
