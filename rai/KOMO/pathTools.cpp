/*  ------------------------------------------------------------------
    Copyright (c) 2011-2020 Marc Toussaint
    email: toussaint@tu-berlin.de

    This code is distributed under the MIT License.
    Please see <root-path>/LICENSE for details.
    --------------------------------------------------------------  */

#include "pathTools.h"
#include "komo.h"

#include "../Kin/F_collisions.h"

arr getVelocities_centralDifference(const arr& q, double tau) {
  arr v;
  v.resizeAs(q);
  for(uint t=1; t<q.d0-1; t++) {
    v[t] = (q[t+1]-q[t-1])/(2.*tau);
  }
  v[0] = (q[1] - q[0])/tau;
  v[-1] = (q[-1] - q[-2])/tau;
  return v;
}

arr getAccelerations_centralDifference(const arr& q, double tau) {
  arr a;
  a.resizeAs(q);
  for(uint t=1; t<q.d0-1; t++)  a[t] = (q[t+1] + q[t-1] - 2.*q[t])/(tau*tau);
  a[0] = a[1]/2.;
  a[-1] = a[-2]/2.;
  return a;
}

double getNaturalDuration(const arr& q, double maxVel, double maxAcc) {
  arr v = getVelocities_centralDifference(q, 1.);
  arr a = getVelocities_centralDifference(q, 1.);

  double vscale = maxVel / absMax(v);
  double ascale = sqrt(maxAcc / absMax(a));

  double duration = q.d0 / rai::MAX(vscale, ascale);
  return duration;
}

arr getSineProfile(const arr& q0, const arr& qT, uint T) {
  arr q(T+1, q0.N);
  for(uint t=0; t<=T; t++) q[t] = q0 + .5 * (1.-cos(RAI_PI*t/T)) * (qT-q0);
  return q;
}

arr reversePath(const arr& q) {
  uint T=q.d0-1;
  arr r(T+1, q.d1);
  for(uint t=0; t<=T; t++) r[T-t] = q[t];
  return r;
}

rai::String validatePath(const rai::Configuration& _C, const arr& q_now, const StringA& joints, const arr& q, const arr& times) {
  rai::Configuration C;
  C.copy(_C, true);

//  arr q0 = K.getJointState();

//  syncModelJointStateWithRealOrSimulation();

//  arr q_now = K.getJointState(joints);

  CHECK_EQ(q_now.N, q.d1, "");

  rai::String txt;
  txt <<"VALIDATE ";

  if(q.d0>1) {
    double startVel, endVel, maxVel=0.;
    startVel = length(q[0]-q_now)/(times(0));
    endVel = length(q[-1]-q[-2])/(times(-1)-times(-2));
    for(uint t=1; t<q.d0; t++) {
      double v = length(q[t]-q[t-1])/(times(t)-times(t-1));
      if(v>maxVel) maxVel = v;
    }
    txt <<"\nv0=" <<startVel <<" vT=" <<endVel <<" vMax=" <<maxVel;
  }
  if(joints.N<=3) {
    txt <<"\n" <<joints;
  }
  txt <<"\n";
  return txt;
}

//  PlanDrawer planDrawer(K, q_now, joints, q, tau);
//  K.gl().remove(K);
//  K.gl().add(planDrawer);
//  for(;;){
//    int key = K.watch(true, txt);

//    if(key==13){ //validated
//      K.gl().remove(planDrawer);
//      K.gl().add(K);
//      K.setJointState(q0);
//      K.watch(false, "validated");
//      return;
//    }

//    if(key==27){
//      LOG(0) <<"NO VALIDATION - exiting";
//      K.gl().closeWindow();
//      exit(0);
//    }
//  }
//}

std::pair<arr, arr> getStartGoalPath(const rai::Configuration& C, const arr& target_q, const StringA& target_joints, const char* endeff, double up, double down) {
  KOMO komo;
  komo.setModel(C, true);
  komo.setTiming(1., 20, 3.);
  komo.add_qControlObjective({}, 2, 1.);

  if(endeff) {
    if(up>0.) {
      komo.addObjective({0., up}, FS_position, {endeff}, OT_sos, {1e2}, {0., 0., .05}, 2);
    }
    if(down>0.) {
      komo.addObjective({down, 1.}, FS_position, {endeff}, OT_sos, {1e2}, {0., 0., -.05}, 2);
    }
  }

  komo.addObjective({1., 1.}, FS_qItself, target_joints, OT_eq, {1e1}, target_q);

  komo.setSlow(0., 0., 1e2, true);
  komo.setSlow(1., 1., 1e2, true);

  komo.opt.verbose=1;
  komo.optimize();

  arr path = komo.getPath_qOrg();
  path[path.d0-1] = target_q; //overwrite last config
  arr times = komo.getPath_times();
  cout <<validatePath(C, C.getJointState(), target_joints, path, times) <<endl;
  int key = komo.view(true);
  if(key=='q') {
    cout <<"ABORT!" <<endl;
    return {arr(), arr()};
  }
  return {path, times};
}

void mirrorDuplicate(std::pair<arr, arr>& path) {
  arr& q = path.first;
  arr& t = path.second;

  if(!q.N) return;

  uint T=q.d0-1;
  double D=2.*t.last();

  q.resizeCopy(2*T+1, q.d1);
  t.resizeCopy(2*T+1);
  for(uint i=1; i<=T; i++) {
    q[T+i] = q[T-i];
    t(T+i) = D - t(T-i);
  }
}

arr path_resample(const arr& q, double durationScale) {
  rai::Spline S = getSpline(q);

  uint T = durationScale * q.d0;
  durationScale = double(T)/double(q.d0);

  arr r(T, q.d1);
  for(uint t=0; t<T; t++) {
    r[t] = S.eval(double(t)/double(T-1));
  }

  return r;
}

rai::Spline getSpline(const arr& q, double duration, uint degree) {
  rai::Spline S;
  S.set(degree, q, grid(1,0.,duration, q.N-1));
  return S;
}

bool checkCollisionsAndLimits(rai::Configuration& C, const FrameL& collisionPairs, const arr& limits, bool solveForFeasible, int verbose){
  arr B;
  //-- check for limits
  if(limits.N){
    arr q = C.getJointState();
    B = ~limits;
    bool good = boundCheck(q, B[0], B[1]);
    if(!good){
      if(solveForFeasible){
        boundClip(q, B[0], B[1]);
        C.setJointState(q);
      }else{
        LOG(-2) <<"BOUNDS FAILED";
        return false;
      }
    }
  }

  //-- check for collisions!
  if(collisionPairs.N){
    CHECK_EQ(&collisionPairs.last()->C, &C, "");
    auto coll = F_PairCollision().eval(collisionPairs);
    bool doesCollide=false;
    for(uint i=0;i<coll.y.N;i++){
      if(coll.y.elem(i)>0.){
        LOG(-1) <<"in collision: " <<collisionPairs(i,0)->name <<'-' <<collisionPairs(i,1)->name <<' ' <<coll.y.elem(i);
        doesCollide=true;
      }
    }
    if(doesCollide){
      if(solveForFeasible){
        KOMO komo;
        komo.setModel(C);
        komo.setTiming(1., 1, 1., 1);
        komo.add_qControlObjective({}, 1, 1e-1);
        komo.addSquaredQuaternionNorms();

        komo.addObjective({}, FS_distance, framesToNames(collisionPairs), OT_ineq, {1e2}, {-.001});

        komo.opt.verbose=0;
        komo.optimize(0., OptOptions().set_verbose(0).set_stopTolerance(1e-3));

        if(komo.ineq>1e-1){
          LOG(-1) <<"solveForFeasible failed!" <<komo.getReport();
          if(verbose>1) komo.view(verbose>2, "FAILED!");
          return false;
        }else{
          LOG(0) <<"collisions resolved";
          if(B.N){
            bool good = boundCheck(komo.x, B[0], B[1]);
            LOG(0) <<"bounds=good after resolution: " <<good;
            if(!good) HALT("this should not be the case! collision resolution should respect bounds!");
          }
          C.setJointState(komo.x);
        }
      }else{
        LOG(-2) <<"COLLIDES!";
        return false;
      }
    }
  }
  return true;
}

bool PoseTool::checkLimits(const arr& limits, bool solve, bool assert){
  //get bounds
  arr B;
  if(limits.N) B = ~limits;
  else B = ~C.getLimits();

  //check
  arr q = C.getJointState();
  bool good = boundCheck(q, B[0], B[1]);
  if(good) return true;

  //without solve
  if(!solve){
    if(verbose) LOG(-2) <<"BOUNDS FAILED";
    if(assert) HALT("limit check failed");
    return false;
  }

  //solve
  boundClip(q, B[0], B[1]);
  C.setJointState(q);
  return true;
}

bool PoseTool::checkCollisions(const FrameL& collisionPairs, bool solve, bool assert){
  bool good=true;
  if(collisionPairs.N){
    //use explicitly given collision pairs
    CHECK_EQ(&collisionPairs.last()->C, &C, "");
    auto coll = F_PairCollision().eval(collisionPairs);
    for(uint i=0;i<coll.y.N;i++){
      if(coll.y.elem(i)>0.){
        if(verbose>1) LOG(-1) <<"in collision: " <<collisionPairs(i,0)->name <<'-' <<collisionPairs(i,1)->name <<' ' <<coll.y.elem(i);
        good=false;
      }
    }
  }else{
    //use broadphase
    C.ensure_proxies();
    double p = C.getTotalPenetration();
    if(verbose>1) C.reportProxies();
    if(p>0.) good=false;
  }

  if(good) return true;

  //without solve
  if(!solve){
    if(verbose){
      LOG(-1) <<"collision check failed";
      if(!collisionPairs.N) C.reportProxies();
    }
    if(assert) HALT("collision check failed");
    return false;
  }

  //solve
  KOMO komo;
  komo.setModel(C);
  komo.setTiming(1., 1, 1., 1);
  komo.add_qControlObjective({}, 1, 1e-1);
  komo.addSquaredQuaternionNorms();

  if(collisionPairs.N){
    komo.addObjective({}, FS_distance, framesToNames(collisionPairs), OT_ineq, {1e2}, {-.001});
  }else{
    komo.addObjective({}, FS_accumulatedCollisions, {}, OT_ineq, {1e2}, {-.001});
  }

  komo.opt.verbose=0;
  komo.optimize(0., OptOptions().set_verbose(0).set_stopTolerance(1e-3));

  if(komo.ineq>1e-1){
    if(verbose) LOG(-1) <<"solveForFeasible failed!" <<komo.getReport();
    if(verbose>1) komo.view(verbose>2, "collision resolution failed");
    if(assert) HALT("collision resolution failed");
    return false;
  }else{
    if(verbose) LOG(0) <<"collisions resolved";
//          if(B.N){
//            bool good = boundCheck(komo.x, B[0], B[1]);
//            LOG(0) <<"bounds=good after resolution: " <<good;
//            if(!good) HALT("this should not be the case! collision resolution should respect bounds!");
//          }
    C.setJointState(komo.x);
    if(verbose>1){
      C.ensure_proxies();
      double p = C.getTotalPenetration();
      if(verbose>1) C.reportProxies();
      CHECK(p<=0., "not resolved");
    }
    return true;
  }
  return true;
}

bool PoseTool::checkLimitsAndCollisions(const arr& limits, const FrameL& collisionPairs, bool solve, bool assert){
  return checkLimits(limits, solve, assert) && checkCollisions(collisionPairs, solve, assert);
}
