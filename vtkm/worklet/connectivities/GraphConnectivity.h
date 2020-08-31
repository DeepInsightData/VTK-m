//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#ifndef vtk_m_worklet_connectivity_graph_connectivity_h
#define vtk_m_worklet_connectivity_graph_connectivity_h

#include <vtkm/cont/Invoker.h>
#include <vtkm/worklet/connectivities/CellSetDualGraph.h>
#include <vtkm/worklet/connectivities/InnerJoin.h>
#include <vtkm/worklet/connectivities/UnionFind.h>

namespace vtkm
{
namespace worklet
{
namespace connectivity
{
namespace detail
{
class GraphGraft : public vtkm::worklet::WorkletMapField
{
public:
  using ControlSignature = void(FieldIn start,
                                FieldIn degree,
                                WholeArrayIn ids,
                                AtomicArrayInOut comp);

  using ExecutionSignature = void(WorkIndex, _1, _2, _3, _4);

  // TODO: Use Scatter?
  template <typename InPortalType, typename AtomicCompInOut>
  VTKM_EXEC void operator()(vtkm::Id index,
                            vtkm::Id start,
                            vtkm::Id degree,
                            const InPortalType& conn,
                            AtomicCompInOut& comp) const
  {
    for (vtkm::Id offset = start; offset < start + degree; offset++)
    {
      vtkm::Id neighbor = conn.Get(offset);

      // We need to reload thisComp and thatComp every iteration since
      // they might have been changed by Unite() both as a result of
      // attaching on tree to the other or as a result of path compression
      // in findRoot().
      auto thisComp = comp.Get(index);
      auto thatComp = comp.Get(neighbor);

      // Merge the two components one way or the other, the order will
      // be resolved by Unite().
      UnionFind::Unite(comp, thisComp, thatComp);
    }
  }
};
}

// Single pass connected component algorithm from
// Jaiganesh, Jayadharini, and Martin Burtscher.
// "A high-performance connected components implementation for GPUs."
// Proceedings of the 27th International Symposium on High-Performance
// Parallel and Distributed Computing. 2018.
class GraphConnectivity
{
public:
  using Algorithm = vtkm::cont::Algorithm;

  template <typename InputPortalType, typename OutputPortalType>
  void Run(const InputPortalType& numIndicesArray,
           const InputPortalType& indexOffsetsArray,
           const InputPortalType& connectivityArray,
           OutputPortalType& componentsOut) const
  {
    vtkm::cont::ArrayHandle<vtkm::Id> components;
    Algorithm::Copy(
      vtkm::cont::ArrayHandleCounting<vtkm::Id>(0, 1, numIndicesArray.GetNumberOfValues()),
      components);

    // TODO: give the reason that single pass algorithm works.
    vtkm::cont::Invoker invoke;
    invoke(detail::GraphGraft{}, indexOffsetsArray, numIndicesArray, connectivityArray, components);
    invoke(PointerJumping{}, components);

    // renumber connected component to the range of [0, number of components).
    vtkm::cont::ArrayHandle<vtkm::Id> uniqueComponents;
    Algorithm::Copy(components, uniqueComponents);
    Algorithm::Sort(uniqueComponents);
    Algorithm::Unique(uniqueComponents);

    vtkm::cont::ArrayHandle<vtkm::Id> cellIds;
    Algorithm::Copy(
      vtkm::cont::ArrayHandleCounting<vtkm::Id>(0, 1, numIndicesArray.GetNumberOfValues()),
      cellIds);

    vtkm::cont::ArrayHandle<vtkm::Id> uniqueColor;
    Algorithm::Copy(
      vtkm::cont::ArrayHandleCounting<vtkm::Id>(0, 1, uniqueComponents.GetNumberOfValues()),
      uniqueColor);
    vtkm::cont::ArrayHandle<vtkm::Id> cellColors;
    vtkm::cont::ArrayHandle<vtkm::Id> cellIdsOut;
    InnerJoin().Run(
      components, cellIds, uniqueComponents, uniqueColor, cellColors, cellIdsOut, componentsOut);

    Algorithm::SortByKey(cellIdsOut, componentsOut);
  }
};
}
}
}
#endif //vtk_m_worklet_connectivity_graph_connectivity_h
