/*=========================================================================

  Copyright (c) Brigham and Women's Hospital (BWH) All Rights Reserved.

  See License.txt or http://www.slicer.org/copyright/copyright.txt for details.

==========================================================================*/

// Slicer includes
#include "vtkSlicerApplicationLogic.h"
// For:
//  - Slicer_BUILD_EXTENSIONMANAGER_SUPPORT
//  - Slicer_EXTENSIONS_DIRBASENAME
//  - Slicer_ORGANIZATION_DOMAIN
//  - Slicer_ORGANIZATION_NAME
#include "vtkSlicerConfigure.h"
#include "vtkSlicerTask.h"

// MRML includes
#include <vtkCacheManager.h>
#include <vtkDataIOManagerLogic.h>
#include <vtkMRMLRemoteIOLogic.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>

// VTKAddon includes
#include <vtkPersonInformation.h>

// VTK includes
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkThreads.h> // For VTK_USE_PTHREADS, VTK_USE_WIN32_THREADS

// ITKSYS includes
#include <itksys/SystemTools.hxx>

// STD includes
#include <algorithm>

#ifdef VTK_USE_PTHREADS
# include <unistd.h>
# include <sys/time.h>
# include <sys/resource.h>
#endif

#include <queue>

#include "vtkSlicerApplicationLogicRequests.h"

//----------------------------------------------------------------------------
class ProcessingTaskQueue : public std::queue<vtkSmartPointer<vtkSlicerTask>>
{
};
class ModifiedQueue : public std::queue<vtkSmartPointer<vtkObject>>
{
};
class ReadDataQueue : public std::queue<DataRequest*>
{
};
class WriteDataQueue : public std::queue<DataRequest*>
{
};

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerApplicationLogic);

//----------------------------------------------------------------------------
vtkSlicerApplicationLogic::vtkSlicerApplicationLogic()
{
  this->ProcessingThreadActive = false;

  this->ModifiedQueueActive = false;

  this->ReadDataQueueActive = false;

  this->WriteDataQueueActive = false;

  this->InternalTaskQueue = new ProcessingTaskQueue;
  this->InternalModifiedQueue = new ModifiedQueue;

  this->InternalReadDataQueue = new ReadDataQueue;
  this->InternalWriteDataQueue = new WriteDataQueue;

  this->UserInformation = vtkPersonInformation::New();
}

//----------------------------------------------------------------------------
vtkSlicerApplicationLogic::~vtkSlicerApplicationLogic()
{
  // All processing must stop before the queue objects are deleted
  this->TerminateProcessingThread();

  delete this->InternalTaskQueue;

  this->ModifiedQueueLock.lock();
  while (!(*this->InternalModifiedQueue).empty())
  {
    vtkObject* obj = (*this->InternalModifiedQueue).front();
    (*this->InternalModifiedQueue).pop();
    obj->Delete(); // decrement ref count
  }
  this->ModifiedQueueLock.unlock();
  delete this->InternalModifiedQueue;
  delete this->InternalReadDataQueue;
  delete this->InternalWriteDataQueue;

  this->UserInformation->Delete();
}

//----------------------------------------------------------------------------
unsigned int vtkSlicerApplicationLogic::GetReadDataQueueSize()
{
  return static_cast<unsigned int>((*this->InternalReadDataQueue).size());
}

//-----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::SetMRMLSceneDataIO(vtkMRMLScene* newMRMLScene, vtkMRMLRemoteIOLogic* remoteIOLogic, vtkDataIOManagerLogic* dataIOManagerLogic)
{
  if (remoteIOLogic)
  {
    if (remoteIOLogic->GetMRMLScene() != newMRMLScene)
    {
      if (remoteIOLogic->GetMRMLScene())
      {
        remoteIOLogic->RemoveDataIOFromScene();
      }
      remoteIOLogic->SetMRMLScene(newMRMLScene);
    }
  }

  if (dataIOManagerLogic)
  {
    if (dataIOManagerLogic->GetMRMLScene() != newMRMLScene)
    {
      dataIOManagerLogic->SetMRMLScene(newMRMLScene);
    }
  }

  if (newMRMLScene)
  {
    if (remoteIOLogic)
    {
      remoteIOLogic->AddDataIOToScene();
    }
  }
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);

  os << indent << "SlicerApplicationLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::CreateProcessingThread()
{
  if (!this->ProcessingThread.joinable())
  {
    this->ProcessingThreadActiveLock.lock();
    this->ProcessingThreadActive = true;
    this->ProcessingThreadActiveLock.unlock();

    this->ProcessingThread = std::thread(vtkSlicerApplicationLogic::ProcessingThreaderCallback, this);

    // Start four network threads (TODO: make the number of threads a setting)
    NetworkingThreads.push_back(std::thread(vtkSlicerApplicationLogic::NetworkingThreaderCallback, this));

    /*
     * TODO: it looks like curl is not thread safe by default
     * - maybe there's a setting that cmcurl can have
     *   similar to the --enable-threading of the standard curl build
     *
    NetworkingThreads.push_back(std::thread(vtkSlicerApplicationLogic::NetworkingThreaderCallback, this));
    NetworkingThreads.push_back(std::thread(vtkSlicerApplicationLogic::NetworkingThreaderCallback, this));
    NetworkingThreads.push_back(std::thread(vtkSlicerApplicationLogic::NetworkingThreaderCallback, this));
    */

    // Setup the communication channel back to the main thread
    this->ModifiedQueueActiveLock.lock();
    this->ModifiedQueueActive = true;
    this->ModifiedQueueActiveLock.unlock();
    this->ReadDataQueueActiveLock.lock();
    this->ReadDataQueueActive = true;
    this->ReadDataQueueActiveLock.unlock();
    this->WriteDataQueueActiveLock.lock();
    this->WriteDataQueueActive = true;
    this->WriteDataQueueActiveLock.unlock();

    int delay = 1000;
    this->InvokeEvent(vtkSlicerApplicationLogic::RequestModifiedEvent, &delay);
    this->InvokeEvent(vtkSlicerApplicationLogic::RequestReadDataEvent, &delay);
    this->InvokeEvent(vtkSlicerApplicationLogic::RequestWriteDataEvent, &delay);
  }
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::TerminateProcessingThread()
{
  if (this->ProcessingThread.joinable())
  {
    this->ModifiedQueueActiveLock.lock();
    this->ModifiedQueueActive = false;
    this->ModifiedQueueActiveLock.unlock();

    this->ReadDataQueueActiveLock.lock();
    this->ReadDataQueueActive = false;
    this->ReadDataQueueActiveLock.unlock();

    this->WriteDataQueueActiveLock.lock();
    this->WriteDataQueueActive = false;
    this->WriteDataQueueActiveLock.unlock();

    this->ProcessingThreadActiveLock.lock();
    this->ProcessingThreadActive = false;
    this->ProcessingThreadActiveLock.unlock();

    this->ProcessingThread.join();

    for (auto& thread : this->NetworkingThreads)
    {
      thread.join();
    }
    this->NetworkingThreads.clear();
  }
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::ProcessingThreaderCallback(vtkSlicerApplicationLogic* appLogic)
{
  if (!appLogic)
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::ProcessingThreaderCallback failed: invalid appLogic");
    return;
  }

  appLogic->SetCurrentThreadPriorityToBackground();

  // Start background processing tasks in this thread
  appLogic->ProcessProcessingTasks();
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::ProcessProcessingTasks()
{
  int active = true;
  vtkSmartPointer<vtkSlicerTask> task = nullptr;

  while (active)
  {
    // Check to see if we should be shutting down
    this->ProcessingThreadActiveLock.lock();
    active = this->ProcessingThreadActive;
    this->ProcessingThreadActiveLock.unlock();

    if (active)
    {
      // pull a task off the queue
      this->ProcessingTaskQueueLock.lock();
      if ((*this->InternalTaskQueue).size() > 0)
      {
        // std::cout << "Number of queued tasks: " << (*this->InternalTaskQueue).size() << std::endl;

        // only handle processing tasks in this thread
        task = (*this->InternalTaskQueue).front();
        if (task->GetType() == vtkSlicerTask::Processing)
        {
          (*this->InternalTaskQueue).pop();
        }
        else
        {
          task = nullptr;
        }
      }
      this->ProcessingTaskQueueLock.unlock();

      // process the task (should this be in a separate thread?)
      if (task)
      {
        task->Execute();
        task = nullptr;
      }
    }

    // busy wait
    itksys::SystemTools::Delay(100);
  }
}

void vtkSlicerApplicationLogic::NetworkingThreaderCallback(vtkSlicerApplicationLogic* appLogic)
{
  if (!appLogic)
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::NetworkingThreaderCallback failed: invalid appLogic");
    return;
  }

  appLogic->SetCurrentThreadPriorityToBackground();

  // Start network communication tasks in this thread
  appLogic->ProcessNetworkingTasks();
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::ProcessNetworkingTasks()
{
  int active = true;
  vtkSmartPointer<vtkSlicerTask> task = nullptr;

  while (active)
  {
    // Check to see if we should be shutting down
    this->ProcessingThreadActiveLock.lock();
    active = this->ProcessingThreadActive;
    this->ProcessingThreadActiveLock.unlock();

    if (active)
    {
      // pull a task off the queue
      this->ProcessingTaskQueueLock.lock();
      if ((*this->InternalTaskQueue).size() > 0)
      {
        // std::cout << "Number of queued tasks: " << (*this->InternalTaskQueue).size() << std::endl;
        task = (*this->InternalTaskQueue).front();
        if (task->GetType() == vtkSlicerTask::Networking)
        {
          (*this->InternalTaskQueue).pop();
        }
        else
        {
          task = nullptr;
        }
      }
      this->ProcessingTaskQueueLock.unlock();

      // process the task (should this be in a separate thread?)
      if (task)
      {
        task->Execute();
        task = nullptr;
      }
    }

    // busy wait
    itksys::SystemTools::Delay(100);
  }
}

//----------------------------------------------------------------------------
int vtkSlicerApplicationLogic::ScheduleTask(vtkSlicerTask* task)
{
  // only schedule a task if the processing task is up
  this->ProcessingThreadActiveLock.lock();
  int active = this->ProcessingThreadActive;
  this->ProcessingThreadActiveLock.unlock();
  if (!active)
  {
    return false;
  }

  this->ProcessingTaskQueueLock.lock();
  (*this->InternalTaskQueue).push(task);
  this->ProcessingTaskQueueLock.unlock();
  return true;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkSlicerApplicationLogic::RequestModified(vtkObject* obj)
{
  // only request a Modified if the Modified queue is up
  this->ModifiedQueueActiveLock.lock();
  int active = this->ModifiedQueueActive;
  this->ModifiedQueueActiveLock.unlock();
  if (!active)
  {
    // could not request the Modified
    return 0;
  }

  obj->Register(this);
  this->ModifiedQueueLock.lock();
  this->RequestTimeStamp.Modified();
  vtkMTimeType uid = this->RequestTimeStamp.GetMTime();
  (*this->InternalModifiedQueue).push(obj);
  this->ModifiedQueueLock.unlock();
  return uid;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkSlicerApplicationLogic::RequestReadFile(const char* refNode, const char* filename, int displayData, int deleteFile)
{
  // only request to read a file if the ReadData queue is up
  this->ReadDataQueueActiveLock.lock();
  int active = this->ReadDataQueueActive;
  this->ReadDataQueueActiveLock.unlock();
  if (!active)
  {
    // could not request the record be added to the queue
    return 0;
  }

  this->ReadDataQueueLock.lock();
  this->RequestTimeStamp.Modified();
  vtkMTimeType uid = this->RequestTimeStamp.GetMTime();
  (*this->InternalReadDataQueue).push(new ReadDataRequestFile(refNode, filename, displayData, deleteFile, uid));
  this->ReadDataQueueLock.unlock();
  return uid;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkSlicerApplicationLogic::RequestUpdateParentTransform(const std::string& refNode, const std::string& parentTransformNode)
{
  // only request to read a file if the ReadData queue is up
  this->ReadDataQueueActiveLock.lock();
  int active = this->ReadDataQueueActive;
  this->ReadDataQueueActiveLock.unlock();
  if (!active)
  {
    // could not request the record be added to the queue
    return 0;
  }

  this->ReadDataQueueLock.lock();
  this->RequestTimeStamp.Modified();
  vtkMTimeType uid = this->RequestTimeStamp.GetMTime();
  (*this->InternalReadDataQueue).push(new ReadDataRequestUpdateParentTransform(refNode, parentTransformNode, uid));
  this->ReadDataQueueLock.unlock();
  return uid;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkSlicerApplicationLogic::RequestUpdateSubjectHierarchyLocation(const std::string& updatedNode, const std::string& siblingNode)
{
  // only request to read a file if the ReadData queue is up
  this->ReadDataQueueActiveLock.lock();
  int active = this->ReadDataQueueActive;
  this->ReadDataQueueActiveLock.unlock();
  if (!active)
  {
    // could not request the record be added to the queue
    return 0;
  }

  this->ReadDataQueueLock.lock();
  this->RequestTimeStamp.Modified();
  vtkMTimeType uid = this->RequestTimeStamp.GetMTime();
  (*this->InternalReadDataQueue).push(new ReadDataRequestUpdateSubjectHierarchyLocation(updatedNode, siblingNode, uid));
  this->ReadDataQueueLock.unlock();
  return uid;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkSlicerApplicationLogic::RequestAddNodeReference(const std::string& referencingNode, const std::string& referencedNode, const std::string& role)
{
  // only request to read a file if the ReadData queue is up
  this->ReadDataQueueActiveLock.lock();
  int active = this->ReadDataQueueActive;
  this->ReadDataQueueActiveLock.unlock();
  if (!active)
  {
    // could not request the record be added to the queue
    return 0;
  }

  this->ReadDataQueueLock.lock();
  this->RequestTimeStamp.Modified();
  vtkMTimeType uid = this->RequestTimeStamp.GetMTime();
  (*this->InternalReadDataQueue).push(new ReadDataRequestAddNodeReference(referencingNode, referencedNode, role, uid));
  this->ReadDataQueueLock.unlock();
  return uid;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkSlicerApplicationLogic::RequestWriteData(const char* refNode, const char* filename)
{
  // only request to write a file if the WriteData queue is up
  this->WriteDataQueueActiveLock.lock();
  int active = this->WriteDataQueueActive;
  this->WriteDataQueueActiveLock.unlock();
  if (!active)
  {
    // could not request the record be added to the queue
    return 0;
  }

  this->WriteDataQueueLock.lock();
  this->RequestTimeStamp.Modified();
  vtkMTimeType uid = this->RequestTimeStamp.GetMTime();
  (*this->InternalWriteDataQueue).push(new WriteDataRequestFile(refNode, filename, uid));
  this->WriteDataQueueLock.unlock();
  return uid;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkSlicerApplicationLogic::RequestReadScene(const std::string& filename,
                                                         std::vector<std::string>& targetIDs,
                                                         std::vector<std::string>& sourceIDs,
                                                         int displayData,
                                                         int deleteFile)
{
  // only request to read a file if the ReadData queue is up
  this->ReadDataQueueActiveLock.lock();
  int active = this->ReadDataQueueActive;
  this->ReadDataQueueActiveLock.unlock();
  if (!active)
  {
    // could not request the record be added to the queue
    return 0;
  }

  this->ReadDataQueueLock.lock();
  this->RequestTimeStamp.Modified();
  vtkMTimeType uid = this->RequestTimeStamp.GetMTime();
  (*this->InternalReadDataQueue).push(new ReadDataRequestScene(targetIDs, sourceIDs, filename, displayData, deleteFile, uid));
  this->ReadDataQueueLock.unlock();
  return uid;
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::ProcessModified()
{
  // Check to see if we should be shutting down
  this->ModifiedQueueActiveLock.lock();
  int active = this->ModifiedQueueActive;
  this->ModifiedQueueActiveLock.unlock();
  if (!active)
  {
    return;
  }

  vtkSmartPointer<vtkObject> obj = nullptr;
  // pull an object off the queue to modify
  this->ModifiedQueueLock.lock();
  if ((*this->InternalModifiedQueue).size() > 0)
  {
    obj = (*this->InternalModifiedQueue).front();
    (*this->InternalModifiedQueue).pop();

    // pop off any extra copies of the same object to save some updates
    while (!(*this->InternalModifiedQueue).empty() //
           && (obj == (*this->InternalModifiedQueue).front()))
    {
      (*this->InternalModifiedQueue).pop();
      obj->Delete(); // decrement ref count
    }
  }
  this->ModifiedQueueLock.unlock();

  // Modify the object
  //  - decrement reference count that was increased when it was added to the queue
  if (obj.GetPointer())
  {
    vtkMRMLNode* node = vtkMRMLNode::SafeDownCast(obj);
    if (node)
    {
      // use Start/EndModify to also invoke all pending events that might have been
      // accumulated because of previous use of SetDisableModifiedEvent (e.g., in itkMRMLIDImageIO).
      bool wasModified = node->StartModify();
      node->Modified();
      node->EndModify(wasModified);
    }
    else
    {
      obj->Modified();
    }
    obj->Delete();
    obj = nullptr;
  }

  // schedule the next timer sooner in case there is stuff in the queue
  // otherwise for a while later
  int delay = (*this->InternalModifiedQueue).size() > 0 ? 0 : 200;
  this->InvokeEvent(vtkSlicerApplicationLogic::RequestModifiedEvent, &delay);
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::ProcessReadData()
{
  // Check to see if we should be shutting down
  this->ReadDataQueueActiveLock.lock();
  int active = this->ReadDataQueueActive;
  this->ReadDataQueueActiveLock.unlock();
  if (!active)
  {
    return;
  }

  // pull an object off the queue
  DataRequest* req = nullptr;
  this->ReadDataQueueLock.lock();
  if ((*this->InternalReadDataQueue).size() > 0)
  {
    req = (*this->InternalReadDataQueue).front();
    (*this->InternalReadDataQueue).pop();
  }
  this->ReadDataQueueLock.unlock();

  vtkMTimeType uid = 0;
  if (req)
  {
    uid = req->GetUID();
    req->Execute(this);
    delete req;
  }

  int delay = (*this->InternalReadDataQueue).size() > 0 ? 0 : 200;
  // schedule the next timer sooner in case there is stuff in the queue
  // otherwise for a while later
  this->InvokeEvent(vtkSlicerApplicationLogic::RequestReadDataEvent, &delay);
  if (uid)
  {
    this->InvokeEvent(vtkSlicerApplicationLogic::RequestProcessedEvent, reinterpret_cast<void*>(uid));
  }
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::ProcessWriteData()
{
  // Check to see if we should be shutting down
  this->WriteDataQueueActiveLock.lock();
  int active = this->WriteDataQueueActive;
  this->WriteDataQueueActiveLock.unlock();
  if (!active)
  {
    return;
  }

  // pull an object off the queue
  DataRequest* req = nullptr;
  this->WriteDataQueueLock.lock();
  if ((*this->InternalWriteDataQueue).size() > 0)
  {
    req = (*this->InternalWriteDataQueue).front();
    (*this->InternalWriteDataQueue).pop();
  }
  this->WriteDataQueueLock.unlock();

  if (req)
  {
    vtkMTimeType uid = req->GetUID();
    req->Execute(this);
    delete req;

    // schedule the next timer sooner in case there is stuff in the queue
    // otherwise for a while later
    int delay = (*this->InternalWriteDataQueue).size() > 0 ? 0 : 200;
    this->InvokeEvent(vtkSlicerApplicationLogic::RequestWriteDataEvent, &delay);
    if (uid)
    {
      this->InvokeEvent(vtkSlicerApplicationLogic::RequestProcessedEvent, reinterpret_cast<void*>(uid));
    }
  }
}

//----------------------------------------------------------------------------
bool vtkSlicerApplicationLogic::IsEmbeddedModule(const std::string& filePath, const std::string& applicationHomeDir, const std::string& slicerRevision)
{
  if (filePath.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::IsEmbeddedModule failed: filePath argument is empty");
    return false;
  }
  if (applicationHomeDir.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::IsEmbeddedModule failed: applicationHomeDir argument is empty");
    return false;
  }
  std::string extensionPath = itksys::SystemTools::GetFilenamePath(filePath);
  bool isEmbedded = itksys::SystemTools::StringStartsWith(extensionPath.c_str(), applicationHomeDir.c_str());
#ifdef Slicer_BUILD_EXTENSIONMANAGER_SUPPORT
  // Extensions may be stored in the application home directory (it is always the case on macOS and for Windows/Linux
  // if Slicer_BUILD_EXTENSIONMANAGER_SUPPORT is enabled), therefore we need to detect these folders as extensions:
  // - Windows/Linux: <applicationHomeDir>/<Slicer_ORGANIZATION_NAME/DOMAIN>/<Slicer_EXTENSIONS_DIRBASENAME>-<slicerRevision>
  // - macOS: <applicationName>.app/Contents/<Slicer_EXTENSIONS_DIRBASENAME>-<slicerRevision>
  // We just check for <Slicer_EXTENSIONS_DIRBASENAME>-<slicerRevision> folder name, as it is simple to do, and
  // it is specific enough.
  if (isEmbedded && extensionPath.find(Slicer_EXTENSIONS_DIRBASENAME "-" + slicerRevision) != std::string::npos)
  {
    isEmbedded = false;
  }
#else
  (void)slicerRevision;
#endif
  return isEmbedded;
}

//----------------------------------------------------------------------------
bool vtkSlicerApplicationLogic::IsPluginInstalled(const std::string& filePath, const std::string& applicationHomeDir)
{
  if (filePath.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::IsPluginInstalled failed: filePath argument is empty");
    return false;
  }
  if (applicationHomeDir.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::IsPluginInstalled failed: applicationHomeDir argument is empty");
    return false;
  }

  // If the plugin is found in the application build directory (or one of its descendent), it is not
  // considered to be installed. This is generally true unless the plugin is found in the extension
  // install directory (or one of its descendent).
  std::string path = itksys::SystemTools::GetFilenamePath(filePath);
  std::string canonicalPath = itksys::SystemTools::GetRealPath(path.c_str());
  if (itksys::SystemTools::StringStartsWith(canonicalPath.c_str(), applicationHomeDir.c_str()))
  {
    bool isAppBuildTree = itksys::SystemTools::FileExists(std::string(applicationHomeDir).append("/CMakeCache.txt").c_str(), true);
    if (isAppBuildTree)
    {
#ifdef Slicer_BUILD_EXTENSIONMANAGER_SUPPORT
      // Check if plugin is installed as an extension
      {
        std::string extensionInstallDir = itksys::SystemTools::GetRealPath(applicationHomeDir + "/" Slicer_ORGANIZATION_DOMAIN "/" Slicer_EXTENSIONS_DIRBASENAME);
        if (itksys::SystemTools::StringStartsWith(canonicalPath.c_str(), extensionInstallDir.c_str()))
        {
          return true;
        }
      }
      {
        std::string extensionInstallDir = itksys::SystemTools::GetRealPath(applicationHomeDir + "/" Slicer_ORGANIZATION_NAME "/" Slicer_EXTENSIONS_DIRBASENAME);
        if (itksys::SystemTools::StringStartsWith(canonicalPath.c_str(), extensionInstallDir.c_str()))
        {
          return true;
        }
      }
#endif
      return false;
    }
    else
    {
      // Application directory corresponds to an install tree and the plugin is packaged along side it.
      return true;
    }
  }

  // If the plugin filePath is in a build directory, it is is not installed.
  std::string root;
  std::string canonicalPathWithoutRoot = itksys::SystemTools::SplitPathRootComponent(canonicalPath.c_str(), &root);
  do
  {
    if (itksys::SystemTools::FileExists((root + canonicalPathWithoutRoot + "/CMakeCache.txt").c_str(), true))
    {
      return false;
    }
    canonicalPathWithoutRoot = itksys::SystemTools::GetParentDirectory(canonicalPathWithoutRoot.c_str());
  } while (!canonicalPathWithoutRoot.empty());

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerApplicationLogic::IsPluginBuiltIn(const std::string& filePath, const std::string& applicationHomeDir, const std::string& slicerRevision)
{
  if (filePath.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::IsPluginBuiltIn failed: filePath argument is empty");
    return false;
  }
  if (applicationHomeDir.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::IsPluginBuiltIn failed: applicationHomeDir argument is empty");
    return false;
  }

  std::string canonicalApplicationHomeDir = itksys::SystemTools::GetRealPath(applicationHomeDir.c_str());

  std::string path = itksys::SystemTools::GetFilenamePath(filePath);
  std::string canonicalPath = itksys::SystemTools::GetRealPath(path.c_str());

  bool isBuiltIn = itksys::SystemTools::StringStartsWith(canonicalPath.c_str(), canonicalApplicationHomeDir.c_str());

#ifdef Slicer_BUILD_EXTENSIONMANAGER_SUPPORT
  // Extensions may be stored in the application home directory (it is always the case on macOS and for Windows/Linux
  // if Slicer_BUILD_EXTENSIONMANAGER_SUPPORT is enabled), therefore we need to detect these folders as extensions:
  // - Windows/Linux: <applicationHomeDir>/<Slicer_ORGANIZATION_NAME/DOMAIN>/<Slicer_EXTENSIONS_DIRBASENAME>-<slicerRevision>
  // - macOS: <applicationName>.app/Contents/<Slicer_EXTENSIONS_DIRBASENAME>-<slicerRevision>
  // We just check for <Slicer_EXTENSIONS_DIRBASENAME>-<slicerRevision> folder name, as it is simple to do, and
  // it is specific enough.
  if (isBuiltIn && canonicalPath.find(Slicer_EXTENSIONS_DIRBASENAME "-" + slicerRevision) != std::string::npos)
  {
    isBuiltIn = false;
  }
#else
  (void)slicerRevision;
#endif

  return isBuiltIn;
}

namespace
{
//----------------------------------------------------------------------------
std::string GetModuleHomeDirectory(const std::string& filePath, std::string& slicerSubDir, std::string& moduleTypeSubDir)
{
  if (filePath.empty())
  {
    vtkGenericWarningMacro("GetModuleHomeDirectory failed: filePath argument is empty");
    return std::string();
  }

  // In the current implementation, we assume the path to a module and its resources
  // will respectively have the following structure:
  //   ../lib/Slicer-X.Y/<module-type>/module
  // and
  //   ../share/Slicer-X.Y/<module-type>/<module-name>/

  std::vector<std::string> components;
  itksys::SystemTools::SplitPath(filePath.c_str(), components, true);

  // components[components.size() - 1] -> fileName
  // components[components.size() - 2 - offset] -> {qt-scripted-modules, qt-loadable-modules, ...}
  // components[components.size() - 3 - offset] -> Slicer-X.Y
  // components[components.size() - 4 - offset] -> lib
  // components[0 .. components.size() - 4 - offset] -> Common path to lib and share directory

  if (components.size() < 5)
  {
    // At least 5 components are expected to be able to compute the module home directory
    vtkGenericWarningMacro("GetModuleHomeDirectory: failed to compute module home directory given filePath: " << filePath);
    return std::string();
  }

  // offset == 1 if there is an intermediate build directory
  int offset = 0;
  std::string intDir(".");
  std::string possibleIntDir = components.at(components.size() - 2);
  if (!itksys::SystemTools::Strucmp(possibleIntDir.c_str(), "Debug") ||          //
      !itksys::SystemTools::Strucmp(possibleIntDir.c_str(), "Release") ||        //
      !itksys::SystemTools::Strucmp(possibleIntDir.c_str(), "RelWithDebInfo") || //
      !itksys::SystemTools::Strucmp(possibleIntDir.c_str(), "MinSizeRel"))
  {
    offset = 1;
    intDir = possibleIntDir;
  }

  moduleTypeSubDir = components.at(components.size() - 2 - offset);
  slicerSubDir = components.at(components.size() - 3 - offset);

  std::string homeDirectory = itksys::SystemTools::JoinPath(components.begin(), components.end() - 4 - offset);
  return homeDirectory;
}

} // end of anonymous namespace

//----------------------------------------------------------------------------
std::string vtkSlicerApplicationLogic::GetModuleShareDirectory(const std::string& moduleName, const std::string& filePath)
{
  if (moduleName.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::GetModuleShareDirectory failed: moduleName argument is empty");
    return std::string();
  }
  if (filePath.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::GetModuleShareDirectory failed: filePath argument is empty");
    return std::string();
  }

  std::string slicerSubDir;
  std::string moduleTypeSubDir;
  std::string shareDirectory = GetModuleHomeDirectory(filePath, slicerSubDir, moduleTypeSubDir);

  if (shareDirectory.empty())
  {
    return std::string();
  }

  shareDirectory.append("/share");
  shareDirectory.append("/");
  shareDirectory.append(slicerSubDir);
  shareDirectory.append("/");
  shareDirectory.append(moduleTypeSubDir);
  shareDirectory.append("/");
  shareDirectory.append(moduleName);

  return shareDirectory;
}

//----------------------------------------------------------------------------
std::string vtkSlicerApplicationLogic::GetModuleSlicerXYShareDirectory(const std::string& filePath)
{
  if (filePath.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::GetModuleSlicerXYShareDirectory failed: filePath argument is empty");
    return std::string();
  }
  std::string slicerSubDir;
  std::string moduleTypeSubDir;
  std::string shareDirectory = GetModuleHomeDirectory(filePath, slicerSubDir, moduleTypeSubDir);

  if (shareDirectory.empty())
  {
    return std::string();
  }

  shareDirectory.append("/share");
  shareDirectory.append("/");
  shareDirectory.append(slicerSubDir);
  return shareDirectory;
}

//----------------------------------------------------------------------------
std::string vtkSlicerApplicationLogic::GetModuleSlicerXYLibDirectory(const std::string& filePath)
{
  if (filePath.empty())
  {
    vtkGenericWarningMacro("vtkSlicerApplicationLogic::GetModuleSlicerXYLibDirectory failed: filePath argument is empty");
    return std::string();
  }
  std::string slicerSubDir;
  std::string moduleTypeSubDir;
  std::string libDirectory = GetModuleHomeDirectory(filePath, slicerSubDir, moduleTypeSubDir);
  libDirectory.append("/lib");
  libDirectory.append("/");
  libDirectory.append(slicerSubDir);
  return libDirectory;
}

//----------------------------------------------------------------------------
vtkPersonInformation* vtkSlicerApplicationLogic::GetUserInformation()
{
  return this->UserInformation;
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::SetCurrentThreadPriorityToBackground()
{
  int processingThreadPriority = 0;
  bool isPriorityEnvSet = false;
  const char* slicerProcThreadPrio = itksys::SystemTools::GetEnv("SLICER_BACKGROUND_THREAD_PRIORITY");
  if (slicerProcThreadPrio)
  {
    const std::string priorityStr = slicerProcThreadPrio;
    try
    {
      processingThreadPriority = std::stoi(priorityStr);
      isPriorityEnvSet = true;
    }
    catch (...)
    {
      vtkWarningMacro("vtkSlicerApplicationLogic::SetCurrentThreadPriorityToBackground failed: "
                      "Invalid SLICER_BACKGROUND_THREAD_PRIORITY value ("
                      << priorityStr << "), expected an integer");
    }
  }

#ifdef VTK_USE_WIN32_THREADS
  // Adjust the priority of this thread
  bool ret = SetThreadPriority(GetCurrentThread(), isPriorityEnvSet ? processingThreadPriority : THREAD_PRIORITY_BELOW_NORMAL);
  if (!ret)
  {
    vtkWarningMacro("vtkSlicerApplicationLogic::SetCurrentThreadPriorityToBackground failed: setThreadPriority did not succeed.");
  }
#endif

#ifdef VTK_USE_PTHREADS
  // Adjust the priority of all PROCESS level threads.  Not a perfect solution.
  int which = PRIO_PROCESS;
  int priority = isPriorityEnvSet ? processingThreadPriority : 20;
  id_t pid = getpid();
  int ret = setpriority(which, pid, priority);
  if (ret != 0)
  {
    vtkWarningMacro("vtkSlicerApplicationLogic::SetCurrentThreadPriorityToBackground failed: "
                    "setpriority did not succeed. You need root privileges to set a priority < 0.");
  }
#endif
}

//----------------------------------------------------------------------------
void vtkSlicerApplicationLogic::RequestModifiedCallback(vtkObject* vtkNotUsed(caller), unsigned long vtkNotUsed(eid), void* clientData, void* callData)
{
  // Note: This method may be called from any thread
  if (clientData == nullptr || callData == nullptr)
  {
    return;
  }
  vtkSlicerApplicationLogic* appLogic = static_cast<vtkSlicerApplicationLogic*>(clientData);
  vtkObject* modifiedObject = static_cast<vtkObject*>(callData);
  appLogic->RequestModified(modifiedObject);
}
