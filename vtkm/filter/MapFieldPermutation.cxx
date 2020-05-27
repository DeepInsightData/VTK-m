//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#include <vtkm/filter/MapFieldPermutation.h>

#include <vtkm/TypeList.h>
#include <vtkm/TypeTraits.h>
#include <vtkm/VecTraits.h>

#include <vtkm/cont/Logging.h>

#include <vtkm/worklet/WorkletMapField.h>

#include <vtkm/filter/PolicyDefault.h>

namespace
{

template <typename T>
struct MapPermutationWorklet : vtkm::worklet::WorkletMapField
{
  T InvalidValue;

  explicit MapPermutationWorklet(T invalidValue)
    : InvalidValue(invalidValue)
  {
  }

  using ControlSignature = void(FieldIn permutationIndex, WholeArrayIn input, FieldOut output);

  template <typename InputPortalType>
  VTKM_EXEC void operator()(vtkm::Id permutationIndex, InputPortalType inputPortal, T& output) const
  {
    if ((permutationIndex >= 0) && (permutationIndex < inputPortal.GetNumberOfValues()))
    {
      output = inputPortal.Get(permutationIndex);
    }
    else
    {
      output = this->InvalidValue;
    }
  }
};

// For simplicity, the invalid value is specified as a single type (vtkm::Float64), and this is
// often a non-finite value, which is not well represented by integer types. This function does its
// best to find a reasonable cast for the value.
template <typename T>
T CastInvalidValue(vtkm::Float64 invalidValue)
{
  using ComponentType = typename vtkm::VecTraits<T>::BaseComponentType;

  if (std::is_same<vtkm::TypeTraitsIntegerTag, typename vtkm::TypeTraits<T>::NumericTag>::value)
  {
    // Casting to integer types
    if (vtkm::IsFinite(invalidValue))
    {
      return T(static_cast<ComponentType>(invalidValue));
    }
    else if (vtkm::IsInf(invalidValue) && (invalidValue > 0))
    {
      return T(std::numeric_limits<ComponentType>::max());
    }
    else
    {
      return T(std::numeric_limits<ComponentType>::min());
    }
  }
  else
  {
    // Not an integer type. Assume can be directly cast
    return T(static_cast<ComponentType>(invalidValue));
  }
}

struct DoMapFieldPermutation
{
  bool CalledMap = false;

  template <typename T, typename S>
  void operator()(const vtkm::cont::ArrayHandle<T, S>& inputArray,
                  const vtkm::cont::ArrayHandle<vtkm::Id>& permutation,
                  vtkm::cont::VariantArrayHandle& output,
                  vtkm::Float64 invalidValue)
  {
    vtkm::cont::ArrayHandle<T> outputArray;
    MapPermutationWorklet<T> worklet(CastInvalidValue<T>(invalidValue));
    vtkm::cont::Invoker invoke;
    invoke(worklet, permutation, inputArray, outputArray);
    output = vtkm::cont::VariantArrayHandle(outputArray);
    this->CalledMap = true;
  }
};

} // anonymous namespace

bool vtkm::filter::MapFieldPermutation(const vtkm::cont::Field& inputField,
                                       const vtkm::cont::ArrayHandle<vtkm::Id>& permutation,
                                       vtkm::cont::Field& outputField,
                                       vtkm::Float64 invalidValue)
{
  vtkm::cont::VariantArrayHandle outputArray;
  DoMapFieldPermutation functor;
  inputField.GetData().ResetTypes<vtkm::TypeListAll>().CastAndCall(
    vtkm::filter::PolicyDefault::StorageList{}, functor, permutation, outputArray, invalidValue);
  if (functor.CalledMap)
  {
    outputField = vtkm::cont::Field(inputField.GetName(), inputField.GetAssociation(), outputArray);
  }
  else
  {
    VTKM_LOG_S(vtkm::cont::LogLevel::Warn, "Faild to map field " << inputField.GetName());
  }
  return functor.CalledMap;
}

bool vtkm::filter::MapFieldPermutation(const vtkm::cont::Field& inputField,
                                       const vtkm::cont::ArrayHandle<vtkm::Id>& permutation,
                                       vtkm::cont::DataSet& outputData,
                                       vtkm::Float64 invalidValue)
{
  vtkm::cont::Field outputField;
  bool success =
    vtkm::filter::MapFieldPermutation(inputField, permutation, outputField, invalidValue);
  if (success)
  {
    outputData.AddField(outputField);
  }
  return success;
}
