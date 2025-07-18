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

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

#ifndef __qMRMLThreeDViewInformationWidget_h
#define __qMRMLThreeDViewInformationWidget_h

// Qt includes
#include <QWidget>

// CTK includes
#include <ctkPimpl.h>

// qMRMLWidget includes
#include "qMRMLWidget.h"

#include "qMRMLWidgetsExport.h"

class qMRMLThreeDViewInformationWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLViewNode;

class QMRML_WIDGETS_EXPORT qMRMLThreeDViewInformationWidget : public qMRMLWidget
{
  Q_OBJECT
public:
  /// Superclass typedef
  typedef qMRMLWidget Superclass;

  /// Constructors
  explicit qMRMLThreeDViewInformationWidget(QWidget* parent = nullptr);
  ~qMRMLThreeDViewInformationWidget() override;

  /// Get \a viewNode
  vtkMRMLViewNode* mrmlViewNode() const;

public slots:

  /// Set a new viewNode.
  void setMRMLViewNode(vtkMRMLNode* newNode);

  /// Set a new SliceNode.
  void setMRMLViewNode(vtkMRMLViewNode* newSliceNode);

  /// Set view group
  void setViewGroup(int viewGroup);

protected:
  QScopedPointer<qMRMLThreeDViewInformationWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLThreeDViewInformationWidget);
  Q_DISABLE_COPY(qMRMLThreeDViewInformationWidget);
};

#endif
