//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#include <vtkm/io/VTKDataSetWriter.h>

#include <vtkm/CellShape.h>

#include <vtkm/cont/CellSetExplicit.h>
#include <vtkm/cont/CellSetSingleType.h>
#include <vtkm/cont/CellSetStructured.h>
#include <vtkm/cont/ErrorBadType.h>
#include <vtkm/cont/ErrorBadValue.h>
#include <vtkm/cont/Field.h>

#include <vtkm/io/ErrorIO.h>

#include <vtkm/io/internal/VTKDataSetTypes.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

template <typename T>
using ArrayHandleRectilinearCoordinates =
  vtkm::cont::ArrayHandleCartesianProduct<vtkm::cont::ArrayHandle<T>,
                                          vtkm::cont::ArrayHandle<T>,
                                          vtkm::cont::ArrayHandle<T>>;

struct OutputPointsFunctor
{
private:
  std::ostream& out;

  template <typename PortalType>
  VTKM_CONT void Output(const PortalType& portal) const
  {
    for (vtkm::Id index = 0; index < portal.GetNumberOfValues(); index++)
    {
      const int VTKDims = 3; // VTK files always require 3 dims for points

      using ValueType = typename PortalType::ValueType;
      using VecType = typename vtkm::VecTraits<ValueType>;

      const ValueType& value = portal.Get(index);

      vtkm::IdComponent numComponents = VecType::GetNumberOfComponents(value);
      for (vtkm::IdComponent c = 0; c < numComponents && c < VTKDims; c++)
      {
        out << (c == 0 ? "" : " ") << VecType::GetComponent(value, c);
      }
      for (vtkm::IdComponent c = numComponents; c < VTKDims; c++)
      {
        out << " 0";
      }
      out << '\n';
    }
  }

public:
  VTKM_CONT
  OutputPointsFunctor(std::ostream& o)
    : out(o)
  {
  }

  template <typename T, typename Storage>
  VTKM_CONT void operator()(const vtkm::cont::ArrayHandle<T, Storage>& array) const
  {
    this->Output(array.ReadPortal());
  }
};

struct OutputFieldFunctor
{
private:
  std::ostream& out;

  template <typename PortalType>
  VTKM_CONT void Output(const PortalType& portal) const
  {
    for (vtkm::Id index = 0; index < portal.GetNumberOfValues(); index++)
    {
      using ValueType = typename PortalType::ValueType;
      using VecType = typename vtkm::VecTraits<ValueType>;

      const ValueType& value = portal.Get(index);

      vtkm::IdComponent numComponents = VecType::GetNumberOfComponents(value);
      for (vtkm::IdComponent c = 0; c < numComponents; c++)
      {
        out << (c == 0 ? "" : " ") << VecType::GetComponent(value, c);
      }
      out << '\n';
    }
  }

public:
  VTKM_CONT
  OutputFieldFunctor(std::ostream& o)
    : out(o)
  {
  }

  template <typename T, typename Storage>
  VTKM_CONT void operator()(const vtkm::cont::ArrayHandle<T, Storage>& array) const
  {
    this->Output(array.ReadPortal());
  }
};

class GetDataTypeName
{
public:
  GetDataTypeName(std::string& name)
    : Name(&name)
  {
  }

  template <typename ArrayHandleType>
  void operator()(const ArrayHandleType&) const
  {
    using DataType = typename vtkm::VecTraits<typename ArrayHandleType::ValueType>::ComponentType;
    *this->Name = vtkm::io::internal::DataTypeName<DataType>::Name();
  }

private:
  std::string* Name;
};

template <vtkm::IdComponent DIM>
void WriteDimensions(std::ostream& out, const vtkm::cont::CellSetStructured<DIM>& cellSet)
{
  auto pointDimensions = cellSet.GetPointDimensions();
  using VTraits = vtkm::VecTraits<decltype(pointDimensions)>;

  out << "DIMENSIONS ";
  out << VTraits::GetComponent(pointDimensions, 0) << " ";
  out << (DIM > 1 ? VTraits::GetComponent(pointDimensions, 1) : 1) << " ";
  out << (DIM > 2 ? VTraits::GetComponent(pointDimensions, 2) : 1) << "\n";
}

void WritePoints(std::ostream& out, const vtkm::cont::DataSet& dataSet)
{
  ///\todo: support other coordinate systems
  int cindex = 0;
  auto cdata = dataSet.GetCoordinateSystem(cindex).GetData();

  std::string typeName;
  vtkm::cont::CastAndCall(cdata, GetDataTypeName(typeName));

  vtkm::Id npoints = cdata.GetNumberOfValues();
  out << "POINTS " << npoints << " " << typeName << " " << '\n';

  cdata.CastAndCall(OutputPointsFunctor{ out });
}

template <class CellSetType>
void WriteExplicitCells(std::ostream& out, const CellSetType& cellSet)
{
  vtkm::Id nCells = cellSet.GetNumberOfCells();

  vtkm::Id conn_length = 0;
  for (vtkm::Id i = 0; i < nCells; ++i)
  {
    conn_length += 1 + cellSet.GetNumberOfPointsInCell(i);
  }

  out << "CELLS " << nCells << " " << conn_length << '\n';

  for (vtkm::Id i = 0; i < nCells; ++i)
  {
    vtkm::cont::ArrayHandle<vtkm::Id> ids;
    vtkm::Id nids = cellSet.GetNumberOfPointsInCell(i);
    cellSet.GetIndices(i, ids);
    out << nids;
    auto IdPortal = ids.ReadPortal();
    for (int j = 0; j < nids; ++j)
      out << " " << IdPortal.Get(j);
    out << '\n';
  }

  out << "CELL_TYPES " << nCells << '\n';
  for (vtkm::Id i = 0; i < nCells; ++i)
  {
    vtkm::Id shape = cellSet.GetCellShape(i);
    out << shape << '\n';
  }
}

void WritePointFields(std::ostream& out, const vtkm::cont::DataSet& dataSet)
{
  bool wrote_header = false;
  for (vtkm::Id f = 0; f < dataSet.GetNumberOfFields(); f++)
  {
    const vtkm::cont::Field field = dataSet.GetField(f);

    if (field.GetAssociation() != vtkm::cont::Field::Association::POINTS)
    {
      continue;
    }

    vtkm::Id npoints = field.GetNumberOfValues();
    int ncomps = field.GetData().GetNumberOfComponents();
    if (ncomps > 4)
    {
      continue;
    }

    if (!wrote_header)
    {
      out << "POINT_DATA " << npoints << '\n';
      wrote_header = true;
    }

    std::string typeName;
    vtkm::cont::CastAndCall(field.GetData().ResetTypes(vtkm::TypeListAll{}),
                            GetDataTypeName(typeName));
    std::string name = field.GetName();
    for (auto& c : name)
    {
      if (std::isspace(c))
      {
        c = '_';
      }
    }
    out << "SCALARS " << name << " " << typeName << " " << ncomps << '\n';
    out << "LOOKUP_TABLE default" << '\n';

    vtkm::cont::CastAndCall(field.GetData().ResetTypes(vtkm::TypeListAll{}),
                            OutputFieldFunctor(out));
  }
}

void WriteCellFields(std::ostream& out, const vtkm::cont::DataSet& dataSet)
{
  bool wrote_header = false;
  for (vtkm::Id f = 0; f < dataSet.GetNumberOfFields(); f++)
  {
    const vtkm::cont::Field field = dataSet.GetField(f);
    if (!field.IsFieldCell())
    {
      continue;
    }


    vtkm::Id ncells = field.GetNumberOfValues();
    int ncomps = field.GetData().GetNumberOfComponents();
    if (ncomps > 4)
      continue;

    if (!wrote_header)
    {
      out << "CELL_DATA " << ncells << '\n';
      wrote_header = true;
    }

    std::string typeName;
    vtkm::cont::CastAndCall(field.GetData().ResetTypes(vtkm::TypeListAll{}),
                            GetDataTypeName(typeName));

    std::string name = field.GetName();
    for (auto& c : name)
    {
      if (std::isspace(c))
      {
        c = '_';
      }
    }

    out << "SCALARS " << name << " " << typeName << " " << ncomps << '\n';
    out << "LOOKUP_TABLE default" << '\n';

    vtkm::cont::CastAndCall(field.GetData().ResetTypes(vtkm::TypeListAll{}),
                            OutputFieldFunctor(out));
  }
}

template <class CellSetType>
void WriteDataSetAsUnstructured(std::ostream& out,
                                const vtkm::cont::DataSet& dataSet,
                                const CellSetType& cellSet)
{
  out << "DATASET UNSTRUCTURED_GRID" << '\n';
  WritePoints(out, dataSet);
  WriteExplicitCells(out, cellSet);
}

template <vtkm::IdComponent DIM>
void WriteDataSetAsStructuredPoints(std::ostream& out,
                                    const vtkm::cont::ArrayHandleUniformPointCoordinates& points,
                                    const vtkm::cont::CellSetStructured<DIM>& cellSet)
{
  out << "DATASET STRUCTURED_POINTS\n";

  WriteDimensions(out, cellSet);

  auto portal = points.ReadPortal();
  auto origin = portal.GetOrigin();
  auto spacing = portal.GetSpacing();
  out << "ORIGIN " << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
  out << "SPACING " << spacing[0] << " " << spacing[1] << " " << spacing[2] << "\n";
}

template <typename T, vtkm::IdComponent DIM>
void WriteDataSetAsRectilinearGrid(std::ostream& out,
                                   const ArrayHandleRectilinearCoordinates<T>& points,
                                   const vtkm::cont::CellSetStructured<DIM>& cellSet)
{
  out << "DATASET RECTILINEAR_GRID\n";

  WriteDimensions(out, cellSet);

  std::string typeName = vtkm::io::internal::DataTypeName<T>::Name();
  vtkm::cont::ArrayHandle<T> dimArray;

  dimArray = points.GetStorage().GetFirstArray();
  out << "X_COORDINATES " << dimArray.GetNumberOfValues() << " " << typeName << "\n";
  OutputFieldFunctor{ out }(dimArray);

  dimArray = points.GetStorage().GetSecondArray();
  out << "Y_COORDINATES " << dimArray.GetNumberOfValues() << " " << typeName << "\n";
  OutputFieldFunctor{ out }(dimArray);

  dimArray = points.GetStorage().GetThirdArray();
  out << "Z_COORDINATES " << dimArray.GetNumberOfValues() << " " << typeName << "\n";
  OutputFieldFunctor{ out }(dimArray);
}

template <vtkm::IdComponent DIM>
void WriteDataSetAsStructuredGrid(std::ostream& out,
                                  const vtkm::cont::DataSet& dataSet,
                                  const vtkm::cont::CellSetStructured<DIM>& cellSet)
{
  out << "DATASET STRUCTURED_GRID" << '\n';

  WriteDimensions(out, cellSet);

  WritePoints(out, dataSet);
}

template <vtkm::IdComponent DIM>
void WriteDataSetAsStructured(std::ostream& out,
                              const vtkm::cont::DataSet& dataSet,
                              const vtkm::cont::CellSetStructured<DIM>& cellSet)
{
  ///\todo: support rectilinear

  // Type of structured grid (uniform, rectilinear, curvilinear) is determined by coordinate system
  auto coordSystem = dataSet.GetCoordinateSystem().GetData();
  if (coordSystem.IsType<vtkm::cont::ArrayHandleUniformPointCoordinates>())
  {
    // uniform is written as "structured points"
    WriteDataSetAsStructuredPoints(
      out, coordSystem.Cast<vtkm::cont::ArrayHandleUniformPointCoordinates>(), cellSet);
  }
  else if (coordSystem.IsType<ArrayHandleRectilinearCoordinates<vtkm::Float32>>())
  {
    WriteDataSetAsRectilinearGrid(
      out, coordSystem.Cast<ArrayHandleRectilinearCoordinates<vtkm::Float32>>(), cellSet);
  }
  else if (coordSystem.IsType<ArrayHandleRectilinearCoordinates<vtkm::Float64>>())
  {
    WriteDataSetAsRectilinearGrid(
      out, coordSystem.Cast<ArrayHandleRectilinearCoordinates<vtkm::Float64>>(), cellSet);
  }
  else
  {
    // Curvilinear is written as "structured grid"
    WriteDataSetAsStructuredGrid(out, dataSet, cellSet);
  }
}

void Write(std::ostream& out, const vtkm::cont::DataSet& dataSet)
{
  // The Paraview parser cannot handle scientific notation:
  out << std::fixed;
  out << "# vtk DataFile Version 3.0" << '\n';
  out << "vtk output" << '\n';
  out << "ASCII" << '\n';

  vtkm::cont::DynamicCellSet cellSet = dataSet.GetCellSet();
  if (cellSet.IsType<vtkm::cont::CellSetExplicit<>>())
  {
    WriteDataSetAsUnstructured(out, dataSet, cellSet.Cast<vtkm::cont::CellSetExplicit<>>());
  }
  else if (cellSet.IsType<vtkm::cont::CellSetStructured<1>>())
  {
    WriteDataSetAsStructured(out, dataSet, cellSet.Cast<vtkm::cont::CellSetStructured<1>>());
  }
  else if (cellSet.IsType<vtkm::cont::CellSetStructured<2>>())
  {
    WriteDataSetAsStructured(out, dataSet, cellSet.Cast<vtkm::cont::CellSetStructured<2>>());
  }
  else if (cellSet.IsType<vtkm::cont::CellSetStructured<3>>())
  {
    WriteDataSetAsStructured(out, dataSet, cellSet.Cast<vtkm::cont::CellSetStructured<3>>());
  }
  else if (cellSet.IsType<vtkm::cont::CellSetSingleType<>>())
  {
    // these function just like explicit cell sets
    WriteDataSetAsUnstructured(out, dataSet, cellSet.Cast<vtkm::cont::CellSetSingleType<>>());
  }
  else
  {
    throw vtkm::cont::ErrorBadType("Could not determine type to write out.");
  }

  WritePointFields(out, dataSet);
  WriteCellFields(out, dataSet);
}

} // anonymous namespace

namespace vtkm
{
namespace io
{

VTKDataSetWriter::VTKDataSetWriter(const char* fileName)
  : FileName(fileName)
{
}

VTKDataSetWriter::VTKDataSetWriter(const std::string& fileName)
  : FileName(fileName)
{
}

void VTKDataSetWriter::WriteDataSet(const vtkm::cont::DataSet& dataSet) const
{
  if (dataSet.GetNumberOfCoordinateSystems() < 1)
  {
    throw vtkm::cont::ErrorBadValue(
      "DataSet has no coordinate system, which is not supported by VTK file format.");
  }
  try
  {
    std::ofstream fileStream(this->FileName.c_str(), std::fstream::trunc);
    Write(fileStream, dataSet);
    fileStream.close();
  }
  catch (std::ofstream::failure& error)
  {
    throw vtkm::io::ErrorIO(error.what());
  }
}
}
} // namespace vtkm::io
