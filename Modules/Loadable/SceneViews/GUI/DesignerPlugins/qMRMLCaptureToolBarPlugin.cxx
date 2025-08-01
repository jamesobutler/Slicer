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

  This file was originally developed by Julien Finet, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

#include "qMRMLCaptureToolBarPlugin.h"
#include "qMRMLCaptureToolBar.h"

// --------------------------------------------------------------------------
qMRMLCaptureToolBarPlugin::qMRMLCaptureToolBarPlugin(QObject* _parent)
  : QObject(_parent)
{
}

// --------------------------------------------------------------------------
QWidget* qMRMLCaptureToolBarPlugin::createWidget(QWidget* _parent)
{
  qMRMLCaptureToolBar* _widget = new qMRMLCaptureToolBar(_parent);
  return _widget;
}

// --------------------------------------------------------------------------
QString qMRMLCaptureToolBarPlugin::domXml() const
{
  return "<widget class=\"qMRMLCaptureToolBar\" \
          name=\"MRMLCaptureToolBar\">\n"
         "</widget>\n";
}

// --------------------------------------------------------------------------
QIcon qMRMLCaptureToolBarPlugin::icon() const
{
  return QIcon();
}

// --------------------------------------------------------------------------
QString qMRMLCaptureToolBarPlugin::includeFile() const
{
  return "qMRMLCaptureToolBar.h";
}

// --------------------------------------------------------------------------
bool qMRMLCaptureToolBarPlugin::isContainer() const
{
  return false;
}

// --------------------------------------------------------------------------
QString qMRMLCaptureToolBarPlugin::name() const
{
  return "qMRMLCaptureToolBar";
}
