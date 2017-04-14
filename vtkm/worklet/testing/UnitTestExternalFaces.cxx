//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2014 Sandia Corporation.
//  Copyright 2014 UT-Battelle, LLC.
//  Copyright 2014 Los Alamos National Security.
//
//  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================

#include <vtkm/cont/DeviceAdapter.h>
#include <vtkm/cont/DataSetBuilderExplicit.h>

#include <vtkm/cont/internal/DeviceAdapterError.h>

#include <vtkm/io/writer/VTKDataSetWriter.h>
#include <vtkm/cont/testing/MakeTestDataSet.h>
#include <vtkm/cont/testing/Testing.h>

#include <vtkm/worklet/ExternalFaces.h>

#include <algorithm>
#include <iostream>

namespace {

// For this test, we want using the default device adapter to be an error
// to make sure that all the code is using the device adapter we specify.
using MyDeviceAdapter = VTKM_DEFAULT_DEVICE_ADAPTER_TAG;
#undef VTKM_DEFAULT_DEVICE_ADAPTER_TAG
#define VTKM_DEFAULT_DEVICE_ADAPTER_TAG ::vtkm::cont::DeviceAdapterTagError

vtkm::cont::DataSet RunExternalFaces(vtkm::cont::DataSet &inDataSet)
{

  vtkm::cont::CellSetExplicit<> inCellSet;
  inDataSet.GetCellSet(0).CopyTo(inCellSet);

  vtkm::cont::CellSetExplicit<> outCellSet("cells");

  //Run the External Faces worklet
  vtkm::worklet::ExternalFaces().Run(inCellSet,
                                     outCellSet,
                                     MyDeviceAdapter());

  vtkm::cont::DataSet outDataSet;
  for(vtkm::IdComponent i=0; i < inDataSet.GetNumberOfCoordinateSystems(); ++i)
  {
    outDataSet.AddCoordinateSystem(inDataSet.GetCoordinateSystem(i));
  }

  outDataSet.AddCellSet(outCellSet);

  return outDataSet;
}

void TestExternalFaces1()
{
  std::cout << "Test 1" << std::endl;

  //--------------Construct a VTK-m Test Dataset----------------
  const int nVerts = 8; //A cube that is tetrahedralized
  typedef vtkm::Vec<vtkm::Float32,3> CoordType;
  vtkm::cont::ArrayHandle< CoordType > coordinates;
  coordinates.Allocate(nVerts);
  coordinates.GetPortalControl().Set(0, CoordType(0.0f, 0.0f, 0.0f) );
  coordinates.GetPortalControl().Set(1, CoordType(1.0f, 0.0f, 0.0f) );
  coordinates.GetPortalControl().Set(2, CoordType(1.0f, 1.0f, 0.0f) );
  coordinates.GetPortalControl().Set(3, CoordType(0.0f, 1.0f, 0.0f) );
  coordinates.GetPortalControl().Set(4, CoordType(0.0f, 0.0f, 1.0f) );
  coordinates.GetPortalControl().Set(5, CoordType(1.0f, 0.0f, 1.0f) );
  coordinates.GetPortalControl().Set(6, CoordType(1.0f, 1.0f, 1.0f) );
  coordinates.GetPortalControl().Set(7, CoordType(0.0f, 1.0f, 1.0f) );

  //Construct the VTK-m shapes and numIndices connectivity arrays
  const int nCells = 6;  //The tetrahedrons of the cube
  vtkm::IdComponent cellVerts[nCells][4] = {
                                            {4,7,6,3}, {4,6,3,2}, {4,0,3,2},
                                            {4,6,5,2}, {4,5,0,2}, {1,0,5,2}
                                           };

  vtkm::cont::ArrayHandle<vtkm::UInt8>       shapes;
  vtkm::cont::ArrayHandle<vtkm::IdComponent> numIndices;
  vtkm::cont::ArrayHandle<vtkm::Id>          conn;
  shapes.Allocate(static_cast<vtkm::Id>(nCells));
  numIndices.Allocate(static_cast<vtkm::Id>(nCells));
  conn.Allocate(static_cast<vtkm::Id>(4 * nCells));

  int index = 0;
  for(int j = 0; j < nCells; j++)
  {
    shapes.GetPortalControl().Set(j, static_cast<vtkm::UInt8>(vtkm::CELL_SHAPE_TETRA));
    numIndices.GetPortalControl().Set(j, 4);
    for(int k = 0; k < 4; k++)
      conn.GetPortalControl().Set(index++, cellVerts[j][k]);
  }

  vtkm::cont::DataSetBuilderExplicit builder;
  vtkm::cont::DataSet ds = builder.Create(coordinates, shapes, numIndices, conn);

  //Run the External Faces worklet
  vtkm::cont::DataSet new_ds = RunExternalFaces(ds);
  vtkm::cont::CellSetExplicit<> new_cs;
  new_ds.GetCellSet(0).CopyTo(new_cs);

  vtkm::Id numExtFaces_out = new_cs.GetNumberOfCells();

  //Validate the number of external faces (output) returned by the worklet
  const vtkm::Id numExtFaces_actual = 12;
  VTKM_TEST_ASSERT(numExtFaces_out == numExtFaces_actual, "Number of External Faces mismatch");

} // TestExternalFaces1

void TestExternalFaces2()
{
  std::cout << "Test 2" << std::endl;

  vtkm::cont::testing::MakeTestDataSet dataSetMaker;
  vtkm::cont::DataSet inDataSet = dataSetMaker.Make3DExplicitDataSet5();

//  vtkm::io::writer::VTKDataSetWriter writer("vtkm_explicit_data_5.vtk");
//  writer.WriteDataSet(inDataSet);

  // Expected faces
  const vtkm::IdComponent MAX_POINTS_PER_FACE = 4;
  const vtkm::Id NUM_FACES = 12;
  const vtkm::Id ExpectedExternalFaces[NUM_FACES][MAX_POINTS_PER_FACE] = {
    { 0, 3, 7, 4 },
    { 0, 1, 2, 3 },
    { 0, 4, 5, 1 },
    { 3, 2, 6, 7 },
    { 1, 5, 8, -1 },
    { 6, 2, 8, -1 },
    { 2, 1, 8, -1 },
    { 8, 10, 6, -1 },
    { 5, 10, 8, -1 },
    { 4, 7, 9, -1 },
    { 7, 6, 10, 9 },
    { 9, 10, 5, 4 }
  };

  vtkm::cont::DataSet outDataSet = RunExternalFaces(inDataSet);
  vtkm::cont::CellSetExplicit<> outCellSet;
  outDataSet.GetCellSet(0).CopyTo(outCellSet);

  VTKM_TEST_ASSERT(outCellSet.GetNumberOfCells() == NUM_FACES,
                   "Got wrong number of faces.");

  bool foundFaces[NUM_FACES];
  for (vtkm::Id faceId = 0; faceId < NUM_FACES; faceId++)
  {
    foundFaces[faceId] = false;
  }

  for (vtkm::Id dataFaceId = 0; dataFaceId < NUM_FACES; dataFaceId++)
  {
    vtkm::Vec<vtkm::Id,MAX_POINTS_PER_FACE> dataIndices(-1);
    outCellSet.GetIndices(dataFaceId, dataIndices);
    std::cout << "Looking for face " << dataIndices << std::endl;
    bool foundFace = false;
    for (vtkm::Id expectedFaceId = 0;
         expectedFaceId < NUM_FACES;
         expectedFaceId++)
    {
      vtkm::Vec<vtkm::Id,MAX_POINTS_PER_FACE> expectedIndices;
      vtkm::make_VecC(ExpectedExternalFaces[expectedFaceId], 4).
          CopyInto(expectedIndices);
      if (expectedIndices == dataIndices)
      {
        VTKM_TEST_ASSERT(!foundFaces[expectedFaceId], "Found face twice.");
        std::cout << "  found" << std::endl;
        foundFace = true;
        foundFaces[expectedFaceId] = true;
        break;
      }
    }
    VTKM_TEST_ASSERT(foundFace, "Face not found.");
  }
}

void TestExternalFaces()
{
  TestExternalFaces1();
  TestExternalFaces2();
}

}

int UnitTestExternalFaces(int, char *[])
{
  return vtkm::cont::testing::Testing::Run(TestExternalFaces);
}
