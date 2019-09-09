//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

namespace vtkm
{
namespace filter
{

//-----------------------------------------------------------------------------
inline VTKM_CONT MaskPoints::MaskPoints()
  : vtkm::filter::FilterDataSet<MaskPoints>()
  , Stride(1)
  , CompactPoints(true)
{
}

//-----------------------------------------------------------------------------
template <typename DerivedPolicy>
inline VTKM_CONT vtkm::cont::DataSet MaskPoints::DoExecute(
  const vtkm::cont::DataSet& input,
  vtkm::filter::PolicyBase<DerivedPolicy> policy)
{
  // extract the input cell set
  const vtkm::cont::DynamicCellSet& cells = input.GetCellSet();

  // run the worklet on the cell set and input field
  vtkm::cont::CellSetSingleType<> outCellSet;
  vtkm::worklet::MaskPoints worklet;

  outCellSet = worklet.Run(vtkm::filter::ApplyPolicyCellSet(cells, policy), this->Stride);

  // create the output dataset
  vtkm::cont::DataSet output;
  output.SetCellSet(outCellSet);
  output.AddCoordinateSystem(input.GetCoordinateSystem(this->GetActiveCoordinateSystemIndex()));

  // compact the unused points in the output dataset
  if (this->CompactPoints)
  {
    this->Compactor.SetCompactPointFields(true);
    this->Compactor.SetMergePoints(false);
    return this->Compactor.Execute(output, PolicyDefault{});
  }
  else
  {
    return output;
  }
}

//-----------------------------------------------------------------------------
template <typename T, typename StorageType, typename DerivedPolicy>
inline VTKM_CONT bool MaskPoints::DoMapField(vtkm::cont::DataSet& result,
                                             const vtkm::cont::ArrayHandle<T, StorageType>& input,
                                             const vtkm::filter::FieldMetadata& fieldMeta,
                                             vtkm::filter::PolicyBase<DerivedPolicy> policy)
{
  // point data is copied as is because it was not collapsed
  if (fieldMeta.IsPointField())
  {
    if (this->CompactPoints)
    {
      return this->Compactor.DoMapField(result, input, fieldMeta, policy);
    }
    else
    {
      result.AddField(fieldMeta.AsField(input));
      return true;
    }
  }

  // cell data does not apply
  return false;
}
}
}
