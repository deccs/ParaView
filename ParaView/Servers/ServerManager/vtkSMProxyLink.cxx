/*=========================================================================

  Program:   ParaView
  Module:    vtkSMProxyLink.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkSMProxyLink.h"

#include "vtkCommand.h"
#include "vtkObjectFactory.h"
#include "vtkPVXMLElement.h"
#include "vtkSmartPointer.h"
#include "vtkSMProperty.h"
#include "vtkSMProxy.h"
#include "vtkSMStateLoader.h"

#include <vtkstd/list>

vtkStandardNewMacro(vtkSMProxyLink);
vtkCxxRevisionMacro(vtkSMProxyLink, "1.4");

//---------------------------------------------------------------------------
struct vtkSMProxyLinkInternals
{
  struct LinkedProxy
  {
    LinkedProxy(vtkSMProxy* proxy, int updateDir) : 
      Proxy(proxy), UpdateDirection(updateDir), Observer(0)
      {
      };
    ~LinkedProxy()
      {
        if (this->Observer && this->Proxy.GetPointer())
          {
          this->Proxy.GetPointer()->RemoveObserver(Observer);
          this->Observer = 0;
          }
      }
    vtkSmartPointer<vtkSMProxy> Proxy;
    int UpdateDirection;
    vtkCommand* Observer;
  };

  typedef vtkstd::list<LinkedProxy> LinkedProxiesType;
  LinkedProxiesType LinkedProxies;
};

//---------------------------------------------------------------------------
vtkSMProxyLink::vtkSMProxyLink()
{
  this->Internals = new vtkSMProxyLinkInternals;
}

//---------------------------------------------------------------------------
vtkSMProxyLink::~vtkSMProxyLink()
{
  delete this->Internals;
}

//---------------------------------------------------------------------------
void vtkSMProxyLink::AddLinkedProxy(vtkSMProxy* proxy, int updateDir)
{
  int addToList = 1;
  int addObserver = updateDir & INPUT;

  vtkSMProxyLinkInternals::LinkedProxiesType::iterator iter =
    this->Internals->LinkedProxies.begin();
  for(; iter != this->Internals->LinkedProxies.end(); iter++)
    {
    if (iter->Proxy == proxy)
      {
      if (iter->UpdateDirection != updateDir)
        {
        iter->UpdateDirection = updateDir;
        if (addObserver)
          {
          iter->Observer = this->Observer;
          }
        }
      else
        {
        addObserver = 0;
        }
      addToList = 0;
      }
    }

  if (addToList)
    {
    vtkSMProxyLinkInternals::LinkedProxy link(proxy, updateDir);
    this->Internals->LinkedProxies.push_back(link);
    if (addObserver)
      {
      this->Internals->LinkedProxies.back().Observer = this->Observer;
      }
    }

  if (addObserver)
    {
    this->ObserveProxyUpdates(proxy);
    }
}

//---------------------------------------------------------------------------
void vtkSMProxyLink::UpdateProperties(vtkSMProxy* fromProxy, const char* pname)
{
  if (!fromProxy)
    {
    return;
    }
  vtkSMProperty* fromProp = fromProxy->GetProperty(pname);
  if (!fromProp)
    {
    return;
    }

  vtkSMProxyLinkInternals::LinkedProxiesType::iterator iter =
    this->Internals->LinkedProxies.begin();
  for(; iter != this->Internals->LinkedProxies.end(); iter++)
    {
    if ((iter->Proxy.GetPointer() != fromProxy) && 
        (iter->UpdateDirection & OUTPUT))
      {
      vtkSMProperty* toProp = iter->Proxy->GetProperty(pname);
      if (toProp)
        {
        toProp->Copy(fromProp);
        }
      }
    }
}

//---------------------------------------------------------------------------
void vtkSMProxyLink::UpdateVTKObjects(vtkSMProxy* caller)
{
  vtkSMProxyLinkInternals::LinkedProxiesType::iterator iter =
    this->Internals->LinkedProxies.begin();
  for(; iter != this->Internals->LinkedProxies.end(); iter++)
    {
    if ((iter->Proxy.GetPointer() != caller) && 
        (iter->UpdateDirection & OUTPUT))
      {
      iter->Proxy->UpdateVTKObjects();
      }
    }
}

//---------------------------------------------------------------------------
void vtkSMProxyLink::SaveState(const char* linkname, vtkPVXMLElement* parent)
{
  vtkPVXMLElement* root = vtkPVXMLElement::New();
  root->SetName("ProxyLink");
  root->AddAttribute("name", linkname);
  vtkSMProxyLinkInternals::LinkedProxiesType::iterator iter =
    this->Internals->LinkedProxies.begin();
  for(; iter != this->Internals->LinkedProxies.end(); ++iter)
    {
    vtkPVXMLElement* child = vtkPVXMLElement::New();
    child->SetName("Proxy");
    child->AddAttribute("direction", (iter->UpdateDirection == INPUT? "input" : "output"));
    child->AddAttribute("id", iter->Proxy.GetPointer()->GetSelfIDAsString());
    root->AddNestedElement(child);
    child->Delete();
    }
  parent->AddNestedElement(root);
  root->Delete();
}

//---------------------------------------------------------------------------
int vtkSMProxyLink::LoadState(vtkPVXMLElement* linkElement, vtkSMStateLoader* loader)
{
  unsigned int numElems = linkElement->GetNumberOfNestedElements();
  for (unsigned int cc=0; cc < numElems; cc++)
    {
    vtkPVXMLElement* child = linkElement->GetNestedElement(cc);
    if (!child->GetName() || strcmp(child->GetName(), "Proxy") != 0)
      {
      vtkWarningMacro("Invalid element in link state. Ignoring.");
      continue;
      }
    const char* direction = child->GetAttribute("direction");
    if (!direction)
      {
      vtkErrorMacro("State missing required attribute direction.");
      return 0;
      }
    int idirection;
    if (strcmp(direction, "input") == 0)
      {
      idirection = INPUT;
      }
    else if (strcmp(direction, "output") == 0)
      {
      idirection = OUTPUT;
      }
    else
      {
      vtkErrorMacro("Invalid value for direction: " << direction);
      return 0;
      }
    int id;
    if (!child->GetScalarAttribute("id", &id))
      {
      vtkErrorMacro("State missing required attribute id.");
      return 0;
      }
    vtkSMProxy* proxy = loader->NewProxy(id); 
    if (!proxy)
      {
      vtkErrorMacro("Failed to locate proxy with ID: " << id);
      return 0;
      }
    this->AddLinkedProxy(proxy, idirection);
    proxy->Delete();
    }
  return 1;
}

//---------------------------------------------------------------------------
void vtkSMProxyLink::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
