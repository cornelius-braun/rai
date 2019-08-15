/*  ------------------------------------------------------------------
    Copyright (c) 2017 Marc Toussaint
    email: marc.toussaint@informatik.uni-stuttgart.de

    This code is distributed under the MIT License.
    Please see <root-path>/LICENSE for details.
    --------------------------------------------------------------  */

#pragma once

#include "feature.h"

struct TM_Energy : Feature {
  double gravity=9.81;

  TM_Energy();

  virtual void phi(arr& y, arr& J, const rai::KinematicWorld& K) { HALT("can only be of higher order"); }
  virtual uint dim_phi(const rai::KinematicWorld& K) { HALT("can only be of higher order"); }

  virtual void phi(arr& y, arr& J, const WorldL& Ktuple);
  virtual uint dim_phi(const WorldL& Ktuple);

  virtual rai::String shortTag(const rai::KinematicWorld& G) { return STRING("Energy-"<<order); }
};
