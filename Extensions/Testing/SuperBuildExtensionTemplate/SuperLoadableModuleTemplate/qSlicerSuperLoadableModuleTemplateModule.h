/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#ifndef __qSlicerSuperLoadableModuleTemplateModule_h
#define __qSlicerSuperLoadableModuleTemplateModule_h

// Slicer includes
#include "qSlicerLoadableModule.h"

#include "qSlicerSuperLoadableModuleTemplateModuleExport.h"

class qSlicerSuperLoadableModuleTemplateModulePrivate;

class Q_SLICER_QTMODULES_SUPERLOADABLEMODULETEMPLATE_EXPORT qSlicerSuperLoadableModuleTemplateModule : public qSlicerLoadableModule
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.slicer.modules.loadable.qSlicerLoadableModule/1.0");
  Q_INTERFACES(qSlicerLoadableModule);

public:
  typedef qSlicerLoadableModule Superclass;
  explicit qSlicerSuperLoadableModuleTemplateModule(QObject* parent = 0);
  ~qSlicerSuperLoadableModuleTemplateModule() override;

  qSlicerGetTitleMacro(tr("qSlicerSuperLoadableModuleTemplateModule"));

  QString helpText() const override;
  QString acknowledgementText() const override;
  QStringList contributors() const override;

  QIcon icon() const override;

  QStringList categories() const override;
  QStringList dependencies() const override;

protected:
  /// Initialize the module. Register the volumes reader/writer
  void setup() override;

  /// Create and return the widget representation associated to this module
  qSlicerAbstractModuleRepresentation* createWidgetRepresentation() override;

  /// Create and return the logic associated to this module
  vtkMRMLAbstractLogic* createLogic() override;

protected:
  QScopedPointer<qSlicerSuperLoadableModuleTemplateModulePrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerSuperLoadableModuleTemplateModule);
  Q_DISABLE_COPY(qSlicerSuperLoadableModuleTemplateModule);
};

#endif
