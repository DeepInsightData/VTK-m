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
#ifndef vtk_m_cont_VariantArrayHandleContainer_h
#define vtk_m_cont_VariantArrayHandleContainer_h

#include <vtkm/cont/vtkm_cont_export.h>

#include <vtkm/cont/ArrayHandleVirtual.h>
#include <vtkm/cont/ArrayHandleVirtual.hxx>


#include <memory>
#include <vtkm/Types.h>


namespace vtkm
{
namespace cont
{

// Forward declaration needed for GetContainer
template <typename TypeList>
class VariantArrayHandleBase;

namespace internal
{

/// \brief Base class for VariantArrayHandleContainer
///
struct VTKM_CONT_EXPORT VariantArrayHandleContainerBase
{
  VariantArrayHandleContainerBase();

  // This must exist so that subclasses are destroyed correctly.
  virtual ~VariantArrayHandleContainerBase();

  virtual vtkm::Id GetNumberOfValues() const = 0;
  virtual vtkm::IdComponent GetNumberOfComponents() const = 0;

  virtual void ReleaseResourcesExecution() = 0;
  virtual void ReleaseResources() = 0;

  virtual void PrintSummary(std::ostream& out) const = 0;

  virtual std::shared_ptr<VariantArrayHandleContainerBase> NewInstance() const = 0;

  virtual const vtkm::cont::StorageVirtual* GetStorage() const = 0;
};

/// \brief ArrayHandle container that can use C++ run-time type information.
///
/// The \c VariantArrayHandleContainer is similar to the
/// \c SimplePolymorphicContainer in that it can contain an object of an
/// unknown type. However, this class specifically holds ArrayHandle objects
/// (with different template parameters) so that it can polymorphically answer
/// simple questions about the object.
///
template <typename T>
struct VTKM_ALWAYS_EXPORT VariantArrayHandleContainer final : public VariantArrayHandleContainerBase
{
  vtkm::cont::ArrayHandleVirtual<T> Array;

  VariantArrayHandleContainer()
    : Array()
  {
  }

  VariantArrayHandleContainer(const vtkm::cont::ArrayHandleVirtual<T>& array)
    : Array(array)
  {
  }

  ~VariantArrayHandleContainer<T>() = default;

  vtkm::Id GetNumberOfValues() const { return this->Array.GetNumberOfValues(); }

  vtkm::IdComponent GetNumberOfComponents() const { return vtkm::VecTraits<T>::NUM_COMPONENTS; }


  void ReleaseResourcesExecution() { this->Array.ReleaseResourcesExecution(); }
  void ReleaseResources() { this->Array.ReleaseResources(); }

  void PrintSummary(std::ostream& out) const
  {
    vtkm::cont::printSummary_ArrayHandle(this->Array, out);
  }

  std::shared_ptr<VariantArrayHandleContainerBase> NewInstance() const
  {
    return std::make_shared<VariantArrayHandleContainer<T>>(this->Array.NewInstance());
  }

  const vtkm::cont::StorageVirtual* GetStorage() const { return this->Array.GetStorage(); }
};

namespace variant
{

// One instance of a template class cannot access the private members of
// another instance of a template class. However, I want to be able to copy
// construct a VariantArrayHandle from another VariantArrayHandle of any other
// type. Since you cannot partially specialize friendship, use this accessor
// class to get at the internals for the copy constructor.
struct GetContainer
{
  template <typename TypeList>
  VTKM_CONT static const std::shared_ptr<VariantArrayHandleContainerBase>& Extract(
    const vtkm::cont::VariantArrayHandleBase<TypeList>& src)
  {
    return src.ArrayContainer;
  }
};

template <typename ArrayHandleType>
VTKM_CONT bool IsType(const VariantArrayHandleContainerBase* container)
{ //container could be nullptr
  VTKM_IS_ARRAY_HANDLE(ArrayHandleType);
  if (!container)
  {
    return false;
  }

  using VT = typename ArrayHandleType::ValueType;
  using ST = typename ArrayHandleType::StorageTag;
  const vtkm::cont::StorageVirtual* storage = container->GetStorage();
  return storage->IsType<vtkm::cont::StorageAny<VT, ST>>();
}

template <typename T>
VTKM_CONT bool IsValueType(const VariantArrayHandleContainerBase* container)
{
  if (container == nullptr)
  { //you can't use typeid on nullptr of polymorphic types
    return false;
  }

  //needs optimizations based on platform. !OSX can use typeid
  return (nullptr != dynamic_cast<const VariantArrayHandleContainer<T>*>(container));
}


template <typename T, typename S>
struct VTKM_ALWAYS_EXPORT Caster
{
  vtkm::cont::ArrayHandle<T, S> operator()(const VariantArrayHandleContainerBase* container) const
  {
    using ArrayHandleType = vtkm::cont::ArrayHandle<T, S>;
    if (!IsType<ArrayHandleType>(container))
    {
      VTKM_LOG_CAST_FAIL(container, ArrayHandleType);
      throwFailedDynamicCast(vtkm::cont::TypeName(container),
                             vtkm::cont::TypeName<ArrayHandleType>());
    }

    //we know the storage isn't a virtual but another storage type
    //that means that the container holds a vtkm::cont::StorageAny<T>
    const auto* any = static_cast<const vtkm::cont::StorageAny<T, S>*>(container->GetStorage());
    VTKM_LOG_CAST_SUCC(container, *any);
    return any->GetHandle();
  }
};

template <typename T>
struct VTKM_ALWAYS_EXPORT Caster<T, vtkm::cont::StorageTagVirtual>
{
  vtkm::cont::ArrayHandle<T, vtkm::cont::StorageTagVirtual> operator()(
    const VariantArrayHandleContainerBase* container) const
  {
    if (!IsValueType<T>(container))
    {
      VTKM_LOG_CAST_FAIL(container, vtkm::cont::ArrayHandleVirtual<T>);
      throwFailedDynamicCast(vtkm::cont::TypeName(container),
                             vtkm::cont::TypeName<vtkm::cont::ArrayHandleVirtual<T>>());
    }

    // Technically, this method returns a copy of the \c ArrayHandle. But
    // because \c ArrayHandle acts like a shared pointer, it is valid to
    // do the copy.
    const auto* derived = static_cast<const VariantArrayHandleContainer<T>*>(container);
    VTKM_LOG_CAST_SUCC(container, derived->Array);
    return derived->Array;
  }
};


template <typename ArrayHandleType>
VTKM_CONT ArrayHandleType Cast(const VariantArrayHandleContainerBase* container)
{ //container could be nullptr
  VTKM_IS_ARRAY_HANDLE(ArrayHandleType);
  using Type = typename ArrayHandleType::ValueType;
  using Storage = typename ArrayHandleType::StorageTag;
  auto ret = Caster<Type, Storage>{}(container);
  return ArrayHandleType(std::move(ret));
}
}
}
}
} //namespace vtkm::cont::internal::variant

#endif