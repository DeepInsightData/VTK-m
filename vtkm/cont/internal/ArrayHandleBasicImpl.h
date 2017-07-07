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

#include <vtkm/cont/ArrayHandle.h>

#ifndef vtk_m_cont_internal_ArrayHandleBasicImpl_h
#define vtk_m_cont_internal_ArrayHandleBasicImpl_h

#include <vtkm/cont/StorageBasic.h>

#include <type_traits>

namespace vtkm
{
namespace cont
{

namespace internal
{

/// Type-agnostic container for an execution memory buffer.
struct VTKM_ALWAYS_EXPORT TypelessExecutionArray
{
  VTKM_CONT
  TypelessExecutionArray(void*& array, void*& arrayEnd, void*& arrayCapacity)
    : Array(array)
    , ArrayEnd(arrayEnd)
    , ArrayCapacity(arrayCapacity)
  {
  }

  void*& Array;
  void*& ArrayEnd;
  void*& ArrayCapacity;
};

/// Factory that generates execution portals for basic storage.
template <typename T, typename DeviceTag>
struct ExecutionPortalFactoryBasic
#ifndef VTKM_DOXYGEN_ONLY
  ;
#else  // VTKM_DOXYGEN_ONLY
  /// The portal type.
  using PortalType = SomePortalType;

/// The cont portal type.
using ConstPortalType = SomePortalType;

/// Create a portal to access the execution data from @a start to @a end.
VTKM_CONT
static PortalType CreatePortal(ValueType* start, ValueType* end);

/// Create a const portal to access the execution data from @a start to @a end.
VTKM_CONT
static PortalConstType CreatePortalConst(const ValueType* start, const ValueType* end);
#endif // VTKM_DOXYGEN_ONLY

/// Typeless interface for interacting with a execution memory buffer when using basic storage.
struct VTKM_CONT_EXPORT ExecutionArrayInterfaceBasicBase
{
  VTKM_CONT ExecutionArrayInterfaceBasicBase(StorageBasicBase& storage);
  VTKM_CONT virtual ~ExecutionArrayInterfaceBasicBase();

  VTKM_CONT
  virtual DeviceAdapterId GetDeviceId() const = 0;

  /// If @a execArray's base pointer is null, allocate a new buffer.
  /// If (capacity - base) < @a numBytes, the buffer will be freed and
  /// reallocated. If (capacity - base) >= numBytes, a new end is marked.
  VTKM_CONT
  virtual void Allocate(TypelessExecutionArray& execArray, vtkm::Id numBytes) const = 0;

  /// Release the buffer held by @a execArray and reset all pointer to null.
  VTKM_CONT
  virtual void Free(TypelessExecutionArray& execArray) const = 0;

  /// Copy @a numBytes from @a controlPtr to @a executionPtr.
  VTKM_CONT
  virtual void CopyFromControl(const void* controlPtr,
                               void* executionPtr,
                               vtkm::Id numBytes) const = 0;

  /// Copy @a numBytes from @a executionPtr to @a controlPtr.
  VTKM_CONT
  virtual void CopyToControl(const void* executionPtr,
                             void* controlPtr,
                             vtkm::Id numBytes) const = 0;

protected:
  StorageBasicBase& ControlStorage;
};

/**
 * Specializations should inherit from and implement the API of
 * ExecutionArrayInterfaceBasicBase.
 */
template <typename DeviceTag>
struct ExecutionArrayInterfaceBasic;

} // end namespace internal

/// Specialization of ArrayHandle for Basic storage. The goal here is to reduce
/// the amount of codegen for the common case of Basic storage when we build
/// the common arrays into libvtkm_cont.
template <typename T>
class ArrayHandle<T, ::vtkm::cont::StorageTagBasic> : public ::vtkm::cont::internal::ArrayHandleBase
{
private:
  using Thisclass = ArrayHandle<T, ::vtkm::cont::StorageTagBasic>;

  template <typename DeviceTag>
  using PortalFactory = vtkm::cont::internal::ExecutionPortalFactoryBasic<T, DeviceTag>;

public:
  using StorageTag = ::vtkm::cont::StorageTagBasic;
  using StorageType = vtkm::cont::internal::Storage<T, StorageTag>;
  using ValueType = T;
  using PortalControl = typename StorageType::PortalType;
  using PortalConstControl = typename StorageType::PortalConstType;
  struct InternalStruct;

  template <typename DeviceTag>
  struct ExecutionTypes
  {
    using Portal = typename PortalFactory<DeviceTag>::PortalType;
    using PortalConst = typename PortalFactory<DeviceTag>::PortalConstType;
  };

  VTKM_CONT ArrayHandle();
  VTKM_CONT ArrayHandle(const Thisclass& src);
  VTKM_CONT ArrayHandle(const Thisclass&& src);
  VTKM_CONT ArrayHandle(const StorageType& storage);
  VTKM_CONT ArrayHandle(const std::shared_ptr<InternalStruct>& i);

  VTKM_CONT ~ArrayHandle();

  VTKM_CONT Thisclass& operator=(const Thisclass& src);
  VTKM_CONT Thisclass& operator=(Thisclass&& src);
  VTKM_CONT bool operator==(const Thisclass& rhs) const;
  VTKM_CONT bool operator!=(const Thisclass& rhs) const;
  VTKM_CONT StorageType& GetStorage();
  VTKM_CONT const StorageType& GetStorage() const;
  VTKM_CONT PortalControl GetPortalControl();
  VTKM_CONT PortalConstControl GetPortalConstControl() const;
  VTKM_CONT vtkm::Id GetNumberOfValues() const;

  template <typename IteratorType, typename DeviceAdapterTag>
  VTKM_CONT void CopyInto(IteratorType dest, DeviceAdapterTag) const;

  VTKM_CONT void Allocate(vtkm::Id numberOfValues);
  VTKM_CONT void Shrink(vtkm::Id numberOfValues);
  VTKM_CONT void ReleaseResourcesExecution();
  VTKM_CONT void ReleaseResources();

  template <typename DeviceAdapterTag>
  VTKM_CONT typename ExecutionTypes<DeviceAdapterTag>::PortalConst PrepareForInput(
    DeviceAdapterTag device) const;

  template <typename DeviceAdapterTag>
  VTKM_CONT typename ExecutionTypes<DeviceAdapterTag>::Portal PrepareForOutput(
    vtkm::Id numVals,
    DeviceAdapterTag device);

  template <typename DeviceAdapterTag>
  VTKM_CONT typename ExecutionTypes<DeviceAdapterTag>::Portal PrepareForInPlace(
    DeviceAdapterTag device);

  template <typename DeviceAdapterTag>
  VTKM_CONT void PrepareForDevice(DeviceAdapterTag) const;

  VTKM_CONT void SyncControlArray() const;
  VTKM_CONT void ReleaseResourcesExecutionInternal();

  struct VTKM_ALWAYS_EXPORT InternalStruct
  {
    InternalStruct()
      : ControlArrayValid(false)
      , ExecutionInterface(nullptr)
      , ExecutionArrayValid(false)
      , ExecutionArray(nullptr)
      , ExecutionArrayEnd(nullptr)
      , ExecutionArrayCapacity(nullptr)
    {
    }

    InternalStruct(const StorageType& storage)
      : ControlArrayValid(true)
      , ControlArray(storage)
      , ExecutionInterface(nullptr)
      , ExecutionArrayValid(false)
      , ExecutionArray(nullptr)
      , ExecutionArrayEnd(nullptr)
      , ExecutionArrayCapacity(nullptr)
    {
    }

    bool ControlArrayValid;
    StorageType ControlArray;

    internal::ExecutionArrayInterfaceBasicBase* ExecutionInterface;
    bool ExecutionArrayValid;
    ValueType* ExecutionArray;
    ValueType* ExecutionArrayEnd;
    ValueType* ExecutionArrayCapacity;
  };

  std::shared_ptr<InternalStruct> Internals;
};

} // end namespace cont
} // end namespace vtkm

#include <vtkm/cont/internal/ArrayHandleBasicImpl.hxx>

#endif // vtk_m_cont_internal_ArrayHandleBasicImpl_h
