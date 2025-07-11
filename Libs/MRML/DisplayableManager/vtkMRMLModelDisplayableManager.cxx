/*=========================================================================

  Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==========================================================================*/

// MRMLDisplayableManager includes
#include "vtkMRMLModelDisplayableManager.h"
#include "vtkMRMLThreeDViewInteractorStyle.h"
#include "vtkMRMLApplicationLogic.h"

// MRML/Slicer includes
#include <vtkEventBroker.h>
#include <vtkMRMLClipNode.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLDisplayableNode.h>
#include <vtkMRMLFolderDisplayNode.h>
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLModelDisplayNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSubjectHierarchyConstants.h>
#include <vtkMRMLSubjectHierarchyNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLVolumeNode.h>

// VTK includes
#include <vtkAlgorithm.h>
#include <vtkAlgorithmOutput.h>
#include <vtkAssignAttribute.h>
#include <vtkCapPolyData.h>
#include <vtkCallbackCommand.h>
#include <vtkCellArray.h>
#include <vtkClipDataSet.h>
#include <vtkClipPolyData.h>
#include <vtkColorTransferFunction.h>
#include <vtkDataSetAttributes.h>
#include <vtkDataSetMapper.h>
#include <vtkExtractCells.h>
#include <vtkExtractGeometry.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkGeneralTransform.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageMapper3D.h>
#include <vtkImplicitBoolean.h>
#include <vtkImplicitFunction.h>
#include <vtkImplicitFunctionCollection.h>
#include <vtkLookupTable.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>
#include <vtkPlaneCollection.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPolyDataMapper.h>
#include <vtkProp3DCollection.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkTexture.h>
#include <vtkTransform.h>
#include <vtkTransformFilter.h>
#include <vtkVersion.h>
#include <vtkWeakPointer.h>
// For picking
#include <vtkCellPicker.h>
#include <vtkPointPicker.h>
#include <vtkPropPicker.h>
#include <vtkRendererCollection.h>
#include <vtkWorldPointPicker.h>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLModelDisplayableManager);

//---------------------------------------------------------------------------
class vtkMRMLModelDisplayableManager::vtkInternal
{
public:
  vtkInternal(vtkMRMLModelDisplayableManager* external);
  ~vtkInternal();

  /// Reset all the pick vars
  void ResetPick();
  /// Find picked node from mesh and set PickedNodeID in Internal
  void FindPickedDisplayNodeFromMesh(vtkPointSet* mesh, double pickedPoint[3]);
  /// Find node in scene from imageData and set PickedNodeID in Internal
  void FindDisplayNodeFromImageData(vtkMRMLScene* scene, vtkImageData* imageData);
  /// Find picked point index in mesh and picked cell (PickedCellID) and set PickedPointID in Internal
  void FindPickedPointOnMeshAndCell(vtkPointSet* mesh, double pickedPoint[3]);
  /// Find first picked node from prop3Ds in cell picker and set PickedNodeID in Internal
  void FindFirstPickedDisplayNodeFromPickerProp3Ds();

public:
  vtkMRMLModelDisplayableManager* External;

  // For the following maps, the key is the display node ID.
  // clang-format off
  std::map<std::string, vtkSmartPointer<vtkProp3D>>             DisplayedActors;
  std::map<std::string, vtkWeakPointer<vtkMRMLDisplayNode>>     DisplayedNodes;
  std::map<std::string, int>                                    DisplayedClipState;
  std::map<std::string, vtkWeakPointer<vtkMRMLDisplayableNode>> DisplayableNodes;
  std::map<std::string, vtkSmartPointer<vtkTransformFilter>>    DisplayNodeTransformFilters;
  std::map<std::string, vtkSmartPointer<vtkAlgorithm>>          Clippers;
  std::map<std::string, vtkSmartPointer<vtkCapPolyData>>        Cappers;
  std::map<std::string, vtkSmartPointer<vtkProp3D>>             DisplayedCapActors;
  std::map<std::string, vtkSmartPointer<vtkTransformFilter>>    DisplayNodeCapTransformFilters;
  // clang-format on

  bool IsUpdatingModelsFromMRML;

  // clang-format off
  vtkSmartPointer<vtkWorldPointPicker> WorldPointPicker;
  vtkSmartPointer<vtkPropPicker>       PropPicker;
  vtkSmartPointer<vtkCellPicker>       CellPicker;
  vtkSmartPointer<vtkPointPicker>      PointPicker;
  // clang-format on

  // Information about a pick event
  // clang-format off
  std::string  PickedDisplayNodeID;
  double       PickedRAS[3];
  vtkIdType    PickedCellID;
  vtkIdType    PickedPointID;
  // clang-format on

  // Used for caching the node pointer so that we do not have to search in the scene each time.
  // We do not add an observer therefore we can let the selection node deleted without our knowledge.
  vtkWeakPointer<vtkMRMLSelectionNode> SelectionNode;
};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLModelDisplayableManager::vtkInternal::vtkInternal(vtkMRMLModelDisplayableManager* external)
  : External(external)
{
  // Instantiate and initialize Pickers
  this->WorldPointPicker = vtkSmartPointer<vtkWorldPointPicker>::New();
  this->PropPicker = vtkSmartPointer<vtkPropPicker>::New();
  this->CellPicker = vtkSmartPointer<vtkCellPicker>::New();
  this->CellPicker->SetTolerance(0.00001);
  this->PointPicker = vtkSmartPointer<vtkPointPicker>::New();
  this->ResetPick();

  this->IsUpdatingModelsFromMRML = false;
}

//---------------------------------------------------------------------------
vtkMRMLModelDisplayableManager::vtkInternal::~vtkInternal() = default;

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::vtkInternal::ResetPick()
{
  this->PickedDisplayNodeID.clear();
  for (int i = 0; i < 3; i++)
  {
    this->PickedRAS[i] = 0.0;
  }
  this->PickedCellID = -1;
  this->PickedPointID = -1;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::vtkInternal::FindPickedDisplayNodeFromMesh(vtkPointSet* mesh, double vtkNotUsed(pickedPoint)[3])
{
  if (!mesh)
  {
    return;
  }

  for (auto modelIt = this->DisplayedNodes.begin(); modelIt != this->DisplayedNodes.end(); modelIt++)
  {
    if (modelIt->second != 0)
    {
      if (vtkMRMLModelDisplayNode::SafeDownCast(modelIt->second) && //
          vtkMRMLModelDisplayNode::SafeDownCast(modelIt->second)->GetOutputMesh() == mesh)
      {
        this->PickedDisplayNodeID = modelIt->first;
        return; // Display node found
      }
    }
  }
}
//
//---------------------------------------------------------------------------
// for consistency with other vtkInternal classes this does not have access
// to the mrmlScene, so it is passed as a parameter
void vtkMRMLModelDisplayableManager::vtkInternal::FindDisplayNodeFromImageData(vtkMRMLScene* scene, vtkImageData* imageData)
{
  if (!scene || !imageData)
  {
    return;
  }
  // note that this library doesn't link to the VolumeRendering code because it is
  // a loadable module.  However we can still iterate over volume rendering nodes
  // and use the superclass abstract methods to confirm that the passed in imageData
  // corresponds to the display node.
  std::vector<vtkMRMLNode*> displayNodes;
  int nodeCount = scene->GetNodesByClass("vtkMRMLVolumeRenderingDisplayNode", displayNodes);
  for (int nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
  {
    vtkMRMLDisplayNode* displayNode = vtkMRMLDisplayNode::SafeDownCast(displayNodes[nodeIndex]);
    if (displayNode)
    {
      vtkMRMLVolumeNode* volumeNode = vtkMRMLVolumeNode::SafeDownCast(displayNode->GetDisplayableNode());
      vtkImageData* volumeImageData = nullptr;
      if (volumeNode)
      {
        volumeImageData = vtkImageData::SafeDownCast(volumeNode->GetImageData());
      }
      if (volumeImageData && volumeImageData == imageData)
      {
        this->PickedDisplayNodeID = displayNode->GetID();
        return; // Display node found
      }
    }
  }
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::vtkInternal::FindPickedPointOnMeshAndCell(vtkPointSet* mesh, double pickedPoint[3])
{
  if (!mesh || this->PickedCellID < 0)
  {
    return;
  }

  // Figure out the closest vertex in the picked cell to the picked RAS
  // point. Only doing this on model nodes for now.
  vtkCell* cell = mesh->GetCell(this->PickedCellID);
  if (!cell)
  {
    return;
  }

  int numPoints = cell->GetNumberOfPoints();
  int closestPointId = -1;
  double closestDistance = 0.0l;
  for (int p = 0; p < numPoints; p++)
  {
    int pointId = cell->GetPointId(p);
    double* pointCoords = mesh->GetPoint(pointId);
    if (pointCoords != nullptr)
    {
      double distance = sqrt(pow(pointCoords[0] - pickedPoint[0], 2) + //
                             pow(pointCoords[1] - pickedPoint[1], 2) + //
                             pow(pointCoords[2] - pickedPoint[2], 2));
      if (p == 0 || distance < closestDistance)
      {
        closestDistance = distance;
        closestPointId = pointId;
      }
    }
  }
  this->PickedPointID = closestPointId;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::vtkInternal::FindFirstPickedDisplayNodeFromPickerProp3Ds()
{
  if (!this->CellPicker)
  {
    return;
  }

  vtkProp3DCollection* props = this->CellPicker->GetProp3Ds();
  for (int propIndex = 0; propIndex < props->GetNumberOfItems(); ++propIndex)
  {
    vtkProp3D* pickedProp = vtkProp3D::SafeDownCast(props->GetItemAsObject(propIndex));
    if (!pickedProp)
    {
      continue;
    }
    for (auto propIt = this->DisplayedActors.begin(); propIt != this->DisplayedActors.end(); propIt++)
    {
      if (pickedProp == propIt->second)
      {
        this->PickedDisplayNodeID = propIt->first;
        return; // Display node found
      }
    }
  }
}

//---------------------------------------------------------------------------
// vtkMRMLModelDisplayableManager methods

//---------------------------------------------------------------------------
vtkMRMLModelDisplayableManager::vtkMRMLModelDisplayableManager()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkMRMLModelDisplayableManager::~vtkMRMLModelDisplayableManager()
{
  this->Internal->SelectionNode = nullptr; // WeakPointer, therefore must not use vtkSetMRMLNodeMacro
  this->ClearDisplayMaps();
  delete this->Internal;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);

  os << indent << "vtkMRMLModelDisplayableManager: " << this->GetClassName() << "\n";

  os << indent << "PickedDisplayNodeID = " << this->Internal->PickedDisplayNodeID.c_str() << "\n";
  os << indent << "PickedRAS = (" << this->Internal->PickedRAS[0] << ", " << this->Internal->PickedRAS[1] << ", " << this->Internal->PickedRAS[2] << ")\n";
  os << indent << "PickedCellID = " << this->Internal->PickedCellID << "\n";
  os << indent << "PickedPointID = " << this->Internal->PickedPointID << "\n";
}

//---------------------------------------------------------------------------
int vtkMRMLModelDisplayableManager::ActiveInteractionModes()
{
  // return vtkMRMLInteractionNode::ViewTransform;
  return 0;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  if (this->GetMRMLScene() == nullptr)
  {
    return;
  }
  if (this->GetInteractor() &&                    //
      this->GetInteractor()->GetRenderWindow() && //
      this->GetInteractor()->GetRenderWindow()->CheckInRenderStatus())
  {
    vtkDebugMacro("skipping ProcessMRMLNodesEvents during render");
    return;
  }

  bool isUpdating = this->GetMRMLScene()->IsBatchProcessing();
  if (vtkMRMLDisplayableNode::SafeDownCast(caller))
  {
    // There is no need to request a render (which can be expensive if the
    // volume rendering is on) if nothing visible has changed.
    bool requestRender = true;
    vtkMRMLDisplayableNode* displayableNode = vtkMRMLDisplayableNode::SafeDownCast(caller);
    switch (event)
    {
      case vtkMRMLDisplayableNode::DisplayModifiedEvent:
        // don't go any further if the modified display node is not a model
        if (!this->IsModelDisplayable(displayableNode) && //
            !this->IsModelDisplayable(reinterpret_cast<vtkMRMLDisplayNode*>(callData)))
        {
          requestRender = false;
          break;
        } // else fall through
      case vtkCommand::ModifiedEvent:
      case vtkMRMLModelNode::MeshModifiedEvent:
      case vtkMRMLTransformableNode::TransformModifiedEvent:
      case vtkMRMLClipNode::ClipNodeModifiedEvent: requestRender = this->OnMRMLDisplayableModelNodeModifiedEvent(displayableNode); break;
      default:
        // We don't expect any other types of events.
        break;
    }
    if (!isUpdating && requestRender)
    {
      this->RequestRender();
    }
  }
  else
  {
    this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UnobserveMRMLScene()
{
  this->RemoveModelProps();
  this->RemoveModelObservers(1);
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::OnMRMLSceneStartClose()
{
  this->RemoveModelObservers(0);
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::OnMRMLSceneEndClose()
{
  // Clean
  this->RemoveModelProps();
  this->RemoveModelObservers(1);

  this->SetUpdateFromMRMLRequested(true);
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UpdateFromMRMLScene()
{
  // UpdateFromMRML will be executed only if there has been some actions
  // during the import that requested it (don't call
  // SetUpdateFromMRMLRequested(1) here, it should be done somewhere else
  // maybe in OnMRMLSceneNodeAddedEvent, OnMRMLSceneNodeRemovedEvent or
  // OnMRMLDisplayableModelNodeModifiedEvent).
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if (!node->IsA("vtkMRMLDisplayableNode") //
      && !node->IsA("vtkMRMLDisplayNode"))
  {
    return;
  }

  this->SetUpdateFromMRMLRequested(true);

  // Escape if the scene a scene is being closed, imported or connected
  if (this->GetMRMLScene()->IsBatchProcessing())
  {
    return;
  }

  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  if (!node->IsA("vtkMRMLDisplayableNode") //
      && !node->IsA("vtkMRMLDisplayNode"))
  {
    return;
  }

  this->SetUpdateFromMRMLRequested(true);

  // Escape if the scene a scene is being closed, imported or connected
  if (this->GetMRMLScene()->IsBatchProcessing())
  {
    return;
  }

  // Node specific processing
  if (node->IsA("vtkMRMLDisplayableNode"))
  {
    this->RemoveDisplayable(vtkMRMLDisplayableNode::SafeDownCast(node));
  }

  this->RequestRender();
}

//---------------------------------------------------------------------------
bool vtkMRMLModelDisplayableManager::IsModelDisplayable(vtkMRMLDisplayableNode* node) const
{
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(node);
  if (!node || //
      (modelNode && modelNode->IsA("vtkMRMLAnnotationNode")))
  {
    /// issue 2666: don't manage annotation nodes - don't show lines between the control points
    return false;
  }
  if (modelNode && modelNode->GetMesh())
  {
    return true;
  }
  // Maybe a model node has no mesh but its display nodes have output
  //  (e.g. vtkMRMLGlyphableVolumeSliceDisplayNode).
  bool displayable = false;
  for (int i = 0; i < node->GetNumberOfDisplayNodes(); ++i)
  {
    displayable |= this->IsModelDisplayable(node->GetNthDisplayNode(i));
    if (displayable)
    {
      // Optimization: no need to search any further.
      break;
    }
  }
  return displayable;
}

//---------------------------------------------------------------------------
bool vtkMRMLModelDisplayableManager::IsModelDisplayable(vtkMRMLDisplayNode* node) const
{
  vtkMRMLModelDisplayNode* modelDisplayNode = vtkMRMLModelDisplayNode::SafeDownCast(node);
  if (!modelDisplayNode)
  {
    return false;
  }
  if (modelDisplayNode->IsA("vtkMRMLAnnotationDisplayNode"))
  {
    /// issue 2666: don't manage annotation nodes - don't show lines between the control points
    return false;
  }
  return modelDisplayNode->GetOutputMesh() ? true : false;
}

//---------------------------------------------------------------------------
bool vtkMRMLModelDisplayableManager::OnMRMLDisplayableModelNodeModifiedEvent(vtkMRMLDisplayableNode* modelNode)
{
  if (!modelNode)
  {
    vtkErrorMacro("OnMRMLDisplayableModelNodeModifiedEvent: No model node given");
    return false;
  }

  // If the node is already cached with an actor process only this one
  // If it was not visible and is still not visible do nothing
  int ndnodes = modelNode->GetNumberOfDisplayNodes();
  bool updateModel = false;
  bool updateMRML = false;
  bool modelDisplayable = this->IsModelDisplayable(modelNode);
  for (int i = 0; i < ndnodes; i++)
  {
    vtkMRMLDisplayNode* dnode = modelNode->GetNthDisplayNode(i);
    if (dnode == nullptr)
    {
      // display node has been removed
      updateMRML = true;
      break;
    }
    bool visible = modelDisplayable && //
                   (dnode->GetVisibility() == 1) && (dnode->GetVisibility3D() == 1) && this->IsModelDisplayable(dnode);
    bool hasActor = this->Internal->DisplayedActors.find(dnode->GetID()) != this->Internal->DisplayedActors.end();
    // If the displayNode is visible and doesn't have actors yet, then request
    // an updated
    if (visible && !hasActor)
    {
      updateMRML = true;
      break;
    }
    // If the displayNode visibility has changed or displayNode is visible, then
    // update the model.
    if (!(!visible && this->GetDisplayedModelsVisibility(dnode) == 0))
    {
      updateModel = true;
      break;
    }
  }
  if (updateModel)
  {
    this->UpdateModifiedModel(modelNode);
  }
  if (updateMRML)
  {
    this->SetUpdateFromMRMLRequested(true);
  }
  return updateModel || updateMRML;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UpdateFromMRML()
{
  if (this->GetInteractor()                       //
      && this->GetInteractor()->GetRenderWindow() //
      && this->GetInteractor()->GetRenderWindow()->CheckInRenderStatus())
  {
    vtkDebugMacro("skipping update during render");
    return;
  }

  this->RemoveModelProps();

  this->UpdateModelsFromMRML();

  this->SetUpdateFromMRMLRequested(false);
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UpdateModelsFromMRML()
{
  // UpdateModelsFromMRML may recursively trigger calling of UpdateModelsFromMRML
  // via node reference updates. IsUpdatingModelsFromMRML flag prevents restarting
  // UpdateModelsFromMRML if it is already in progress.
  if (this->Internal->IsUpdatingModelsFromMRML)
  {
    return;
  }
  this->Internal->IsUpdatingModelsFromMRML = true;
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkMRMLNode* node = nullptr;
  std::vector<vtkMRMLDisplayableNode*> slices;
  std::vector<vtkMRMLDisplayableNode*> nonSlices;

  // find volume slices
  bool clearDisplayedModels = scene ? false : true;

  std::vector<vtkMRMLNode*> dnodes;
  int nnodes = scene ? scene->GetNodesByClass("vtkMRMLDisplayableNode", dnodes) : 0;
  for (int n = 0; n < nnodes; n++)
  {
    node = dnodes[n];
    vtkMRMLDisplayableNode* model = vtkMRMLDisplayableNode::SafeDownCast(node);
    // render slices last so that transparent objects are rendered in front of them
    if (vtkMRMLSliceLogic::IsSliceModelNode(model))
    {
      slices.push_back(model);

      int ndnodes = model->GetNumberOfDisplayNodes();
      for (int i = 0; i < ndnodes && !clearDisplayedModels; i++)
      {
        vtkMRMLDisplayNode* dnode = model->GetNthDisplayNode(i);
        if (dnode && this->Internal->DisplayedActors.find(dnode->GetID()) == this->Internal->DisplayedActors.end())
        {
          // it is a new slice display node, therefore we need to remove all existing model node actors
          // and insert this slice actor before them
          clearDisplayedModels = true;
          break;
        }
      }
    }
    else
    {
      nonSlices.push_back(model);
    }
  }

  if (clearDisplayedModels)
  {
    for (std::pair<const std::string, vtkProp3D*> iter : this->Internal->DisplayedActors)
    {
      this->GetRenderer()->RemoveViewProp(iter.second);
    }
    this->RemoveModelObservers(1);
    this->ClearDisplayMaps();
  }

  // render slices first
  for (vtkMRMLDisplayableNode* model : slices)
  {
    // add nodes that are not in the list yet
    int ndnodes = model->GetNumberOfDisplayNodes();
    for (int i = 0; i < ndnodes; i++)
    {
      vtkMRMLDisplayNode* dnode = model->GetNthDisplayNode(i);
      if (dnode && this->Internal->DisplayedActors.find(dnode->GetID()) == this->Internal->DisplayedActors.end())
      {
        this->UpdateModel(model);
        break;
      }
    }
    this->SetModelDisplayProperty(model);
  }

  // render the rest of the models
  for (vtkMRMLDisplayableNode* model : nonSlices)
  {
    this->UpdateModifiedModel(model);
  }
  this->Internal->IsUpdatingModelsFromMRML = false;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::ClearDisplayMaps()
{
  if (this->GetRenderer())
  {
    for (auto iter = this->Internal->DisplayedActors.begin(); iter != this->Internal->DisplayedActors.end(); iter++)
    {
      this->GetRenderer()->RemoveViewProp(iter->second);
    }
  }
  this->Internal->DisplayedActors.clear();
  this->Internal->DisplayedNodes.clear();
  this->Internal->DisplayedClipState.clear();
  this->Internal->DisplayableNodes.clear();
  for (auto iter = this->Internal->DisplayNodeTransformFilters.begin(); iter != this->Internal->DisplayNodeTransformFilters.end(); iter++)
  {
    vtkTransformFilter* transformFilter = iter->second;
    transformFilter->SetInputConnection(nullptr);
    transformFilter->SetTransform(nullptr);
  }
  this->Internal->DisplayNodeTransformFilters.clear();
  this->Internal->Clippers.clear();
  this->Internal->Cappers.clear();
  if (this->GetRenderer())
  {
    for (auto iter = this->Internal->DisplayedCapActors.begin(); iter != this->Internal->DisplayedCapActors.end(); iter++)
    {
      this->GetRenderer()->RemoveViewProp(iter->second);
    }
  }
  this->Internal->DisplayedCapActors.clear();
  for (auto iter = this->Internal->DisplayNodeCapTransformFilters.begin(); iter != this->Internal->DisplayNodeCapTransformFilters.end(); iter++)
  {
    vtkTransformFilter* transformFilter = iter->second;
    transformFilter->SetInputConnection(nullptr);
    transformFilter->SetTransform(nullptr);
  }
  this->Internal->DisplayNodeCapTransformFilters.clear();
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UpdateModifiedModel(vtkMRMLDisplayableNode* model)
{
  this->UpdateModel(model);
  this->SetModelDisplayProperty(model);
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UpdateModelMesh(vtkMRMLDisplayableNode* displayableNode)
{
  int ndnodes = displayableNode->GetNumberOfDisplayNodes();
  int i;

  // if no model display nodes found, return
  int modelDisplayNodeCount = 0;
  for (i = 0; i < ndnodes; i++)
  {
    vtkMRMLDisplayNode* dNode = displayableNode->GetNthDisplayNode(i);
    if (vtkMRMLModelDisplayNode::SafeDownCast(dNode) != nullptr)
    {
      modelDisplayNodeCount++;
    }
  }
  if (modelDisplayNodeCount == 0)
  {
    return;
  }

  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(displayableNode);
  vtkMRMLDisplayNode* hdnode = vtkMRMLFolderDisplayNode::GetOverridingHierarchyDisplayNode(displayableNode);
  vtkMRMLModelNode* hierarchyModelDisplayNode = vtkMRMLModelNode::SafeDownCast(hdnode);

  bool hasNonLinearTransform = false;
  vtkMRMLTransformNode* tnode = displayableNode->GetParentTransformNode();
  vtkSmartPointer<vtkGeneralTransform> worldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  worldTransform->Identity();
  if (tnode != nullptr && !tnode->IsTransformToWorldLinear())
  {
    hasNonLinearTransform = true;
    tnode->GetTransformToWorld(worldTransform);
  }

  for (i = 0; i < ndnodes; i++)
  {
    vtkMRMLDisplayNode* displayNode = displayableNode->GetNthDisplayNode(i);
    vtkMRMLModelDisplayNode* modelDisplayNode = vtkMRMLModelDisplayNode::SafeDownCast(displayNode);

    // don't do anything if display node is invalid or it is a color legend
    if (!displayNode || (displayNode && displayNode->IsA("vtkMRMLColorLegendDisplayNode")))
    {
      continue;
    }

    vtkSmartPointer<vtkProp3D> prop = nullptr;
    vtkSmartPointer<vtkProp3D> capProp = nullptr;

    int clipping = displayNode->GetClipping();
    vtkAlgorithmOutput* meshConnection = nullptr;
    if (this->IsModelDisplayable(modelDisplayNode))
    {
      meshConnection = modelDisplayNode->GetOutputMeshConnection();
    }
    if (hdnode)
    {
      clipping = hdnode->GetClipping();
      meshConnection = hierarchyModelDisplayNode ? hierarchyModelDisplayNode->GetMeshConnection() : meshConnection;
    }
    // hierarchy display nodes may not have mesh pointer
    if (meshConnection == nullptr && this->IsModelDisplayable(modelNode))
    {
      meshConnection = modelNode ? modelNode->GetMeshConnection() : nullptr;
    }
    bool hasMesh = (meshConnection != nullptr);

    if (!hasMesh)
    {
      continue;
    }

    vtkMRMLClipNode* clipNode = modelDisplayNode ? modelDisplayNode->GetClipNode() : nullptr;
    int numberOfClipNodes = clipNode ? clipNode->GetNumberOfClippingNodes() : 0;
    if (clipping && clipNode)
    {
      bool allClippingOff = true;
      for (int i = 0; i < numberOfClipNodes; ++i)
      {
        if (clipNode->GetNthClippingNodeState(i) != vtkMRMLClipNode::ClipOff)
        {
          allClippingOff = false;
          break;
        }
      }
      if (allClippingOff)
      {
        clipping = false;
      }
    }

    bool filterUpdateNeeded = false;
    vtkMRMLModelNode::MeshTypeHint meshType = modelNode ? modelNode->GetMeshType() : vtkMRMLModelNode::PolyDataMeshType;

    vtkAlgorithm* clipper = nullptr;
    vtkSmartPointer<vtkImplicitBoolean> implicitBoolean;
    if (clipping && modelDisplayNode && clipNode)
    {
      vtkImplicitFunction* implicitFunction = clipNode->GetImplicitFunctionWorld();
      if (implicitFunction)
      {
        implicitBoolean = vtkSmartPointer<vtkImplicitBoolean>::New();
        implicitBoolean->AddFunction(implicitFunction);

        vtkSmartPointer<vtkAlgorithm> oldClipper = nullptr;
        if (this->Internal->Clippers.find(modelDisplayNode->GetID()) != this->Internal->Clippers.end())
        {
          oldClipper = this->Internal->Clippers[modelDisplayNode->GetID()];
        }
        clipper = this->GetClipper(modelDisplayNode, meshType, implicitBoolean, clipNode->GetClippingMethod());
        filterUpdateNeeded = oldClipper != clipper;
      }

      if (tnode && !hasNonLinearTransform)
      {
        // If the transform is non-linear, worldTransform will have already been set.
        // Only need to calculate here for linear transforms.
        tnode->GetTransformToWorld(worldTransform);
        implicitBoolean->SetTransform(worldTransform);
      }
    }

    // create TransformFilter for non-linear transform
    vtkSmartPointer<vtkTransformFilter> transformFilter = nullptr;
    if (hasNonLinearTransform)
    {
      auto tit = this->Internal->DisplayNodeTransformFilters.find(displayNode->GetID());
      if (tit == this->Internal->DisplayNodeTransformFilters.end())
      {
        transformFilter = vtkSmartPointer<vtkTransformFilter>::New();
        this->Internal->DisplayNodeTransformFilters[displayNode->GetID()] = transformFilter;
      }
      else
      {
        transformFilter = tit->second;
      }
    }

    if (transformFilter)
    {
      transformFilter->SetInputConnection(meshConnection);
      // It is important to only update the transform if the transform chain is actually changed,
      // because recomputing a non-linear transformation on a complex model may be very time-consuming.
      if (!vtkMRMLTransformNode::AreTransformsEqual(worldTransform, transformFilter->GetTransform()))
      {
        transformFilter->SetTransform(worldTransform);
      }
    }

    auto ait = this->Internal->DisplayedActors.find(displayNode->GetID());
    if (ait == this->Internal->DisplayedActors.end())
    {
      if (!prop)
      {
        prop = vtkSmartPointer<vtkActor>::New();
      }
    }
    else
    {
      prop = ait->second;
      auto cit = this->Internal->DisplayedClipState.end();
      if (modelDisplayNode)
      {
        cit = this->Internal->DisplayedClipState.find(modelDisplayNode->GetID());
      }
      if (cit != this->Internal->DisplayedClipState.end() && cit->second == clipping)
      {
        // make sure that we are looking at the current mesh (most of the code in here
        // assumes a display node will never change what mesh it wants to view and hence
        // caches information to skip steps if the display node has already rendered. but we
        // can have rendered a display node but not rendered its current mesh.
        vtkActor* actor = vtkActor::SafeDownCast(prop);
        bool mapperUpdateNeeded = true; // mapper might not match the mesh type
        if (actor && actor->GetMapper())
        {
          vtkMapper* mapper = actor->GetMapper();
          if (transformFilter)
          {
            mapper->SetInputConnection(transformFilter->GetOutputPort());
          }
          else if (mapper && !clipping)
          {
            mapper->SetInputConnection(meshConnection);
          }
          if ((meshType == vtkMRMLModelNode::UnstructuredGridMeshType && mapper->IsA("vtkDataSetMapper")) //
              || (meshType == vtkMRMLModelNode::PolyDataMeshType && mapper->IsA("vtkPolyDataMapper")))
          {
            mapperUpdateNeeded = false;
          }
        }

        vtkMRMLTransformNode* tnode = displayableNode->GetParentTransformNode();
        if ((!clipping || tnode == nullptr) && !mapperUpdateNeeded && !filterUpdateNeeded)
        {
          continue;
        }
      }
    }

    auto cait = this->Internal->DisplayedCapActors.find(displayNode->GetID());
    if (cait == this->Internal->DisplayedCapActors.end())
    {
      if (!capProp)
      {
        capProp = vtkSmartPointer<vtkActor>::New();
      }
    }
    else
    {
      capProp = cait->second;
    }

    vtkSmartPointer<vtkActor> actor = vtkActor::SafeDownCast(prop);
    if (actor)
    {
      vtkSmartPointer<vtkMapper> mapper = nullptr;
      if (meshType == vtkMRMLModelNode::UnstructuredGridMeshType)
      {
        mapper = vtkSmartPointer<vtkDataSetMapper>::New();
      }
      else // if (meshType == vtkMRMLModelNode::PolyDataMeshType) // unknown when new. need to set type
      {
        mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
      }

      if (clipper)
      {
        if (transformFilter)
        {
          clipper->SetInputConnection(transformFilter->GetOutputPort());
        }
        else
        {
          clipper->SetInputConnection(meshConnection);
        }

        vtkSmartPointer<vtkCapPolyData> capFilter = nullptr;
        auto capIter = this->Internal->Cappers.find(displayNode->GetID());
        if (capIter != this->Internal->Cappers.end())
        {
          capFilter = capIter->second;
        }
        if (!capFilter)
        {
          capFilter = vtkSmartPointer<vtkCapPolyData>::New();
          this->Internal->Cappers[displayNode->GetID()] = capFilter;
        }
        capFilter->SetClipFunction(implicitBoolean);

        if (transformFilter)
        {
          capFilter->SetInputConnection(transformFilter->GetOutputPort());
        }
        else
        {
          capFilter->SetInputConnection(meshConnection);
        }

        mapper->SetInputConnection(clipper->GetOutputPort());
      }
      else if (transformFilter)
      {
        mapper->SetInputConnection(transformFilter->GetOutputPort());
      }
      else
      {
        mapper->SetInputConnection(meshConnection);
      }

      actor->SetMapper(mapper);
    }

    vtkActor* capActor = vtkActor::SafeDownCast(capProp);
    if (capActor)
    {
      vtkSmartPointer<vtkCapPolyData> capFilter = nullptr;
      auto capIter = this->Internal->Cappers.find(displayNode->GetID());
      if (capIter != this->Internal->Cappers.end())
      {
        capFilter = capIter->second;
      }
      if (capFilter)
      {
        vtkSmartPointer<vtkMapper> capMapper = nullptr;
        if (meshType == vtkMRMLModelNode::UnstructuredGridMeshType)
        {
          capMapper = vtkSmartPointer<vtkDataSetMapper>::New();
        }
        else // if (meshType == vtkMRMLModelNode::PolyDataMeshType) // unknown when new. need to set type
        {
          capMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        }

        capMapper->SetInputConnection(clipping ? capFilter->GetOutputPort() : nullptr);
        capActor->SetMapper(capMapper);
      }
    }

    if (hasMesh && modelDisplayNode && ait == this->Internal->DisplayedActors.end())
    {
      this->GetRenderer()->AddViewProp(prop);
      this->Internal->DisplayedActors[modelDisplayNode->GetID()] = prop;
      this->Internal->DisplayedNodes[std::string(modelDisplayNode->GetID())] = modelDisplayNode;

      if (clipper)
      {
        this->Internal->DisplayedClipState[modelDisplayNode->GetID()] = 1;
      }
      else
      {
        this->Internal->DisplayedClipState[modelDisplayNode->GetID()] = 0;
      }
    }
    else if (modelDisplayNode)
    {
      if (clipper)
      {
        this->Internal->DisplayedClipState[modelDisplayNode->GetID()] = 1;
      }
      else
      {
        this->Internal->DisplayedClipState[modelDisplayNode->GetID()] = 0;
      }
    }

    if (hasMesh && modelDisplayNode && cait == this->Internal->DisplayedCapActors.end())
    {
      this->GetRenderer()->AddViewProp(capProp);
      this->Internal->DisplayedCapActors[modelDisplayNode->GetID()] = capProp;
    }
  }
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UpdateModel(vtkMRMLDisplayableNode* model)
{
  this->UpdateModelMesh(model);

  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  vtkEventBroker::ObservationVector observations;
  // observe mesh;
  if (!broker->GetObservationExist(model, vtkMRMLModelNode::MeshModifiedEvent, this, this->GetMRMLNodesCallbackCommand()))
  {
    broker->AddObservation(model, vtkMRMLModelNode::MeshModifiedEvent, this, this->GetMRMLNodesCallbackCommand());
    this->Internal->DisplayableNodes[model->GetID()] = model;
  }
  // observe display node
  if (!broker->GetObservationExist(model, vtkMRMLDisplayableNode::DisplayModifiedEvent, this, this->GetMRMLNodesCallbackCommand()))
  {
    broker->AddObservation(model, vtkMRMLDisplayableNode::DisplayModifiedEvent, this, this->GetMRMLNodesCallbackCommand());
  }

  if (!broker->GetObservationExist(model, vtkMRMLTransformableNode::TransformModifiedEvent, this, this->GetMRMLNodesCallbackCommand()))
  {
    broker->AddObservation(model, vtkMRMLTransformableNode::TransformModifiedEvent, this, this->GetMRMLNodesCallbackCommand());
  }

  if (!broker->GetObservationExist(model, vtkMRMLClipNode::ClipNodeModifiedEvent, this, this->GetMRMLNodesCallbackCommand()))
  {
    broker->AddObservation(model, vtkMRMLClipNode::ClipNodeModifiedEvent, this, this->GetMRMLNodesCallbackCommand());
  }
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::RemoveModelProps()
{
  std::vector<std::string> removedIDs;
  for (auto iter = this->Internal->DisplayedActors.begin(); iter != this->Internal->DisplayedActors.end(); iter++)
  {
    vtkMRMLDisplayNode* modelDisplayNode = vtkMRMLDisplayNode::SafeDownCast(this->GetMRMLScene() ? this->GetMRMLScene()->GetNodeByID(iter->first) : nullptr);
    if (modelDisplayNode == nullptr)
    {
      removedIDs.push_back(iter->first);
    }
    else
    {
      int clipModel = 0;
      if (modelDisplayNode != nullptr)
      {
        clipModel = modelDisplayNode->GetClipping();
      }
      auto clipIter = this->Internal->DisplayedClipState.find(iter->first);
      if (clipIter == this->Internal->DisplayedClipState.end())
      {
        vtkErrorMacro("vtkMRMLModelDisplayableManager::RemoveModelProps() Unknown clip state\n");
      }
      else
      {

        if (clipIter->second || (modelDisplayNode->GetClipping() && clipIter->second != clipModel))
        {
          removedIDs.push_back(iter->first);
        }
      }
    }
  }

  for (unsigned int i = 0; i < removedIDs.size(); i++)
  {
    this->RemoveDisplayedID(removedIDs[i]);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::RemoveDisplayable(vtkMRMLDisplayableNode* model)
{
  if (!model)
  {
    return;
  }
  const int ndnodes = model->GetNumberOfDisplayNodes();
  std::vector<std::string> removedIDs;
  for (int i = 0; i < ndnodes; i++)
  {
    const char* displayNodeIDToRemove = model->GetNthDisplayNodeID(i);
    if (!displayNodeIDToRemove)
    {
      continue;
    }
    this->RemoveDisplayedID(std::string(displayNodeIDToRemove));
  }

  this->RemoveDisplayableNodeObservers(model);
  this->Internal->DisplayableNodes.erase(model->GetID());
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::RemoveDisplayedID(const std::string& id)
{
  auto actorIter = this->Internal->DisplayedActors.find(id);
  if (actorIter != this->Internal->DisplayedActors.end())
  {
    this->GetRenderer()->RemoveViewProp(actorIter->second);
    this->Internal->DisplayedActors.erase(actorIter);
  }

  auto modelIter = this->Internal->DisplayedNodes.find(id);
  if (modelIter != this->Internal->DisplayedNodes.end())
  {
    this->Internal->DisplayedNodes.erase(modelIter);
  }

  auto clipStateIter = this->Internal->DisplayedClipState.find(id);
  if (clipStateIter != this->Internal->DisplayedClipState.end())
  {
    this->Internal->DisplayedClipState.erase(clipStateIter);
  }

  auto displayNodeTransformFilterIter = this->Internal->DisplayNodeTransformFilters.find(id);
  if (displayNodeTransformFilterIter != this->Internal->DisplayNodeTransformFilters.end())
  {
    displayNodeTransformFilterIter->second->SetInputConnection(nullptr);
    displayNodeTransformFilterIter->second->SetTransform(nullptr);
    this->Internal->DisplayNodeTransformFilters.erase(displayNodeTransformFilterIter);
  }

  auto clipperIter = this->Internal->Clippers.find(id);
  if (clipperIter != this->Internal->Clippers.end())
  {
    this->Internal->Clippers.erase(clipperIter);
  }

  auto capIter = this->Internal->Cappers.find(id);
  if (capIter != this->Internal->Cappers.end())
  {
    this->Internal->Cappers.erase(capIter);
  }

  auto capActorIter = this->Internal->DisplayedCapActors.find(id);
  if (capActorIter != this->Internal->DisplayedCapActors.end())
  {
    this->GetRenderer()->RemoveViewProp(capActorIter->second);
    this->Internal->DisplayedCapActors.erase(capActorIter);
  }

  auto capTransformFilterIter = this->Internal->DisplayNodeCapTransformFilters.find(id);
  if (capTransformFilterIter != this->Internal->DisplayNodeCapTransformFilters.end())
  {
    capTransformFilterIter->second->SetInputConnection(nullptr);
    capTransformFilterIter->second->SetTransform(nullptr);
    this->Internal->DisplayNodeCapTransformFilters.erase(capTransformFilterIter);
  }
}

//---------------------------------------------------------------------------
int vtkMRMLModelDisplayableManager::GetDisplayedModelsVisibility(vtkMRMLDisplayNode* displayNode)
{
  if (!displayNode)
  {
    vtkErrorMacro("GetDisplayedModelsVisibility: No display node given");
    return 0;
  }

  auto it = this->Internal->DisplayedActors.find(displayNode->GetID());
  if (it == this->Internal->DisplayedActors.end())
  {
    return 0;
  }

  vtkProp3D* actor = it->second;
  return actor->GetVisibility();
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::RemoveMRMLObservers()
{
  this->RemoveModelObservers(1);

  this->Superclass::RemoveMRMLObservers();
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::RemoveModelObservers(int clearCache)
{
  for (auto iter = this->Internal->DisplayableNodes.begin(); iter != this->Internal->DisplayableNodes.end(); iter++)
  {
    this->RemoveDisplayableNodeObservers(iter->second);
  }
  if (clearCache)
  {
    this->ClearDisplayMaps();
  }
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::RemoveDisplayableNodeObservers(vtkMRMLDisplayableNode* model)
{
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  vtkEventBroker::ObservationVector observations;
  if (model != nullptr)
  {
    observations = broker->GetObservations(model, vtkMRMLModelNode::MeshModifiedEvent, this, this->GetMRMLNodesCallbackCommand());
    broker->RemoveObservations(observations);
    observations = broker->GetObservations(model, vtkMRMLDisplayableNode::DisplayModifiedEvent, this, this->GetMRMLNodesCallbackCommand());
    broker->RemoveObservations(observations);
    observations = broker->GetObservations(model, vtkMRMLTransformableNode::TransformModifiedEvent, this, this->GetMRMLNodesCallbackCommand());
    broker->RemoveObservations(observations);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::SetModelDisplayProperty(vtkMRMLDisplayableNode* model)
{
  // Get transformation applied on model
  vtkMRMLTransformNode* transformNode = model->GetParentTransformNode();
  vtkNew<vtkMatrix4x4> matrixTransformToWorld;
  if (transformNode != nullptr && transformNode->IsTransformToWorldLinear())
  {
    transformNode->GetMatrixTransformToWorld(matrixTransformToWorld.GetPointer());
  }

  // Get display node from hierarchy that applies display properties on branch
  vtkMRMLDisplayNode* overrideHierarchyDisplayNode = vtkMRMLFolderDisplayNode::GetOverridingHierarchyDisplayNode(model);

  // Set display properties to props for all display nodes
  int numberOfDisplayNodes = model->GetNumberOfDisplayNodes();
  for (int i = 0; i < numberOfDisplayNodes; i++)
  {
    vtkMRMLDisplayNode* displayNode = model->GetNthDisplayNode(i);
    vtkMRMLModelDisplayNode* modelDisplayNode = vtkMRMLModelDisplayNode::SafeDownCast(displayNode);
    if (!modelDisplayNode)
    {
      continue;
    }
    vtkProp3D* prop = this->GetActorByID(modelDisplayNode->GetID());
    if (prop == nullptr)
    {
      continue;
    }

    vtkProp3D* capProp = nullptr;
    auto capPropIter = this->Internal->DisplayedCapActors.find(modelDisplayNode->GetID());
    if (capPropIter != this->Internal->DisplayedCapActors.end())
    {
      capProp = capPropIter->second;
    }

    // Use hierarchy display node if any, and if overriding is allowed for the current display node.
    // If override is explicitly disabled, then do not apply hierarchy visibility or opacity either.
    bool hierarchyVisibility = true;
    double hierarchyOpacity = 1.0;
    if (displayNode->GetFolderDisplayOverrideAllowed())
    {
      if (overrideHierarchyDisplayNode)
      {
        displayNode = overrideHierarchyDisplayNode;
      }

      // Get visibility and opacity defined by the hierarchy.
      // These two properties are influenced by the hierarchy regardless the fact whether there is override
      // or not. Visibility of items defined by hierarchy is off if any of the ancestors is explicitly hidden,
      // and the opacity is the product of the ancestors' opacities.
      // However, this does not apply on display nodes that do not allow overrides (FolderDisplayOverrideAllowed)
      hierarchyVisibility = vtkMRMLFolderDisplayNode::GetHierarchyVisibility(model);
      hierarchyOpacity = vtkMRMLFolderDisplayNode::GetHierarchyOpacity(model);
    }

    vtkActor* actor = vtkActor::SafeDownCast(prop);
    vtkActor* capActor = vtkActor::SafeDownCast(capProp);
    if (capProp)
    {
      capProp->SetUserMatrix(matrixTransformToWorld);
    }

    vtkImageActor* imageActor = vtkImageActor::SafeDownCast(prop);
    prop->SetUserMatrix(matrixTransformToWorld);

    // If there is an overriding hierarchy display node, then consider its visibility as well
    // as the model's. It is important to consider the model's visibility, because the user will
    // still want to show/hide children regardless of application of display properties from the
    // hierarchy.
    bool visible = hierarchyVisibility                                                         //
                   && modelDisplayNode->GetVisibility() && modelDisplayNode->GetVisibility3D() //
                   && modelDisplayNode->IsDisplayableInView(this->GetMRMLViewNode()->GetID());
    prop->SetVisibility(visible);
    bool capVisible = visible                                       //
                      && modelDisplayNode->GetClipping()            //
                      && modelDisplayNode->GetClipNode() != nullptr //
                      && (modelDisplayNode->GetClippingCapSurface() || modelDisplayNode->GetClippingOutline());
    if (capActor)
    {
      capActor->SetVisibility(capVisible);
    }
    double opacity = hierarchyOpacity * displayNode->GetOpacity();

    if (visible && actor)
    {
      this->UpdateMapperProperties(vtkMRMLModelNode::SafeDownCast(model), displayNode, actor->GetMapper());
      this->UpdateActorProperties(vtkMRMLModelNode::SafeDownCast(model), modelDisplayNode, displayNode, actor, opacity);
    }

    if (capVisible && capActor)
    {
      this->UpdateMapperProperties(vtkMRMLModelNode::SafeDownCast(model), displayNode, capActor->GetMapper());
      this->UpdateCapActorProperties(vtkMRMLModelNode::SafeDownCast(model), modelDisplayNode, displayNode, capActor, opacity);
    }

    if (imageActor)
    {
      imageActor->GetMapper()->SetInputConnection(displayNode->GetTextureImageDataConnection());
      imageActor->SetDisplayExtent(-1, 0, 0, 0, 0, 0);
    }
  }
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UpdateMapperProperties(vtkMRMLModelNode* modelNode, vtkMRMLDisplayNode* displayNode, vtkMapper* mapper)
{
  if (!mapper)
  {
    return;
  }

  mapper->SetScalarVisibility(displayNode->GetScalarVisibility());
  // if the scalars are visible, set active scalars, the lookup table
  // and the scalar range
  if (displayNode->GetScalarVisibility())
  {
    // Check if using point data or cell data
    if (this->IsCellScalarsActive(displayNode, modelNode))
    {
      mapper->SetScalarModeToUseCellData();
    }
    else
    {
      mapper->SetScalarModeToUsePointData();
    }

    if (displayNode->GetScalarRangeFlag() == vtkMRMLDisplayNode::UseDirectMapping)
    {
      mapper->UseLookupTableScalarRangeOn(); // avoid warning about bad table range
      mapper->SetColorModeToDirectScalars();
      mapper->SetLookupTable(nullptr);
    }
    else
    {
      mapper->UseLookupTableScalarRangeOff();
      mapper->SetColorModeToMapScalars();

      // The renderer uses the lookup table scalar range to
      // render colors. By default, UseLookupTableScalarRange
      // is set to false and SetScalarRange can be used on the
      // mapper to map scalars into the lookup table. When set
      // to true, SetScalarRange has no effect and it is necessary
      // to force the scalarRange on the lookup table manually.
      // Whichever way is used, the look up table range needs
      // to be changed to render the correct scalar values, thus
      // one lookup table can not be shared by multiple mappers
      // if any of those mappers needs to map using its scalar
      // values range. It is therefore necessary to make a copy
      // of the colorNode vtkLookupTable in order not to impact
      // that lookup table original range.
      vtkSmartPointer<vtkLookupTable> dNodeLUT =
        vtkSmartPointer<vtkLookupTable>::Take(displayNode->GetColorNode() ? displayNode->GetColorNode()->CreateLookupTableCopy() : nullptr);
      mapper->SetLookupTable(dNodeLUT);
    }

    // Set scalar range
    mapper->SetScalarRange(displayNode->GetScalarRange());
  }
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UpdateActorProperties(vtkMRMLModelNode* modelNode,
                                                           vtkMRMLModelDisplayNode* modelDisplayNode,
                                                           vtkMRMLDisplayNode* displayNode,
                                                           vtkActor* actor,
                                                           double opacity)
{
  if (!modelNode || !modelDisplayNode || !actor)
  {
    return;
  }

  if (!displayNode)
  {
    displayNode = modelDisplayNode;
  }

  vtkProperty* actorProperties = actor->GetProperty();
  actorProperties->SetRepresentation(displayNode->GetRepresentation());
  actorProperties->SetPointSize(displayNode->GetPointSize());
  actorProperties->SetLineWidth(displayNode->GetLineWidth());
  actorProperties->SetLighting(displayNode->GetLighting());
  actorProperties->SetInterpolation(displayNode->GetInterpolation());
  actorProperties->SetShading(displayNode->GetShading());
  actorProperties->SetFrontfaceCulling(displayNode->GetFrontfaceCulling());
  actorProperties->SetBackfaceCulling(displayNode->GetBackfaceCulling());

  actor->SetPickable(modelNode->GetSelectable());
  if (displayNode->GetSelected())
  {
    actorProperties->SetColor(displayNode->GetSelectedColor());
    actorProperties->SetAmbient(displayNode->GetSelectedAmbient());
    actorProperties->SetSpecular(displayNode->GetSelectedSpecular());
  }
  else
  {
    actorProperties->SetColor(displayNode->GetColor());
    actorProperties->SetAmbient(displayNode->GetAmbient());
    actorProperties->SetSpecular(displayNode->GetSpecular());
  }
  // Opacity will be the product of the opacities of the model and the overriding
  // hierarchy, in order to keep the relative opacities the same.
  actorProperties->SetOpacity(opacity);
  actorProperties->SetDiffuse(displayNode->GetDiffuse());
  actorProperties->SetSpecularPower(displayNode->GetPower());
  actorProperties->SetMetallic(displayNode->GetMetallic());
  actorProperties->SetRoughness(displayNode->GetRoughness());
  actorProperties->SetEdgeVisibility(displayNode->GetEdgeVisibility());
  actorProperties->SetEdgeColor(displayNode->GetEdgeColor());

  if (displayNode->GetTextureImageDataConnection() != nullptr)
  {
    if (actor->GetTexture() == nullptr)
    {
      vtkNew<vtkTexture> texture;
      actor->SetTexture(texture);
    }
    actor->GetTexture()->SetInputConnection(displayNode->GetTextureImageDataConnection());
    actor->GetTexture()->SetInterpolate(displayNode->GetInterpolateTexture());
    actorProperties->SetColor(1., 1., 1.);

    // Force actors to be treated as opaque. Otherwise, transparent
    // elements in the texture cause the actor to be treated as
    // translucent, i.e. rendered without writing to the depth buffer.
    // See https://github.com/Slicer/Slicer/issues/4253.
    actor->SetForceOpaque(actorProperties->GetOpacity() >= 1.0);
  }
  else
  {
    actor->SetTexture(nullptr);
    actor->ForceOpaqueOff();
  }

  // Set backface properties
  vtkProperty* actorBackfaceProperties = actor->GetBackfaceProperty();
  if (!actorBackfaceProperties)
  {
    vtkNew<vtkProperty> newActorBackfaceProperties;
    actor->SetBackfaceProperty(newActorBackfaceProperties);
    actorBackfaceProperties = newActorBackfaceProperties;
  }
  actorBackfaceProperties->DeepCopy(actorProperties);

  double offsetHsv[3];
  modelDisplayNode->GetBackfaceColorHSVOffset(offsetHsv);

  double colorHsv[3];
  vtkMath::RGBToHSV(actorProperties->GetColor(), colorHsv);
  double colorRgb[3];
  colorHsv[0] += offsetHsv[0];
  // wrap around hue value
  if (colorHsv[0] < 0.0)
  {
    colorHsv[0] += 1.0;
  }
  else if (colorHsv[0] > 1.0)
  {
    colorHsv[0] -= 1.0;
  }
  colorHsv[1] = vtkMath::ClampValue<double>(colorHsv[1] + offsetHsv[1], 0, 1);
  colorHsv[2] = vtkMath::ClampValue<double>(colorHsv[2] + offsetHsv[2], 0, 1);
  vtkMath::HSVToRGB(colorHsv, colorRgb);
  actorBackfaceProperties->SetColor(colorRgb);
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::UpdateCapActorProperties(vtkMRMLModelNode* modelNode,
                                                              vtkMRMLModelDisplayNode* modelDisplayNode,
                                                              vtkMRMLDisplayNode* displayNode,
                                                              vtkActor* capActor,
                                                              double opacity)
{
  if (!displayNode)
  {
    displayNode = modelDisplayNode;
  }

  this->UpdateActorProperties(modelNode, modelDisplayNode, displayNode, capActor, opacity);

  vtkSmartPointer<vtkProperty> capActorProperties = capActor->GetProperty();
  if (!capActorProperties)
  {
    capActorProperties = vtkSmartPointer<vtkProperty>::New();
    capActor->SetProperty(capActorProperties);
  }
  capActorProperties->SetLineWidth(modelDisplayNode->GetLineWidth());

  vtkMapper* capMapper = capActor->GetMapper();
  if (!capMapper)
  {
    return;
  }

  double offsetHsv[3];
  modelDisplayNode->GetClippingCapColorHSVOffset(offsetHsv);

  double colorHsv[3];
  vtkMath::RGBToHSV(capActorProperties->GetColor(), colorHsv);
  colorHsv[0] += offsetHsv[0];
  // wrap around hue value
  if (colorHsv[0] < 0.0)
  {
    colorHsv[0] += 1.0;
  }
  else if (colorHsv[0] > 1.0)
  {
    colorHsv[0] -= 1.0;
  }
  colorHsv[1] = vtkMath::ClampValue<double>(colorHsv[1] + offsetHsv[1], 0.0, 1.0);
  colorHsv[2] = vtkMath::ClampValue<double>(colorHsv[2] + offsetHsv[2], 0.0, 1.0);

  double colorRgb[3];
  vtkMath::HSVToRGB(colorHsv, colorRgb);
  capActorProperties->SetColor(colorRgb);

  bool capSurface = modelDisplayNode ? modelDisplayNode->GetClippingCapSurface() : false;
  bool clipOutline = modelDisplayNode ? modelDisplayNode->GetClippingOutline() : false;

  double capOpacity = capSurface ? opacity * modelDisplayNode->GetClippingCapOpacity() : 0.0;
  double outlineOpacity = clipOutline ? opacity : 0.0;

  double edgeColor[4] = { 1.0, 0.0, 0.0, 1.0 };
  modelDisplayNode->GetEdgeColor(edgeColor);
  edgeColor[3] = outlineOpacity;

  // Create a lookup table to map cell data to colors.
  vtkNew<vtkLookupTable> lut;
  lut->SetTableRange(VTK_LINE, VTK_POLY_LINE);
  lut->SetNumberOfColors(1);
  lut->Build();
  lut->SetTableValue(0, edgeColor);
  lut->UseBelowRangeColorOn();
  lut->UseAboveRangeColorOn();
  lut->SetBelowRangeColor(colorRgb[0], colorRgb[1], colorRgb[2], capOpacity);
  lut->SetAboveRangeColor(colorRgb[0], colorRgb[1], colorRgb[2], capOpacity);

  capMapper->SetLookupTable(lut);
  capMapper->UseLookupTableScalarRangeOn();
  capMapper->SetScalarModeToUseCellData();
  capMapper->SetColorModeToMapScalars();
  capMapper->SetScalarVisibility(true);
}

//---------------------------------------------------------------------------
const char* vtkMRMLModelDisplayableManager::GetActiveScalarName(vtkMRMLDisplayNode* displayNode, vtkMRMLModelNode* modelNode)
{
  const char* activeScalarName = nullptr;
  if (displayNode)
  {
    vtkMRMLModelDisplayNode* modelDisplayNode = vtkMRMLModelDisplayNode::SafeDownCast(displayNode);
    if (modelDisplayNode && modelDisplayNode->GetOutputMesh())
    {
      modelDisplayNode->GetOutputMeshConnection()->GetProducer()->Update();
    }
    activeScalarName = displayNode->GetActiveScalarName();
  }
  if (activeScalarName)
  {
    return activeScalarName;
  }
  if (modelNode)
  {
    if (modelNode->GetMesh())
    {
      vtkAlgorithmOutput* meshConnection = modelNode->GetMeshConnection();
      if (meshConnection != nullptr)
      {
        meshConnection->GetProducer()->Update();
      }
    }
    activeScalarName = modelNode->GetActiveCellScalarName(vtkDataSetAttributes::SCALARS);
    if (activeScalarName)
    {
      return activeScalarName;
    }
    activeScalarName = modelNode->GetActivePointScalarName(vtkDataSetAttributes::SCALARS);
    if (activeScalarName)
    {
      return activeScalarName;
    }
  }
  return nullptr;
}

//---------------------------------------------------------------------------
bool vtkMRMLModelDisplayableManager::IsCellScalarsActive(vtkMRMLDisplayNode* displayNode, vtkMRMLModelNode* modelNode)
{
  if (displayNode && displayNode->GetActiveScalarName())
  {
    return (displayNode->GetActiveAttributeLocation() == vtkAssignAttribute::CELL_DATA);
  }
  if (modelNode && //
      modelNode->GetActiveCellScalarName(vtkDataSetAttributes::SCALARS))
  {
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------
// Description:
// return the current actor corresponding to a give MRML ID
vtkProp3D* vtkMRMLModelDisplayableManager::GetActorByID(const char* id)
{
  if (!id)
  {
    return (nullptr);
  }

  auto iter = this->Internal->DisplayedActors.find(std::string(id));
  if (iter != this->Internal->DisplayedActors.end())
  {
    return iter->second;
  }

  return (nullptr);
}

//---------------------------------------------------------------------------
// Description:
// return the ID for the given actor
const char* vtkMRMLModelDisplayableManager::GetIDByActor(vtkProp3D* actor)
{
  if (!actor)
  {
    return (nullptr);
  }

  for (auto iter = this->Internal->DisplayedActors.begin(); iter != this->Internal->DisplayedActors.end(); iter++)
  {
    if (iter->second && (iter->second == actor))
    {
      return (iter->first.c_str());
    }
  }
  return (nullptr);
}

//---------------------------------------------------------------------------
vtkWorldPointPicker* vtkMRMLModelDisplayableManager::GetWorldPointPicker()
{
  vtkDebugMacro(<< "returning Internal->WorldPointPicker address " << this->Internal->WorldPointPicker.GetPointer());
  return this->Internal->WorldPointPicker;
}

//---------------------------------------------------------------------------
vtkPropPicker* vtkMRMLModelDisplayableManager::GetPropPicker()
{
  vtkDebugMacro(<< "returning Internal->PropPicker address " << this->Internal->PropPicker.GetPointer());
  return this->Internal->PropPicker;
}

//---------------------------------------------------------------------------
vtkCellPicker* vtkMRMLModelDisplayableManager::GetCellPicker()
{
  vtkDebugMacro(<< "returning Internal->CellPicker address " << this->Internal->CellPicker.GetPointer());
  return this->Internal->CellPicker;
}

//---------------------------------------------------------------------------
vtkPointPicker* vtkMRMLModelDisplayableManager::GetPointPicker()
{
  vtkDebugMacro(<< "returning Internal->PointPicker address " << this->Internal->PointPicker.GetPointer());
  return this->Internal->PointPicker;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::SetPickTolerance(double tolerance)
{
  this->Internal->CellPicker->SetTolerance(tolerance);
}

//---------------------------------------------------------------------------
double vtkMRMLModelDisplayableManager::GetPickTolerance()
{
  return this->Internal->CellPicker->GetTolerance();
}

//---------------------------------------------------------------------------
int vtkMRMLModelDisplayableManager::Pick(int x, int y)
{
  double RASPoint[3] = { 0.0, 0.0, 0.0 };
  double pickPoint[3] = { 0.0, 0.0, 0.0 };

  // Reset the pick vars
  this->Internal->ResetPick();

  vtkRenderer* ren = this->GetRenderer();
  if (!ren)
  {
    vtkErrorMacro("Pick: unable to get renderer\n");
    return 0;
  }
  // get the current renderer's size
  int* renSize = ren->GetSize();
  // resize the interactor?

  // pass the event's display point to the world point picker
  double displayPoint[3];
  displayPoint[0] = x;
  displayPoint[1] = renSize[1] - y;
  displayPoint[2] = 0.0;

  if (this->Internal->CellPicker->Pick(displayPoint[0], displayPoint[1], displayPoint[2], ren))
  {
    this->Internal->CellPicker->GetPickPosition(pickPoint);
    this->SetPickedCellID(this->Internal->CellPicker->GetCellId());

    // look for either picked mesh or volume
    // and set picked display node accordingly
    vtkPointSet* mesh = vtkPointSet::SafeDownCast(this->Internal->CellPicker->GetDataSet());
    if (mesh)
    {
      // get the pointer to the mesh that the cell was in
      // and then find the model this mesh belongs to
      this->Internal->FindPickedDisplayNodeFromMesh(mesh, pickPoint);
    }
    vtkImageData* imageData = vtkImageData::SafeDownCast(this->Internal->CellPicker->GetDataSet());
    if (imageData)
    {
      // get the pointer to the picked imageData
      // and then find the volume this imageData belongs to
      this->Internal->FindDisplayNodeFromImageData(this->GetMRMLScene(), imageData);
    }
  }
  else
  {
    // there may not have been an actor at the picked point, but the Pick should be translated to a valid position
    // TBD: warn the user that they're picking in empty space?
    this->Internal->CellPicker->GetPickPosition(pickPoint);
  }

  // translate world to RAS
  for (int p = 0; p < 3; p++)
  {
    RASPoint[p] = pickPoint[p];
  }

  // now set up the class vars
  this->SetPickedRAS(RASPoint);

  return 1;
}

//---------------------------------------------------------------------------
int vtkMRMLModelDisplayableManager::Pick3D(double ras[3])
{
  // Reset the pick vars
  this->Internal->ResetPick();

  vtkRenderer* ren = this->GetRenderer();
  if (!ren)
  {
    vtkErrorMacro("Pick3D: Unable to get renderer");
    return 0;
  }

  if (this->Internal->CellPicker->Pick3DPoint(ras, ren))
  {
    this->SetPickedCellID(this->Internal->CellPicker->GetCellId());

    // Find first picked model from picker
    // Note: Getting the mesh using GetDataSet is not a good solution as the dataset is the first
    //   one that is picked and it may be of different type (volume, segmentation, etc.)
    this->Internal->FindFirstPickedDisplayNodeFromPickerProp3Ds();
    // Find picked point in mesh
    vtkMRMLModelDisplayNode* displayNode = vtkMRMLModelDisplayNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(this->Internal->PickedDisplayNodeID.c_str()));
    if (displayNode)
    {
      this->Internal->FindPickedPointOnMeshAndCell(displayNode->GetOutputMesh(), ras);
    }

    this->SetPickedRAS(ras);
  }

  return 1;
}

//---------------------------------------------------------------------------
const char* vtkMRMLModelDisplayableManager::GetPickedNodeID()
{
  vtkDebugMacro(<< "returning this->Internal->PickedDisplayNodeID of " << (this->Internal->PickedDisplayNodeID.empty() ? "(empty)" : this->Internal->PickedDisplayNodeID));
  return this->Internal->PickedDisplayNodeID.c_str();
}

//---------------------------------------------------------------------------
double* vtkMRMLModelDisplayableManager::GetPickedRAS()
{
  vtkDebugMacro(<< "returning Internal->PickedRAS pointer " << this->Internal->PickedRAS);
  return this->Internal->PickedRAS;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::SetPickedRAS(double* newPickedRAS)
{
  int i;
  for (i = 0; i < 3; i++)
  {
    if (newPickedRAS[i] != this->Internal->PickedRAS[i])
    {
      break;
    }
  }
  if (i < 3)
  {
    for (i = 0; i < 3; i++)
    {
      this->Internal->PickedRAS[i] = newPickedRAS[i];
    }
    this->Modified();
  }
}

//---------------------------------------------------------------------------
vtkIdType vtkMRMLModelDisplayableManager::GetPickedCellID()
{
  vtkDebugMacro(<< "returning this->Internal->PickedCellID of " << this->Internal->PickedCellID);
  return this->Internal->PickedCellID;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::SetPickedCellID(vtkIdType newCellID)
{
  vtkDebugMacro(<< "setting PickedCellID to " << newCellID);
  if (this->Internal->PickedCellID != newCellID)
  {
    this->Internal->PickedCellID = newCellID;
    this->Modified();
  }
}

//---------------------------------------------------------------------------
vtkIdType vtkMRMLModelDisplayableManager::GetPickedPointID()
{
  vtkDebugMacro(<< "returning this->Internal->PickedPointID of " << this->Internal->PickedPointID);
  return this->Internal->PickedPointID;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::SetPickedPointID(vtkIdType newPointID)
{
  vtkDebugMacro(<< "setting PickedPointID to " << newPointID);
  if (this->Internal->PickedPointID != newPointID)
  {
    this->Internal->PickedPointID = newPointID;
    this->Modified();
  }
}

//---------------------------------------------------------------------------
vtkAlgorithm* vtkMRMLModelDisplayableManager::GetClipper(vtkMRMLDisplayNode* dnode, vtkMRMLModelNode::MeshTypeHint type, vtkImplicitFunction* clipFunction, int clippingMethod)
{
  if (!dnode || !clipFunction)
  {
    return nullptr;
  }

  vtkSmartPointer<vtkAlgorithm> clipper = nullptr;
  if (this->Internal->Clippers.find(dnode->GetID()) != this->Internal->Clippers.end())
  {
    clipper = this->Internal->Clippers[dnode->GetID()];
  }

  if (type == vtkMRMLModelNode::UnstructuredGridMeshType)
  {
    if (clippingMethod == vtkMRMLClipNode::Straight)
    {
      vtkSmartPointer<vtkClipDataSet> clipDataSet = vtkClipDataSet::SafeDownCast(clipper);
      if (!clipDataSet)
      {
        clipDataSet = vtkSmartPointer<vtkClipDataSet>::New();
        clipper = clipDataSet;
      }
      clipDataSet->SetClipFunction(clipFunction);
    }
    else
    {
      vtkSmartPointer<vtkExtractGeometry> extractGeometry = vtkExtractGeometry::SafeDownCast(clipper);
      if (!extractGeometry)
      {
        extractGeometry = vtkSmartPointer<vtkExtractGeometry>::New();
        clipper = extractGeometry;
      }
      extractGeometry->SetImplicitFunction(clipFunction);
      extractGeometry->ExtractInsideOff();
      if (clippingMethod == vtkMRMLClipNode::WholeCellsWithBoundary)
      {
        extractGeometry->ExtractBoundaryCellsOn();
      }
    }
  }
  else
  {
    if (clippingMethod == vtkMRMLClipNode::Straight)
    {
      vtkSmartPointer<vtkClipPolyData> clipPolyData = vtkClipPolyData::SafeDownCast(clipper);
      if (!clipPolyData)
      {
        clipPolyData = vtkSmartPointer<vtkClipPolyData>::New();
        clipper = clipPolyData;
      }
      clipPolyData->SetValue(0.0);
      clipPolyData->SetClipFunction(clipFunction);
    }
    else
    {
      vtkSmartPointer<vtkExtractPolyDataGeometry> extractPolyDataGeometry = vtkExtractPolyDataGeometry::SafeDownCast(clipper);
      if (!extractPolyDataGeometry)
      {
        extractPolyDataGeometry = vtkSmartPointer<vtkExtractPolyDataGeometry>::New();
        clipper = extractPolyDataGeometry;
      }
      extractPolyDataGeometry->SetImplicitFunction(clipFunction);
      extractPolyDataGeometry->ExtractInsideOff();
      if (clippingMethod == vtkMRMLClipNode::WholeCellsWithBoundary)
      {
        extractPolyDataGeometry->ExtractBoundaryCellsOn();
      }
    }
  }

  this->Internal->Clippers[dnode->GetID()] = clipper;
  return clipper;
}

//---------------------------------------------------------------------------
void vtkMRMLModelDisplayableManager::OnInteractorStyleEvent(int eventid)
{
  bool keyPressed = false;
  char* keySym = this->GetInteractor()->GetKeySym();
  if (keySym && strcmp(keySym, "i") == 0)
  {
    keyPressed = true;
  }

  if (eventid == vtkCommand::LeftButtonPressEvent && keyPressed)
  {
    double x = this->GetInteractor()->GetEventPosition()[0];
    double y = this->GetInteractor()->GetEventPosition()[1];

    double windowWidth = this->GetInteractor()->GetRenderWindow()->GetSize()[0];
    double windowHeight = this->GetInteractor()->GetRenderWindow()->GetSize()[1];

    if (x < windowWidth && y < windowHeight)
    {
      // it's a 3D displayable manager and the click could have been on a node
      double yNew = windowHeight - y - 1;
      vtkMRMLDisplayNode* displayNode = nullptr;

      if (this->Pick(x, yNew) //
          && strcmp(this->GetPickedNodeID(), "") != 0)
      {
        // find the node id, the picked node name is probably the display node
        const char* pickedNodeID = this->GetPickedNodeID();

        vtkMRMLNode* mrmlNode = this->GetMRMLScene()->GetNodeByID(pickedNodeID);
        if (mrmlNode)
        {
          displayNode = vtkMRMLDisplayNode::SafeDownCast(mrmlNode);
        }
        else
        {
          vtkDebugMacro("couldn't find a mrml node with ID " << pickedNodeID);
        }
      }

      if (displayNode)
      {
        displayNode->SetColor(1.0, 0, 0);
        this->GetInteractionNode()->SetCurrentInteractionMode(vtkMRMLInteractionNode::ViewTransform);
      }
    }
  }
  if (keyPressed)
  {
    this->GetInteractor()->SetKeySym(nullptr);
  }

  this->PassThroughInteractorStyleEvent(eventid);

  return;
}
