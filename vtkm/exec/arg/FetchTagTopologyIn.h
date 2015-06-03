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
#ifndef vtk_m_exec_arg_FetchTagTopologyIn_h
#define vtk_m_exec_arg_FetchTagTopologyIn_h

#include <vtkm/exec/arg/AspectTagDefault.h>
#include <vtkm/exec/arg/Fetch.h>

namespace vtkm {
namespace exec {
namespace arg {

/// \brief \c Fetch tag for getting topology information.
///
/// \c FetchTagTopologyIn is a tag used with the \c Fetch class to retreive
/// values from a topology object.  This default parameter returns
/// the basis topology type, i.e. cell type in a \c WorkletCellMap.
///
struct FetchTagTopologyIn {  };


template<typename Invocation, vtkm::IdComponent ParameterIndex>
struct Fetch<
    vtkm::exec::arg::FetchTagTopologyIn,
    vtkm::exec::arg::AspectTagDefault,
    Invocation,
    ParameterIndex>
{
  typedef typename Invocation::ParameterInterface::
      template ParameterType<ParameterIndex>::type ExecObjectType;

  typedef vtkm::Id ValueType;

  VTKM_EXEC_EXPORT
  ValueType Load(vtkm::Id index, const Invocation &invocation) const
  {
    return invocation.Parameters.template GetParameter<ParameterIndex>().
        GetElementShapeType(index);
  }

  VTKM_EXEC_EXPORT
  void Store(vtkm::Id, const Invocation &, const ValueType &) const
  {
    // Store is a no-op for this fetch.
  }
};

}
}
} // namespace vtkm::exec::arg

#endif //vtk_m_exec_arg_FetchTagTopologyIn_h
