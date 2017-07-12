//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2017 Sandia Corporation.
//  Copyright 2017 UT-Battelle, LLC.
//  Copyright 2017 Los Alamos National Security.
//
//  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================

#ifndef vtk_m_cont_internal_ArrayHandleBasicImpl_hxx
#define vtk_m_cont_internal_ArrayHandleBasicImpl_hxx

#include <vtkm/cont/internal/ArrayHandleBasicImpl.h>

namespace vtkm
{
namespace cont
{

template <typename T>
ArrayHandle<T, StorageTagBasic>::ArrayHandle()
  : Internals(new InternalStruct)
{
}

template <typename T>
ArrayHandle<T, StorageTagBasic>::ArrayHandle(const Thisclass& src)
  : Internals(src.Internals)
{
}

template <typename T>
ArrayHandle<T, StorageTagBasic>::ArrayHandle(const Thisclass&& src)
  : Internals(std::move(src.Internals))
{
}

template <typename T>
ArrayHandle<T, StorageTagBasic>::ArrayHandle(const StorageType& storage)
  : Internals(new InternalStruct(storage))
{
}

template <typename T>
ArrayHandle<T, StorageTagBasic>::ArrayHandle(const std::shared_ptr<InternalStruct>& i)
  : Internals(i)
{
}

template <typename T>
ArrayHandle<T, StorageTagBasic>::~ArrayHandle()
{
}

template <typename T>
ArrayHandle<T, StorageTagBasic>& ArrayHandle<T, StorageTagBasic>::operator=(const Thisclass& src)
{
  this->Internals = src.Internals;
  return *this;
}

template <typename T>
ArrayHandle<T, StorageTagBasic>& ArrayHandle<T, StorageTagBasic>::operator=(Thisclass&& src)
{
  this->Internals = std::move(src.Internals);
  return *this;
}

template <typename T>
bool ArrayHandle<T, StorageTagBasic>::operator==(const Thisclass& rhs) const
{
  return this->Internals == rhs.Internals;
}

template <typename T>
bool ArrayHandle<T, StorageTagBasic>::operator!=(const Thisclass& rhs) const
{
  return this->Internals != rhs.Internals;
}

template <typename T>
typename ArrayHandle<T, StorageTagBasic>::StorageType& ArrayHandle<T, StorageTagBasic>::GetStorage()
{
  this->SyncControlArray();
  if (this->Internals->ControlArrayValid)
  {
    return this->Internals->ControlArray;
  }
  else
  {
    throw vtkm::cont::ErrorInternal(
      "ArrayHandle::SyncControlArray did not make control array valid.");
  }
}

template <typename T>
const typename ArrayHandle<T, StorageTagBasic>::StorageType&
ArrayHandle<T, StorageTagBasic>::GetStorage() const
{
  this->SyncControlArray();
  if (this->Internals->ControlArrayValid)
  {
    return this->Internals->ControlArray;
  }
  else
  {
    throw vtkm::cont::ErrorInternal(
      "ArrayHandle::SyncControlArray did not make control array valid.");
  }
}

template <typename T>
typename ArrayHandle<T, StorageTagBasic>::PortalControl
ArrayHandle<T, StorageTagBasic>::GetPortalControl()
{
  this->SyncControlArray();
  if (this->Internals->ControlArrayValid)
  {
    // If the user writes into the iterator we return, then the execution
    // array will become invalid. Play it safe and release the execution
    // resources. (Use the const version to preserve the execution array.)
    this->ReleaseResourcesExecutionInternal();
    return this->Internals->ControlArray.GetPortal();
  }
  else
  {
    throw vtkm::cont::ErrorInternal(
      "ArrayHandle::SyncControlArray did not make control array valid.");
  }
}


template <typename T>
typename ArrayHandle<T, StorageTagBasic>::PortalConstControl
ArrayHandle<T, StorageTagBasic>::GetPortalConstControl() const
{
  this->SyncControlArray();
  if (this->Internals->ControlArrayValid)
  {
    return this->Internals->ControlArray.GetPortalConst();
  }
  else
  {
    throw vtkm::cont::ErrorInternal(
      "ArrayHandle::SyncControlArray did not make control array valid.");
  }
}

template <typename T>
vtkm::Id ArrayHandle<T, StorageTagBasic>::GetNumberOfValues() const
{
  if (this->Internals->ControlArrayValid)
  {
    return this->Internals->ControlArray.GetNumberOfValues();
  }
  else if (this->Internals->ExecutionArrayValid)
  {
    return static_cast<vtkm::Id>(this->Internals->ExecutionArrayEnd -
                                 this->Internals->ExecutionArray);
  }
  else
  {
    return 0;
  }
}

template <typename T>
template <typename IteratorType, typename DeviceAdapterTag>
void ArrayHandle<T, StorageTagBasic>::CopyInto(IteratorType dest, DeviceAdapterTag) const
{
  using pointer_type = typename std::iterator_traits<IteratorType>::pointer;
  using value_type = typename std::remove_pointer<pointer_type>::type;

  static_assert(!std::is_const<value_type>::value, "CopyInto requires a non const iterator.");

  VTKM_IS_DEVICE_ADAPTER_TAG(DeviceAdapterTag);

  if (!this->Internals->ControlArrayValid && !this->Internals->ExecutionArrayValid)
  {
    throw vtkm::cont::ErrorBadValue("ArrayHandle has no data to copy into Iterator.");
  }

  // If we can copy directly from the execution environment, do so.
  DeviceAdapterId devId = DeviceAdapterTraits<DeviceAdapterTag>::GetId();
  if (!this->Internals->ControlArrayValid && std::is_pointer<IteratorType>::value &&
      this->Internals->ExecutionInterface &&
      this->Internals->ExecutionInterface->GetDeviceId() == devId)
  {
    vtkm::Id numBytes = static_cast<vtkm::Id>(sizeof(ValueType)) * this->GetNumberOfValues();
    this->Internals->ExecutionInterface->CopyToControl(
      this->Internals->ExecutionArray, dest, numBytes);
  }
  else
  {
    // Otherwise copy from control.
    PortalConstControl portal = this->GetPortalConstControl();
    std::copy(vtkm::cont::ArrayPortalToIteratorBegin(portal),
              vtkm::cont::ArrayPortalToIteratorEnd(portal),
              dest);
  }
}

template <typename T>
void ArrayHandle<T, StorageTagBasic>::Allocate(vtkm::Id numberOfValues)
{
  this->ReleaseResourcesExecutionInternal();
  this->Internals->ControlArray.Allocate(numberOfValues);
  this->Internals->ControlArrayValid = true;
}

template <typename T>
void ArrayHandle<T, StorageTagBasic>::Shrink(vtkm::Id numberOfValues)
{
  VTKM_ASSERT(numberOfValues >= 0);

  if (numberOfValues > 0)
  {
    vtkm::Id originalNumberOfValues = this->GetNumberOfValues();

    if (numberOfValues < originalNumberOfValues)
    {
      if (this->Internals->ControlArrayValid)
      {
        this->Internals->ControlArray.Shrink(numberOfValues);
      }
      if (this->Internals->ExecutionArrayValid)
      {
        this->Internals->ExecutionArrayEnd = this->Internals->ExecutionArray + numberOfValues;
      }
    }
    else if (numberOfValues == originalNumberOfValues)
    {
      // Nothing to do.
    }
    else // numberOfValues > originalNumberOfValues
    {
      throw vtkm::cont::ErrorBadValue("ArrayHandle::Shrink cannot be used to grow array.");
    }

    VTKM_ASSERT(this->GetNumberOfValues() == numberOfValues);
  }
  else // numberOfValues == 0
  {
    // If we are shrinking to 0, there is nothing to save and we might as well
    // free up memory. Plus, some storage classes expect that data will be
    // deallocated when the size goes to zero.
    this->Allocate(0);
  }
}

template <typename T>
void ArrayHandle<T, StorageTagBasic>::ReleaseResourcesExecution()
{
  // Save any data in the execution environment by making sure it is synced
  // with the control environment.
  this->SyncControlArray();
  this->ReleaseResourcesExecutionInternal();
}

template <typename T>
void ArrayHandle<T, StorageTagBasic>::ReleaseResources()
{
  this->ReleaseResourcesExecutionInternal();

  if (this->Internals->ControlArrayValid)
  {
    this->Internals->ControlArray.ReleaseResources();
    this->Internals->ControlArrayValid = false;
  }
}

template <typename T>
template <typename DeviceAdapterTag>
typename ArrayHandle<T, StorageTagBasic>::template ExecutionTypes<DeviceAdapterTag>::PortalConst
ArrayHandle<T, StorageTagBasic>::PrepareForInput(DeviceAdapterTag device) const
{
  VTKM_IS_DEVICE_ADAPTER_TAG(DeviceAdapterTag);
  InternalStruct* priv = const_cast<InternalStruct*>(this->Internals.get());

  this->PrepareForDevice(device);

  if (!this->Internals->ExecutionArrayValid)
  {
    // Initialize an empty array if needed:
    if (!this->Internals->ControlArrayValid)
    {
      this->Internals->ControlArray.Allocate(0);
      this->Internals->ControlArrayValid = true;
    }

    internal::TypelessExecutionArray execArray(
      reinterpret_cast<void*&>(priv->ExecutionArray),
      reinterpret_cast<void*&>(priv->ExecutionArrayEnd),
      reinterpret_cast<void*&>(priv->ExecutionArrayCapacity));

    const vtkm::Id numBytes =
      static_cast<vtkm::Id>(sizeof(ValueType)) * this->GetStorage().GetNumberOfValues();

    priv->ExecutionInterface->Allocate(execArray, numBytes);

    priv->ExecutionInterface->CopyFromControl(
      priv->ControlArray.GetArray(), priv->ExecutionArray, numBytes);

    this->Internals->ExecutionArrayValid = true;
  }

  return PortalFactory<DeviceAdapterTag>::CreatePortalConst(this->Internals->ExecutionArray,
                                                            this->Internals->ExecutionArrayEnd);
}

template <typename T>
template <typename DeviceAdapterTag>
typename ArrayHandle<T, StorageTagBasic>::template ExecutionTypes<DeviceAdapterTag>::Portal
ArrayHandle<T, StorageTagBasic>::PrepareForOutput(vtkm::Id numVals, DeviceAdapterTag device)
{
  VTKM_IS_DEVICE_ADAPTER_TAG(DeviceAdapterTag);
  InternalStruct* priv = const_cast<InternalStruct*>(this->Internals.get());

  this->PrepareForDevice(device);

  // Invalidate control arrays since we expect the execution data to be
  // overwritten. Don't free control resources in case they're shared with
  // the execution environment.
  this->Internals->ControlArrayValid = false;

  internal::TypelessExecutionArray execArray(
    reinterpret_cast<void*&>(priv->ExecutionArray),
    reinterpret_cast<void*&>(priv->ExecutionArrayEnd),
    reinterpret_cast<void*&>(priv->ExecutionArrayCapacity));

  this->Internals->ExecutionInterface->Allocate(execArray,
                                                static_cast<vtkm::Id>(sizeof(ValueType)) * numVals);

  this->Internals->ExecutionArrayValid = true;

  return PortalFactory<DeviceAdapterTag>::CreatePortal(this->Internals->ExecutionArray,
                                                       this->Internals->ExecutionArrayEnd);
}

template <typename T>
template <typename DeviceAdapterTag>
typename ArrayHandle<T, StorageTagBasic>::template ExecutionTypes<DeviceAdapterTag>::Portal
ArrayHandle<T, StorageTagBasic>::PrepareForInPlace(DeviceAdapterTag device)
{
  VTKM_IS_DEVICE_ADAPTER_TAG(DeviceAdapterTag);
  InternalStruct* priv = const_cast<InternalStruct*>(this->Internals.get());

  this->PrepareForDevice(device);

  if (!this->Internals->ExecutionArrayValid)
  {
    // Initialize an empty array if needed:
    if (!this->Internals->ControlArrayValid)
    {
      this->Internals->ControlArray.Allocate(0);
      this->Internals->ControlArrayValid = true;
    }

    internal::TypelessExecutionArray execArray(
      reinterpret_cast<void*&>(this->Internals->ExecutionArray),
      reinterpret_cast<void*&>(this->Internals->ExecutionArrayEnd),
      reinterpret_cast<void*&>(this->Internals->ExecutionArrayCapacity));

    vtkm::Id numBytes =
      static_cast<vtkm::Id>(sizeof(ValueType)) * this->GetStorage().GetNumberOfValues();

    priv->ExecutionInterface->Allocate(execArray, numBytes);

    priv->ExecutionInterface->CopyFromControl(
      priv->ControlArray.GetArray(), priv->ExecutionArray, numBytes);

    this->Internals->ExecutionArrayValid = true;
  }

  // Invalidate the control array, since we expect the values to be modified:
  this->Internals->ControlArrayValid = false;

  return PortalFactory<DeviceAdapterTag>::CreatePortal(this->Internals->ExecutionArray,
                                                       this->Internals->ExecutionArrayEnd);
}

template <typename T>
template <typename DeviceAdapterTag>
void ArrayHandle<T, StorageTagBasic>::PrepareForDevice(DeviceAdapterTag) const
{
  DeviceAdapterId devId = DeviceAdapterTraits<DeviceAdapterTag>::GetId();
  InternalStruct* priv = const_cast<InternalStruct*>(this->Internals.get());

  // Check if the current device matches the last one and sync through
  // the control environment if the device changes.
  if (this->Internals->ExecutionInterface)
  {
    if (this->Internals->ExecutionInterface->GetDeviceId() == devId)
    {
      // All set, nothing to do.
      return;
    }
    else
    {
      // Update the device allocator:
      this->SyncControlArray();
      internal::TypelessExecutionArray execArray(
        reinterpret_cast<void*&>(priv->ExecutionArray),
        reinterpret_cast<void*&>(priv->ExecutionArrayEnd),
        reinterpret_cast<void*&>(priv->ExecutionArrayCapacity));
      priv->ExecutionInterface->Free(execArray);
      delete priv->ExecutionInterface;
      priv->ExecutionInterface = nullptr;
      priv->ExecutionArrayValid = false;
    }
  }

  VTKM_ASSERT(priv->ExecutionInterface == nullptr);
  VTKM_ASSERT(!priv->ExecutionArrayValid);

  priv->ExecutionInterface =
    new internal::ExecutionArrayInterfaceBasic<DeviceAdapterTag>(this->Internals->ControlArray);
}

template <typename T>
void ArrayHandle<T, StorageTagBasic>::SyncControlArray() const
{
  if (!this->Internals->ControlArrayValid)
  {
    // Need to change some state that does not change the logical state from
    // an external point of view.
    InternalStruct* priv = const_cast<InternalStruct*>(this->Internals.get());
    if (this->Internals->ExecutionArrayValid)
    {
      const vtkm::Id numValues =
        static_cast<vtkm::Id>(this->Internals->ExecutionArrayEnd - this->Internals->ExecutionArray);
      const vtkm::Id numBytes = static_cast<vtkm::Id>(sizeof(ValueType)) * numValues;
      priv->ControlArray.Allocate(numValues);
      priv->ExecutionInterface->CopyToControl(
        priv->ExecutionArray, priv->ControlArray.GetArray(), numBytes);
      priv->ControlArrayValid = true;
    }
    else
    {
      // This array is in the null state (there is nothing allocated), but
      // the calling function wants to do something with the array. Put this
      // class into a valid state by allocating an array of size 0.
      priv->ControlArray.Allocate(0);
      priv->ControlArrayValid = true;
    }
  }
}

template <typename T>
void ArrayHandle<T, StorageTagBasic>::ReleaseResourcesExecutionInternal()
{
  if (this->Internals->ExecutionArrayValid)
  {
    internal::TypelessExecutionArray execArray(
      reinterpret_cast<void*&>(this->Internals->ExecutionArray),
      reinterpret_cast<void*&>(this->Internals->ExecutionArrayEnd),
      reinterpret_cast<void*&>(this->Internals->ExecutionArrayCapacity));
    this->Internals->ExecutionInterface->Free(execArray);
    this->Internals->ExecutionArrayValid = false;
  }
}
}
} // end namespace vtkm::cont


#endif // not vtk_m_cont_internal_ArrayHandleBasicImpl_hxx