// STL includes
#include <sstream>

// VTK includes
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkXMLDataParser.h>

// MRML includes
#include "vtkMRMLLayoutNode.h"
#include "vtkMRMLAbstractViewNode.h"

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLLayoutNode);

//----------------------------------------------------------------------------
vtkMRMLLayoutNode::vtkMRMLLayoutNode()
{
  this->GUIPanelVisibility = 1;
  this->BottomPanelVisibility = 1;
  this->GUIPanelLR = 0;
  this->ViewArrangement = vtkMRMLLayoutNode::SlicerLayoutNone;
  this->CollapseSliceControllers = 0;
  this->NumberOfCompareViewRows = 1;
  this->NumberOfCompareViewColumns = 1;
  this->NumberOfCompareViewLightboxRows = 6;
  this->NumberOfCompareViewLightboxColumns = 6;
  this->MainPanelSize = 400;
  this->SecondaryPanelSize = 400;
  this->SelectedModule = nullptr;

  this->CurrentLayoutDescription = nullptr;
  this->LayoutRootElement = nullptr;

  // Synchronize the view description with the layout
  this->AddLayoutDescription(vtkMRMLLayoutNode::SlicerLayoutNone, "");
}

//----------------------------------------------------------------------------
vtkMRMLLayoutNode::~vtkMRMLLayoutNode()
{
  if (this->SelectedModule)
  {
    delete[] this->SelectedModule;
    this->SelectedModule = nullptr;
  }
  if (this->LayoutRootElement)
  {
    this->LayoutRootElement->Delete();
    this->LayoutRootElement = nullptr;
  }
  this->SetCurrentLayoutDescription(nullptr);
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::WriteXML(ostream& of, int nIndent)
{
  // Write all attributes, since the parsing of the string is dependent on the
  // order here.

  Superclass::WriteXML(of, nIndent);

  of << " currentViewArrangement=\"" << this->ViewArrangement << "\"";
  of << " guiPanelVisibility=\"" << this->GUIPanelVisibility << "\"";
  of << " bottomPanelVisibility =\"" << this->BottomPanelVisibility << "\"";
  of << " guiPanelLR=\"" << this->GUIPanelLR << "\"";
  of << " collapseSliceControllers=\"" << this->CollapseSliceControllers << "\"" << std::endl;
  of << " numberOfCompareViewRows=\"" << this->NumberOfCompareViewRows << "\"";
  of << " numberOfCompareViewColumns=\"" << this->NumberOfCompareViewColumns << "\"";
  of << " numberOfLightboxRows=\"" << this->NumberOfCompareViewLightboxRows << "\"";
  of << " numberOfLightboxColumns=\"" << this->NumberOfCompareViewLightboxColumns << "\"";
  of << " mainPanelSize=\"" << this->MainPanelSize << "\"";
  of << " secondaryPanelSize=\"" << this->SecondaryPanelSize << "\"";
  if (this->SelectedModule != nullptr)
  {
    of << " selectedModule=\"" << (this->SelectedModule != nullptr ? this->SelectedModule : "") << "\"";
  }
  // of << " layout=\"" << this->CurrentLayoutDescription << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;

  while (*atts != nullptr)
  {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, "currentViewArrangement"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->ViewArrangement;
      if (this->ViewArrangement < vtkMRMLLayoutNode::SlicerLayoutInitialView)
      {
        this->ViewArrangement = vtkMRMLLayoutNode::SlicerLayoutInitialView;
      }
    }
    else if (!strcmp(attName, "guiPanelVisibility"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->GUIPanelVisibility;
    }
    else if (!strcmp(attName, "bottomPanelVisibility"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->BottomPanelVisibility;
    }
    else if (!strcmp(attName, "guiPanelLR"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->GUIPanelLR;
    }
    else if (!strcmp(attName, "collapseSliceControllers"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->CollapseSliceControllers;
    }
    else if (!strcmp(attName, "numberOfCompareViewRows"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->NumberOfCompareViewRows;
    }
    else if (!strcmp(attName, "numberOfCompareViewColumns"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->NumberOfCompareViewColumns;
    }
    else if (!strcmp(attName, "numberOfLightboxRows"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->NumberOfCompareViewLightboxRows;
    }
    else if (!strcmp(attName, "numberOfLightboxColumns"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->NumberOfCompareViewLightboxColumns;
    }
    else if (!strcmp(attName, "mainPanelSize"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->MainPanelSize;
    }
    else if (!strcmp(attName, "secondaryPanelSize"))
    {
      std::stringstream ss;
      ss << attValue;
      ss >> this->SecondaryPanelSize;
    }
    else if (!strcmp(attName, "selectedModule"))
    {
      this->SetSelectedModule(attValue);
    }
    else if (!strcmp(attName, "layout"))
    {
      // this->SetAndParseCurrentLayoutDescription(attValue);
    }
  }

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::Reset(vtkMRMLNode* defaultNode)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::Reset(defaultNode);
  this->RemoveAllMaximizedViewNodes();
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::SetViewArrangement(int arrNew)
{
  // if the view arrangement definition has not been changed, return
  if (this->ViewArrangement == arrNew        //
      && this->GetCurrentLayoutDescription() //
      && this->GetCurrentLayoutDescription() == this->GetLayoutDescription(arrNew))
  {
    return;
  }
  this->ViewArrangement = arrNew;
  int wasModifying = this->StartModify();
  this->UpdateCurrentLayoutDescription();
  this->Modified();
  this->EndModify(wasModifying);
}

//----------------------------------------------------------------------------
bool vtkMRMLLayoutNode::AddLayoutDescription(int layout, const char* layoutDescription)
{
  if (this->IsLayoutDescription(layout))
  {
    vtkDebugMacro(<< "Layout " << layout << " has already been registered");
    return false;
  }
  this->Layouts[layout] = std::string(layoutDescription);
  this->Modified();
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLLayoutNode::SetLayoutDescription(int layout, const char* layoutDescription)
{
  if (!this->IsLayoutDescription(layout))
  {
    vtkDebugMacro(<< "Layout " << layout << " has NOT been registered");
    return false;
  }
  std::string layoutDescriptionStr;
  if (layoutDescription)
  {
    layoutDescriptionStr = std::string(layoutDescription);
  }
  if (this->Layouts[layout] == layoutDescriptionStr)
  {
    // No change
    return true;
  }
  if (layoutDescriptionStr.empty())
  {
    this->Layouts.erase(layout);
  }
  else
  {
    this->Layouts[layout] = layoutDescriptionStr;
  }
  int wasModifying = this->StartModify();
  this->UpdateCurrentLayoutDescription();
  this->Modified();
  this->EndModify(wasModifying);
  return true;
}

//----------------------------------------------------------------------------
std::vector<int> vtkMRMLLayoutNode::GetLayoutIndices()
{
  std::vector<int> indices;
  for (const auto& keyValuePair : this->Layouts)
  {
    indices.push_back(keyValuePair.first);
  }
  return indices;
}

//----------------------------------------------------------------------------
bool vtkMRMLLayoutNode::IsLayoutDescription(int layout)
{
  std::map<int, std::string>::const_iterator it = this->Layouts.find(layout);
  return it != this->Layouts.end();
}

//----------------------------------------------------------------------------
std::string vtkMRMLLayoutNode::GetLayoutDescription(int layout)
{
  std::map<int, std::string>::const_iterator it = this->Layouts.find(layout);
  if (it == this->Layouts.end())
  {
    vtkWarningMacro("Can't find layout:" << layout);
    return std::string();
  }
  return it->second;
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::UpdateCurrentLayoutDescription()
{
  if (this->GetViewArrangement() == vtkMRMLLayoutNode::SlicerLayoutCustomView)
  {
    return;
  }
  int viewArrangement = this->ViewArrangement;
  std::string description = this->GetLayoutDescription(viewArrangement);
  if (this->GetCurrentLayoutDescription() && //
      description == this->GetCurrentLayoutDescription())
  {
    return;
  }
  this->SetAndParseCurrentLayoutDescription(description.c_str());
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::SetAndParseCurrentLayoutDescription(const char* description)
{
  // Be careful that it matches the ViewArrangement value
  if (this->LayoutRootElement)
  {
    this->LayoutRootElement->Delete();
  }
  this->LayoutRootElement = this->ParseLayout(description);
  if (this->LayoutRootElement == nullptr)
  {
    // ParseLayout has already logged an error, if there was any
    this->SetCurrentLayoutDescription("");
    return;
  }

  this->SetCurrentLayoutDescription(description);
}

//----------------------------------------------------------------------------
vtkXMLDataElement* vtkMRMLLayoutNode::ParseLayout(const char* description)
{
  if (!description || strlen(description) <= 0)
  {
    return nullptr;
  }

  std::istringstream iss(description, std::istringstream::in);
  vtkNew<vtkXMLDataParser> parser;
  parser->SetStream(&iss);
  parser->Parse();

  vtkXMLDataElement* root = parser->GetRootElement();
  if (root == nullptr)
  {
    vtkErrorWithObjectMacro(parser, "vtkMRMLLayoutNode::ParseLayout: failed to parse layout description");
    return nullptr;
  }

  // if we don't register, then the root element will be destroyed when the
  // parser gets out of scope
  root->Register(nullptr);
  return root;
}

//----------------------------------------------------------------------------
// Copy the node's attributes to this object.
// Does NOT copy: ID, FilePrefix, LabelText, ID
void vtkMRMLLayoutNode::CopyContent(vtkMRMLNode* anode, bool deepCopy /*=true*/)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::CopyContent(anode, deepCopy);

  vtkMRMLLayoutNode* node = (vtkMRMLLayoutNode*)anode;
  // Try to copy the registered layout descriptions. However, if the node
  // currently has layout descriptions (more than the default None description)
  // then we don't want to copy them (it would overwrite the descriptions)
  if (node->Layouts.size() > 1 && this->Layouts.size() == 1)
  {
    this->Layouts = node->Layouts;
    this->Modified();
  }

  vtkMRMLCopyBeginMacro(anode);
  vtkMRMLCopyIntMacro(ViewArrangement);
  vtkMRMLCopyIntMacro(GUIPanelVisibility);
  vtkMRMLCopyIntMacro(BottomPanelVisibility);
  vtkMRMLCopyIntMacro(GUIPanelLR);
  vtkMRMLCopyIntMacro(CollapseSliceControllers);
  vtkMRMLCopyIntMacro(NumberOfCompareViewRows);
  vtkMRMLCopyIntMacro(NumberOfCompareViewColumns);
  vtkMRMLCopyIntMacro(NumberOfCompareViewLightboxRows);
  vtkMRMLCopyIntMacro(NumberOfCompareViewLightboxColumns);
  vtkMRMLCopyIntMacro(MainPanelSize);
  vtkMRMLCopyIntMacro(SecondaryPanelSize);
  vtkMRMLCopyStringMacro(SelectedModule);
  vtkMRMLCopyEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::CopyLayoutDescriptions(vtkMRMLLayoutNode* source)
{
  if (!source)
  {
    vtkErrorMacro("CopyLayoutDescriptions: Invalid source node");
    return;
  }
  this->Layouts = source->Layouts;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);

  // Layout:
  os << indent << "ViewArrangement: " << this->ViewArrangement << "\n";
  os << indent << "GUIPanelVisibility: " << this->GUIPanelVisibility << "\n";
  os << indent << "GUIPanelLR: " << this->GUIPanelLR << "\n";
  os << indent << "BottomPanelVisibility: " << this->BottomPanelVisibility << "\n";
  os << indent << "CollapseSliceControllers: " << this->CollapseSliceControllers << "\n";
  os << indent << "NumberOfCompareViewRows: " << this->NumberOfCompareViewRows << "\n";
  os << indent << "NumberOfCompareViewColumns: " << this->NumberOfCompareViewColumns << "\n";
  os << indent << "NumberOfCompareViewLightboxRows: " << this->NumberOfCompareViewLightboxRows << "\n";
  os << indent << "NumberOfCompareViewLightboxColumns: " << this->NumberOfCompareViewLightboxColumns << "\n";
  os << indent << "Main panel size: " << this->MainPanelSize << "\n";
  os << indent << "Secondary panel size: " << this->SecondaryPanelSize << "\n";
  if (this->SelectedModule)
  {
    os << indent << "Selected module: " << this->SelectedModule << "\n";
  }
  else
  {
    os << indent << "Selected module: (none)\n";
  }
}

//----------------------------------------------------------------------------
vtkMRMLAbstractViewNode* vtkMRMLLayoutNode::GetMaximizedViewNode(int maximizedViewNodeIndex)
{
  return vtkMRMLAbstractViewNode::SafeDownCast(this->GetNthNodeReference("MaximizedView", maximizedViewNodeIndex));
}

//----------------------------------------------------------------------------
int vtkMRMLLayoutNode::GetNumberOfMaximizedViewNodes()
{
  return this->GetNumberOfNodeReferences("MaximizedView");
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::AddMaximizedViewNode(vtkMRMLAbstractViewNode* maximizedViewNode)
{
  if (!maximizedViewNode || !maximizedViewNode->GetID())
  {
    // nothing to add
    return;
  }
  if (this->HasNodeReferenceID("MaximizedView", maximizedViewNode->GetID()))
  {
    // already added
    return;
  }
  this->AddNodeReferenceID("MaximizedView", maximizedViewNode->GetID());
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::RemoveMaximizedViewNode(vtkMRMLAbstractViewNode* maximizedViewNode)
{
  for (int nodeReferenceIndex = 0; nodeReferenceIndex < this->GetNumberOfMaximizedViewNodes(); ++nodeReferenceIndex)
  {
    vtkMRMLAbstractViewNode* viewNode = this->GetMaximizedViewNode(nodeReferenceIndex);
    if (viewNode == maximizedViewNode)
    {
      this->RemoveNthNodeReferenceID("MaximizedView", nodeReferenceIndex);
      return;
    }
  }
}

//----------------------------------------------------------------------------
bool vtkMRMLLayoutNode::IsMaximizedViewNode(vtkMRMLAbstractViewNode* viewNode)
{
  if (!viewNode || !viewNode->GetID())
  {
    return false;
  }
  return this->HasNodeReferenceID("MaximizedView", viewNode->GetID());
}

//----------------------------------------------------------------------------
void vtkMRMLLayoutNode::RemoveAllMaximizedViewNodes()
{
  this->RemoveNodeReferenceIDs("MaximizedView");
}
