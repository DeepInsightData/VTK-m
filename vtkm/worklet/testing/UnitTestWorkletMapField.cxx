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
#include <vtkm/cont/ArrayHandle.h>
#include <vtkm/cont/DynamicArrayHandle.h>

#include <vtkm/worklet/DispatcherMapField.h>
#include <vtkm/worklet/WorkletMapField.h>

#include <vtkm/cont/testing/Testing.h>

class TestMapFieldWorklet : public vtkm::worklet::WorkletMapField
{
public:
  typedef void ControlSignature(FieldIn<>, FieldOut<>, FieldInOut<>);
  typedef void ExecutionSignature(_1, _2, _3, WorkIndex);

  template <typename T>
  VTKM_EXEC void operator()(const T& in, T& out, T& inout, vtkm::Id workIndex) const
  {
    if (!test_equal(in, TestValue(workIndex, T()) + T(100)))
    {
      this->RaiseError("Got wrong input value.");
    }
    out = in - T(100);
    if (!test_equal(inout, TestValue(workIndex, T()) + T(100)))
    {
      this->RaiseError("Got wrong in-out value.");
    }
    inout = inout - T(100);
  }

  template <typename T1, typename T2, typename T3>
  VTKM_EXEC void operator()(const T1&, const T2&, const T3&, vtkm::Id) const
  {
    this->RaiseError("Cannot call this worklet with different types.");
  }
};

class TestMapFieldWorkletLimitedTypes : public vtkm::worklet::WorkletMapField
{
public:
  typedef void ControlSignature(FieldIn<ScalarAll>, FieldOut<ScalarAll>, FieldInOut<ScalarAll>);
  typedef _2 ExecutionSignature(_1, _3, WorkIndex);

  template <typename T1, typename T3>
  VTKM_EXEC T1 operator()(const T1& in, T3& inout, vtkm::Id workIndex) const
  {
    if (!test_equal(in, TestValue(workIndex, T1()) + T1(100)))
    {
      this->RaiseError("Got wrong input value.");
    }

    if (!test_equal(inout, TestValue(workIndex, T3()) + T3(100)))
    {
      this->RaiseError("Got wrong in-out value.");
    }
    inout = inout - T3(100);

    return in - T1(100);
  }
};

namespace mapfield
{
static const vtkm::Id ARRAY_SIZE = 10;

template <typename WorkletType>
struct DoStaticTestWorklet
{
  template <typename T>
  VTKM_CONT void operator()(T) const
  {
    std::cout << "Set up data." << std::endl;
    T inputArray[ARRAY_SIZE];

    for (vtkm::Id index = 0; index < ARRAY_SIZE; index++)
    {
      inputArray[index] = TestValue(index, T()) + T(100);
    }

    vtkm::cont::ArrayHandle<T> inputHandle = vtkm::cont::make_ArrayHandle(inputArray, ARRAY_SIZE);
    vtkm::cont::ArrayHandle<T> outputHandle;
    vtkm::cont::ArrayHandle<T> inoutHandle;

    vtkm::cont::DeviceAdapterAlgorithm<VTKM_DEFAULT_DEVICE_ADAPTER_TAG>::Copy(inputHandle,
                                                                              inoutHandle);

    std::cout << "Create and run dispatcher." << std::endl;
    vtkm::worklet::DispatcherMapField<WorkletType> dispatcher;
    dispatcher.Invoke(inputHandle, outputHandle, inoutHandle);

    std::cout << "Check result." << std::endl;
    CheckPortal(outputHandle.GetPortalConstControl());
    CheckPortal(inoutHandle.GetPortalConstControl());

    std::cout << "Try to invoke with an input array of the wrong size." << std::endl;
    inputHandle.Shrink(ARRAY_SIZE / 2);
    bool exceptionThrown = false;
    try
    {
      dispatcher.Invoke(inputHandle, outputHandle, inoutHandle);
    }
    catch (vtkm::cont::ErrorBadValue& error)
    {
      std::cout << "  Caught expected error: " << error.GetMessage() << std::endl;
      exceptionThrown = true;
    }
    VTKM_TEST_ASSERT(exceptionThrown, "Dispatcher did not throw expected exception.");
  }
};

template <typename WorkletType>
struct DoDynamicTestWorklet
{
  template <typename T>
  VTKM_CONT void operator()(T) const
  {
    std::cout << "Set up data." << std::endl;
    T inputArray[ARRAY_SIZE];

    for (vtkm::Id index = 0; index < ARRAY_SIZE; index++)
    {
      inputArray[index] = TestValue(index, T()) + T(100);
    }

    vtkm::cont::ArrayHandle<T> inputHandle = vtkm::cont::make_ArrayHandle(inputArray, ARRAY_SIZE);
    vtkm::cont::ArrayHandle<T> outputHandle;
    vtkm::cont::ArrayHandle<T> inoutHandle;

    vtkm::cont::DeviceAdapterAlgorithm<VTKM_DEFAULT_DEVICE_ADAPTER_TAG>::Copy(inputHandle,
                                                                              inoutHandle);

    std::cout << "Create and run dispatcher with dynamic arrays." << std::endl;
    vtkm::worklet::DispatcherMapField<WorkletType> dispatcher;

    vtkm::cont::DynamicArrayHandle inputDynamic(inputHandle);
    vtkm::cont::DynamicArrayHandle outputDynamic(outputHandle);
    vtkm::cont::DynamicArrayHandle inoutDynamic(inoutHandle);

    dispatcher.Invoke(inputDynamic, outputDynamic, inoutDynamic);

    CheckPortal(outputHandle.GetPortalConstControl());
    CheckPortal(inoutHandle.GetPortalConstControl());
  }
};

template <typename WorkletType>
struct DoTestWorklet
{
  template <typename T>
  VTKM_CONT void operator()(T t) const
  {
    DoStaticTestWorklet<WorkletType> sw;
    sw(t);
    DoDynamicTestWorklet<WorkletType> dw;
    dw(t);
  }
};

void TestWorkletMapField()
{
  typedef vtkm::cont::DeviceAdapterTraits<VTKM_DEFAULT_DEVICE_ADAPTER_TAG> DeviceAdapterTraits;
  std::cout << "Testing Map Field on device adapter: " << DeviceAdapterTraits::GetName()
            << std::endl;

  std::cout << "--- Worklet accepting all types." << std::endl;
  vtkm::testing::Testing::TryTypes(mapfield::DoTestWorklet<TestMapFieldWorklet>(),
                                   vtkm::TypeListTagCommon());

  std::cout << "--- Worklet accepting some types." << std::endl;
  vtkm::testing::Testing::TryTypes(mapfield::DoTestWorklet<TestMapFieldWorkletLimitedTypes>(),
                                   vtkm::TypeListTagFieldScalar());

  std::cout << "--- Sending bad type to worklet." << std::endl;
  try
  {
    //can only test with dynamic arrays, as static arrays will fail to compile
    DoDynamicTestWorklet<TestMapFieldWorkletLimitedTypes> badWorkletTest;
    badWorkletTest(vtkm::Vec<vtkm::Float32, 3>());
    VTKM_TEST_FAIL("Did not throw expected error.");
  }
  catch (vtkm::cont::ErrorBadType& error)
  {
    std::cout << "Got expected error: " << error.GetMessage() << std::endl;
  }
}

} // mapfield namespace

int UnitTestWorkletMapField(int, char* [])
{
  return vtkm::cont::testing::Testing::Run(mapfield::TestWorkletMapField);
}
