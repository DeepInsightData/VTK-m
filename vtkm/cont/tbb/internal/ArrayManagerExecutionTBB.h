//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2014 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
//  Copyright 2014 UT-Battelle, LLC.
//  Copyright 2014 Los Alamos National Security.
//
//  Under the terms of Contract DE-NA0003525 with NTESS,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================
#ifndef vtk_m_cont_tbb_internal_ArrayManagerExecutionTBB_h
#define vtk_m_cont_tbb_internal_ArrayManagerExecutionTBB_h

#include <vtkm/cont/tbb/internal/DeviceAdapterTagTBB.h>

#include <vtkm/cont/internal/ArrayExportMacros.h>
#include <vtkm/cont/internal/ArrayManagerExecution.h>
#include <vtkm/cont/internal/ArrayManagerExecutionShareWithControl.h>

// These must be placed in the vtkm::cont::internal namespace so that
// the template can be found.

namespace vtkm
{
namespace cont
{
namespace internal
{

template <typename T, class StorageTag>
class ArrayManagerExecution<T, StorageTag, vtkm::cont::DeviceAdapterTagTBB>
  : public vtkm::cont::internal::ArrayManagerExecutionShareWithControl<T, StorageTag>
{
public:
  using Superclass = vtkm::cont::internal::ArrayManagerExecutionShareWithControl<T, StorageTag>;
  using ValueType = typename Superclass::ValueType;
  using PortalType = typename Superclass::PortalType;
  using PortalConstType = typename Superclass::PortalConstType;
  using StorageType = typename Superclass::StorageType;

  VTKM_CONT
  ArrayManagerExecution(StorageType* storage)
    : Superclass(storage)
  {
  }

  VTKM_CONT
  PortalConstType PrepareForInput(bool updateData)
  {
    return this->Superclass::PrepareForInput(updateData);
  }

  VTKM_CONT
  PortalType PrepareForInPlace(bool updateData)
  {
    return this->Superclass::PrepareForInPlace(updateData);
  }

  VTKM_CONT
  PortalType PrepareForOutput(vtkm::Id numberOfValues)
  {
    return this->Superclass::PrepareForOutput(numberOfValues);
  }
};


template <typename T>
struct ExecutionPortalFactoryBasic<T, DeviceAdapterTagTBB>
  : public ExecutionPortalFactoryBasicShareWithControl<T>
{
  using Superclass = ExecutionPortalFactoryBasicShareWithControl<T>;

  using typename Superclass::ValueType;
  using typename Superclass::PortalType;
  using typename Superclass::PortalConstType;
  using Superclass::CreatePortal;
  using Superclass::CreatePortalConst;
};

template <>
struct VTKM_CONT_EXPORT ExecutionArrayInterfaceBasic<DeviceAdapterTagTBB>
  : public ExecutionArrayInterfaceBasicShareWithControl
{
  using Superclass = ExecutionArrayInterfaceBasicShareWithControl;

  VTKM_CONT
  ExecutionArrayInterfaceBasic(StorageBasicBase& storage);

  VTKM_CONT
  virtual DeviceAdapterId GetDeviceId() const final { return VTKM_DEVICE_ADAPTER_TBB; }
};

} // namespace internal
#ifndef vtk_m_cont_tbb_internal_ArrayManagerExecutionTBB_cxx
VTKM_EXPORT_ARRAYHANDLES_FOR_DEVICE_ADAPTER(DeviceAdapterTagTBB)
#endif // !vtk_m_cont_tbb_internal_ArrayManagerExecutionTBB_cxx
}
} // namespace vtkm::cont

#endif //vtk_m_cont_tbb_internal_ArrayManagerExecutionTBB_h
