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

// Qt includes
#include <QDebug>
#include <QFileInfo>

// QtGUI includes
#include "qSlicerNodeWriter.h"
#include "qSlicerNodeWriterOptionsWidget.h"

// QTCore includes
#include "qSlicerCoreApplication.h"
#include "qSlicerCoreIOManager.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLStorableNode.h>
#include <vtkMRMLStorageNode.h>
#include <vtkMRMLMessageCollection.h>

// VTK includes
#include <vtkStringArray.h>

// STD includes
#include <string>

//-----------------------------------------------------------------------------
class qSlicerNodeWriterPrivate
{
public:
  QString Description;
  qSlicerIO::IOFileType FileType;
  QStringList NodeClassNames;
  bool SupportUseCompression;
};

//----------------------------------------------------------------------------
qSlicerNodeWriter::qSlicerNodeWriter(const QString& description,
                                     const qSlicerIO::IOFileType& fileIO,
                                     const QStringList& nodeClassNames,
                                     bool supportUseCompression,
                                     QObject* parentObject)
  : Superclass(parentObject)
  , d_ptr(new qSlicerNodeWriterPrivate)
{
  Q_D(qSlicerNodeWriter);
  d->Description = description;
  d->FileType = fileIO;
  d->SupportUseCompression = supportUseCompression;
  this->setNodeClassNames(nodeClassNames);
}

//----------------------------------------------------------------------------
qSlicerNodeWriter::~qSlicerNodeWriter() = default;

//----------------------------------------------------------------------------
QString qSlicerNodeWriter::description() const
{
  Q_D(const qSlicerNodeWriter);
  return d->Description;
}

//----------------------------------------------------------------------------
qSlicerIO::IOFileType qSlicerNodeWriter::fileType() const
{
  Q_D(const qSlicerNodeWriter);
  return d->FileType;
}

//----------------------------------------------------------------------------
bool qSlicerNodeWriter::canWriteObject(vtkObject* object) const
{
  Q_D(const qSlicerNodeWriter);
  vtkMRMLStorableNode* node = vtkMRMLStorableNode::SafeDownCast(object);
  if (node)
  {
    for (const QString& className : d->NodeClassNames)
    {
      if (node->IsA(className.toUtf8()))
      {
        return true;
      }
    }
  }
  return false;
}

//----------------------------------------------------------------------------
QStringList qSlicerNodeWriter::extensions(vtkObject* object) const
{
  QStringList supportedExtensions;
  vtkMRMLStorageNode* snode = qSlicerCoreIOManager::createAndAddDefaultStorageNode(vtkMRMLStorableNode::SafeDownCast(object));
  if (snode)
  {
    const int formatCount = snode->GetSupportedWriteFileTypes()->GetNumberOfValues();
    for (int formatIt = 0; formatIt < formatCount; ++formatIt)
    {
      std::string format = snode->GetSupportedWriteFileTypes()->GetValue(formatIt);
      supportedExtensions << QString::fromStdString(format);
    }
  }
  return supportedExtensions;
}

//----------------------------------------------------------------------------
bool qSlicerNodeWriter::write(const qSlicerIO::IOProperties& properties)
{
  this->setWrittenNodes(QStringList());

  Q_ASSERT(!properties["nodeID"].toString().isEmpty());

  vtkMRMLStorableNode* node = vtkMRMLStorableNode::SafeDownCast(this->getNodeByID(properties["nodeID"].toString().toUtf8().data()));
  if (this->canWriteObjectConfidence(node) <= 0.0)
  {
    return false;
  }
  vtkMRMLStorageNode* snode = qSlicerCoreIOManager::createAndAddDefaultStorageNode(node);
  if (snode == nullptr)
  {
    qDebug() << "No storage node for node" << properties["nodeID"].toString();
    return false;
  }

  Q_ASSERT(!properties["fileName"].toString().isEmpty());
  QString fileName = properties["fileName"].toString();
  snode->SetFileName(fileName.toUtf8());

  QString fileFormat = properties.value("fileFormat").toString();
  if (!fileFormat.isEmpty())
  {
    snode->SetWriteFileFormat(fileFormat.toUtf8());
  }
  snode->SetURI(nullptr);
  if (properties.contains("useCompression"))
  {
    snode->SetUseCompression(properties["useCompression"].toInt());
    if (properties.contains("compressionParameter"))
    {
      snode->SetCompressionParameter(properties["compressionParameter"].toString().toStdString());
    }
  }
  bool res = snode->WriteData(node);

  if (res)
  {
    this->setWrittenNodes(QStringList() << node->GetID());
  }

  this->userMessages()->AddMessages(snode->GetUserMessages());

  return res;
}

//-----------------------------------------------------------------------------
vtkMRMLNode* qSlicerNodeWriter::getNodeByID(const char* id) const
{
  vtkMRMLNode* node = this->mrmlScene()->GetNodeByID(id);
  return node;
}

//----------------------------------------------------------------------------
void qSlicerNodeWriter::setNodeClassNames(const QStringList& nodeClassNames)
{
  Q_D(qSlicerNodeWriter);
  d->NodeClassNames = nodeClassNames;
}

//----------------------------------------------------------------------------
QStringList qSlicerNodeWriter::nodeClassNames() const
{
  Q_D(const qSlicerNodeWriter);
  return d->NodeClassNames;
}

//----------------------------------------------------------------------------
void qSlicerNodeWriter::setSupportUseCompression(bool support)
{
  Q_D(qSlicerNodeWriter);
  d->SupportUseCompression = support;
}

//----------------------------------------------------------------------------
bool qSlicerNodeWriter::supportUseCompression() const
{
  Q_D(const qSlicerNodeWriter);
  return d->SupportUseCompression;
}
//-----------------------------------------------------------------------------
qSlicerIOOptions* qSlicerNodeWriter::options() const
{
  Q_D(const qSlicerNodeWriter);
  qSlicerNodeWriterOptionsWidget* options = new qSlicerNodeWriterOptionsWidget;
  options->setShowUseCompression(d->SupportUseCompression);
  return options;
}
