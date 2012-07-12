/*=========================================================================

  Program:   ParaView
  Module:    vtkCubeAxesRepresentation.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkCubeAxesRepresentation.h"

#include "vtkAlgorithmOutput.h"
#include "vtkBoundingBox.h"
#include "vtkCommand.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCompositeDataSet.h"
#include "vtkCubeAxesActor.h"
#include "vtkDataSet.h"
#include "vtkFieldData.h"
#include "vtkFloatArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkOutlineSource.h"
#include "vtkPVRenderView.h"
#include "vtkPolyData.h"
#include "vtkProperty.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkStringArray.h"
#include "vtkTextProperty.h"
#include "vtkTransform.h"
#include "vtkAxisActor.h"

vtkStandardNewMacro(vtkCubeAxesRepresentation);
//----------------------------------------------------------------------------
vtkCubeAxesRepresentation::vtkCubeAxesRepresentation()
{
  this->OutlineSource = vtkOutlineSource::New();
  this->OutlineSource->SetBoxTypeToAxisAligned();

  this->CubeAxesActor = vtkCubeAxesActor::New();
  this->CubeAxesActor->SetPickable(0);

  this->Position[0] = this->Position[1] = this->Position[2] = 0.0;
  this->Orientation[0] = this->Orientation[1] = this->Orientation[2] = 0.0;
  this->Scale[0] = this->Scale[1] = this->Scale[2] = 1.0;
  this->CustomBounds[0] = this->CustomBounds[2] = this->CustomBounds[4] = 0.0;
  this->CustomBounds[1] = this->CustomBounds[3] = this->CustomBounds[5] = 1.0;
  this->CustomBoundsActive[0] = 0;
  this->CustomBoundsActive[1] = 0;
  this->CustomBoundsActive[2] = 0;
  this->CustomRange[0] = this->CustomRange[2] = this->CustomRange[4] = 0.0;
  this->CustomRange[1] = this->CustomRange[3] = this->CustomRange[5] = 1.0;
  this->CustomRangeActive[0] = 0;
  this->CustomRangeActive[1] = 0;
  this->CustomRangeActive[2] = 0;
  this->UseBoundsRangeAsLabel = true;
  this->UseOrientedBounds = false;

  this->UserXTitle = this->UserYTitle = this->UserZTitle = NULL;
  this->UseDefaultXTitle = this->UseDefaultYTitle = this->UseDefaultZTitle = 1;
}

//----------------------------------------------------------------------------
vtkCubeAxesRepresentation::~vtkCubeAxesRepresentation()
{
  this->CubeAxesActor->Delete();
  this->OutlineSource->Delete();
  this->SetUserXTitle(NULL);
  this->SetUserYTitle(NULL);
  this->SetUserZTitle(NULL);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetVisibility(bool val)
{
  this->Superclass::SetVisibility(val);
  this->CubeAxesActor->SetVisibility(val? 1 : 0);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetColor(double r, double g, double b)
{
  this->CubeAxesActor->GetProperty()->SetColor(r, g, b);

  this->CubeAxesActor->GetXAxesLinesProperty()->SetColor(r, g, b);
  this->CubeAxesActor->GetYAxesLinesProperty()->SetColor(r, g, b);
  this->CubeAxesActor->GetZAxesLinesProperty()->SetColor(r, g, b);

  this->CubeAxesActor->GetXAxesGridlinesProperty()->SetColor(r, g, b);
  this->CubeAxesActor->GetYAxesGridlinesProperty()->SetColor(r, g, b);
  this->CubeAxesActor->GetZAxesGridlinesProperty()->SetColor(r, g, b);

  this->CubeAxesActor->GetXAxesInnerGridlinesProperty()->SetColor(r, g, b);
  this->CubeAxesActor->GetYAxesInnerGridlinesProperty()->SetColor(r, g, b);
  this->CubeAxesActor->GetZAxesInnerGridlinesProperty()->SetColor(r, g, b);

  this->CubeAxesActor->GetXAxesGridpolysProperty()->SetColor(r, g, b);
  this->CubeAxesActor->GetYAxesGridpolysProperty()->SetColor(r, g, b);
  this->CubeAxesActor->GetZAxesGridpolysProperty()->SetColor(r, g, b);

  for(int i=0; i < 3; i++)
    {
    this->CubeAxesActor->GetTitleTextProperty(i)->SetColor(r, g, b);
    this->CubeAxesActor->GetLabelTextProperty(i)->SetColor(r, g, b);
    }
}

//----------------------------------------------------------------------------
bool vtkCubeAxesRepresentation::AddToView(vtkView* view)
{
  vtkPVRenderView* pvview = vtkPVRenderView::SafeDownCast(view);
  if (pvview)
    {
    pvview->GetRenderer()->AddActor(this->CubeAxesActor);
    this->CubeAxesActor->SetCamera(pvview->GetActiveCamera());
    this->View = pvview;
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
bool vtkCubeAxesRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* pvview = vtkPVRenderView::SafeDownCast(view);
  if (pvview)
    {
    pvview->GetRenderer()->RemoveActor(this->CubeAxesActor);
    this->CubeAxesActor->SetCamera(NULL);
    this->View = NULL;
    return true;
    }
  this->View = NULL;
  return false;
}

//----------------------------------------------------------------------------
int vtkCubeAxesRepresentation::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
  return 1;
}

//----------------------------------------------------------------------------
int vtkCubeAxesRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type, vtkInformation* inInfo,
  vtkInformation* outInfo)
{
  if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
    {
    return 0;
    }

  if (request_type == vtkPVView::REQUEST_UPDATE())
    {
    // we don't call Update() on the outline source, hence on processes without
    // any input data, the dataset will be uninitialized.
    vtkPVRenderView::SetPiece(inInfo, this, 
      this->OutlineSource->GetOutputDataObject(0));
    }
  else if (request_type == vtkPVView::REQUEST_RENDER())
    {
    vtkAlgorithmOutput* producerPort = vtkPVRenderView::GetPieceProducer(inInfo, this);
    if (producerPort)
      {
      vtkAlgorithm* producer = producerPort->GetProducer();
      vtkDataSet* ds = vtkDataSet::SafeDownCast(producer->GetOutputDataObject(
          producerPort->GetIndex()));
      if (ds)
        {
        ds->GetBounds(this->DataBounds);
        }
      }
    this->UpdateBounds();
    }

  return 1;
}

//----------------------------------------------------------------------------
int vtkCubeAxesRepresentation::RequestData(vtkInformation*,
  vtkInformationVector** inputVector, vtkInformationVector*)
{
  vtkMath::UninitializeBounds(this->DataBounds);
  if (inputVector[0]->GetNumberOfInformationObjects()==1)
    {
    vtkDataObject* input = vtkDataObject::GetData(inputVector[0], 0);

    // Update axes title informations
    vtkFieldData* fieldData = input->GetFieldData();
    vtkStringArray* titleX =
        vtkStringArray::SafeDownCast(fieldData->GetAbstractArray("AxisTitleForX"));
    vtkStringArray* titleY =
        vtkStringArray::SafeDownCast(fieldData->GetAbstractArray("AxisTitleForY"));
    vtkStringArray* titleZ =
        vtkStringArray::SafeDownCast(fieldData->GetAbstractArray("AxisTitleForZ"));
    if(titleX && titleX->GetNumberOfValues() > 0 && this->UseDefaultXTitle == 1)
      {
      this->CubeAxesActor->SetXTitle(titleX->GetValue(0).c_str());
      }
    else if(this->UserXTitle)
      {
      this->CubeAxesActor->SetXTitle(this->UserXTitle);
      }
    if(titleY && titleY->GetNumberOfValues() > 0 && this->UseDefaultYTitle == 1)
      {
      this->CubeAxesActor->SetYTitle(titleY->GetValue(0).c_str());
      }
    else if(this->UserYTitle)
      {
      this->CubeAxesActor->SetYTitle(this->UserYTitle);
      }
    if(titleZ && titleZ->GetNumberOfValues() > 0 && this->UseDefaultZTitle == 1)
      {
      this->CubeAxesActor->SetZTitle(titleZ->GetValue(0).c_str());
      }
    else if(this->UserZTitle)
      {
      this->CubeAxesActor->SetZTitle(this->UserZTitle);
      }

    // Update Axis orientation
    vtkFloatArray* uBase =
        vtkFloatArray::SafeDownCast(fieldData->GetArray("AxisBaseForX"));
    if(uBase && uBase->GetNumberOfTuples() > 0)
      {
      this->CubeAxesActor->SetAxisBaseForX(uBase->GetTuple(0));
      }
    else
      {
      this->CubeAxesActor->SetAxisBaseForX(1,0,0);
      }
    vtkFloatArray* vBase =
        vtkFloatArray::SafeDownCast(fieldData->GetArray("AxisBaseForY"));
    if(vBase && vBase->GetNumberOfTuples() > 0)
      {
      this->CubeAxesActor->SetAxisBaseForY(vBase->GetTuple(0));
      }
    else
      {
      this->CubeAxesActor->SetAxisBaseForY(0,1,0);
      }
    vtkFloatArray* wBase =
        vtkFloatArray::SafeDownCast(fieldData->GetArray("AxisBaseForZ"));
    if(wBase && wBase->GetNumberOfTuples() > 0)
      {
      this->CubeAxesActor->SetAxisBaseForZ(wBase->GetTuple(0));
      }
    else
      {
      this->CubeAxesActor->SetAxisBaseForZ(0,0,1);
      }

    // Make sure we enable oriented bounding box if any
    vtkFloatArray* orientedboundingBox =
        vtkFloatArray::SafeDownCast(fieldData->GetArray("OrientedBoundingBox"));
    if(orientedboundingBox)
      {
      this->UseOrientedBounds = true;
      this->CubeAxesActor->SetUseOrientedBounds(1);
      this->CubeAxesActor->SetOrientedBounds(orientedboundingBox->GetTuple(0));
      }
    else
      {
      this->UseOrientedBounds = false;
      this->CubeAxesActor->SetUseOrientedBounds(0);
      }

    // Make sure we enable the custom origin if any
    vtkFloatArray* customOrigin =
        vtkFloatArray::SafeDownCast(fieldData->GetArray("AxisOrigin"));
    if(customOrigin)
      {
      this->CubeAxesActor->SetUseAxisOrigin(1);
      this->CubeAxesActor->SetAxisOrigin(customOrigin->GetTuple(0));
      }
    else
      {
      this->CubeAxesActor->SetUseAxisOrigin(0);
      }


    vtkDataSet* ds = vtkDataSet::SafeDownCast(input);
    vtkCompositeDataSet* cd = vtkCompositeDataSet::SafeDownCast(input);
    if (ds)
      {
      ds->GetBounds(this->DataBounds);
      }
    else
      {
      vtkCompositeDataIterator* iter = cd->NewIterator();
      vtkBoundingBox bbox;
      for (iter->InitTraversal(); !iter->IsDoneWithTraversal();
        iter->GoToNextItem())
        {
        ds = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject());
        if (ds)
          {
          double bds[6];
          ds->GetBounds(bds);
          if (vtkMath::AreBoundsInitialized(bds))
            {
            bbox.AddBounds(bds);
            }
          }
        }
      iter->Delete();
      bbox.GetBounds(this->DataBounds);
      }
    this->OutlineSource->SetBounds(this->DataBounds);
    this->OutlineSource->Update();
    }


  // We fire UpdateDataEvent to notify the representation proxy that the
  // representation was updated. The representation proxty will then call
  // PostUpdateData(). We do this since now representations are not updated at
  // the proxy level.
  this->InvokeEvent(vtkCommand::UpdateDataEvent);
  return 1;
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::UpdateBounds()
{
  double *scale = this->Scale;
  double *position = this->Position;
  double *rotation = this->Orientation;
  double bds[6];
  if (scale[0] != 1.0 || scale[1] != 1.0 || scale[2] != 1.0 ||
    position[0] != 0.0 || position[1] != 0.0 || position[2] != 0.0 ||
    rotation[0] != 0.0 || rotation[1] != 0.0 || rotation[2] != 0.0)
    {
    const double *bounds = this->DataBounds;
    vtkSmartPointer<vtkTransform> transform =
      vtkSmartPointer<vtkTransform>::New();
    transform->Translate(position);
    transform->RotateZ(rotation[2]);
    transform->RotateX(rotation[0]);
    transform->RotateY(rotation[1]);
    transform->Scale(scale);
    vtkBoundingBox bbox;
    int i, j, k;
    double origX[3], x[3];

    for (i = 0; i < 2; i++)
      {
      origX[0] = bounds[i];
      for (j = 0; j < 2; j++)
        {
        origX[1] = bounds[2 + j];
        for (k = 0; k < 2; k++)
          {
          origX[2] = bounds[4 + k];
          transform->TransformPoint(origX, x);
          bbox.AddPoint(x);
          }
        }
      }
    bbox.GetBounds(bds);
    }
  else
    {
    memcpy(bds, this->DataBounds, sizeof(double)*6);
    }

  //overload bounds with the active custom bounds
  for ( int i=0; i < 3; ++i)
    {
    int pos = i * 2;
    if ( this->CustomBoundsActive[i] )
      {
      bds[pos]=this->CustomBounds[pos];
      bds[pos+1]=this->CustomBounds[pos+1];
      }
    }
  this->CubeAxesActor->SetBounds(bds);

  // Override the range if we have any custom value or set them as the bounds
  for ( int i=0; i < 3; ++i)
    {
    int pos = i * 2;
    if ( this->CustomRangeActive[i] )
      {
      if(i == 0) this->CubeAxesActor->SetXAxisRange(&this->CustomRange[pos]);
      if(i == 1) this->CubeAxesActor->SetYAxisRange(&this->CustomRange[pos]);
      if(i == 2) this->CubeAxesActor->SetZAxisRange(&this->CustomRange[pos]);
      }
    else if (this->UseBoundsRangeAsLabel && !this->UseOrientedBounds)
      {
      if(i == 0) this->CubeAxesActor->SetXAxisRange(&bds[pos]);
      if(i == 1) this->CubeAxesActor->SetYAxisRange(&bds[pos]);
      if(i == 2) this->CubeAxesActor->SetZAxisRange(&bds[pos]);
      }
    }
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//***************************************************************************
// Forwarded to internal vtkCubeAxesActor
void vtkCubeAxesRepresentation::SetFlyMode(int val)
{
  this->CubeAxesActor->SetFlyMode(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetInertia(int val)
{
  this->CubeAxesActor->SetInertia(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetCornerOffset(double val)
{
  this->CubeAxesActor->SetCornerOffset(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetTickLocation(int val)
{
  this->CubeAxesActor->SetTickLocation(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetXTitle(const char* val)
{
  this->SetUserXTitle(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetXAxisVisibility(int val)
{
  this->CubeAxesActor->SetXAxisVisibility(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetXAxisTickVisibility(int val)
{
  this->CubeAxesActor->SetXAxisTickVisibility(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetXAxisMinorTickVisibility(int val)
{
  this->CubeAxesActor->SetXAxisMinorTickVisibility(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetDrawXGridlines(int val)
{
  this->CubeAxesActor->SetDrawXGridlines(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetYTitle(const char* val)
{
  this->SetUserYTitle(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetYAxisVisibility(int val)
{
  this->CubeAxesActor->SetYAxisVisibility(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetYAxisTickVisibility(int val)
{
  this->CubeAxesActor->SetYAxisTickVisibility(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetYAxisMinorTickVisibility(int val)
{
  this->CubeAxesActor->SetYAxisMinorTickVisibility(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetDrawYGridlines(int val)
{
  this->CubeAxesActor->SetDrawYGridlines(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetZTitle(const char* val)
{
  this->SetUserZTitle(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetZAxisVisibility(int val)
{
  this->CubeAxesActor->SetZAxisVisibility(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetZAxisTickVisibility(int val)
{
  this->CubeAxesActor->SetZAxisTickVisibility(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetZAxisMinorTickVisibility(int val)
{
  this->CubeAxesActor->SetZAxisMinorTickVisibility(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetDrawZGridlines(int val)
{
  this->CubeAxesActor->SetDrawZGridlines(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetGridLineLocation(int val)
{
  this->CubeAxesActor->SetGridLineLocation(val);
}

//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetXAxisRange(double min, double max)
{
  if(!UseBoundsRangeAsLabel)
    {
    this->CubeAxesActor->SetXAxisRange(min, max);
    }
}
//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetYAxisRange(double min, double max)
{
  if(!UseBoundsRangeAsLabel)
    {
    this->CubeAxesActor->SetYAxisRange(min, max);
    }
}
//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::SetZAxisRange(double min, double max)
{
  if(!UseBoundsRangeAsLabel)
    {
    this->CubeAxesActor->SetZAxisRange(min, max);
    }
}
//----------------------------------------------------------------------------
void vtkCubeAxesRepresentation::EnableCustomAxisRange(bool useCustomRange)
{
  this->UseBoundsRangeAsLabel = !useCustomRange;
  if(UseBoundsRangeAsLabel)
    {
    double bounds[6];
    this->CubeAxesActor->GetBounds(bounds);
    this->CubeAxesActor->SetXAxisRange(&bounds[0]);
    this->CubeAxesActor->SetYAxisRange(&bounds[2]);
    this->CubeAxesActor->SetZAxisRange(&bounds[4]);
    }
}
