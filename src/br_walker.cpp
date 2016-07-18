#include "br_walker.h"

void BrWalker::Init() {
  Simple::Init();
  orientation_[0] = 0; // Set default color to red
  orientation_[1] -=orientation_[1];
}

void BrWalker::KickBead() {
  for (int i=0; i<n_dim_; ++i) {
    double kick = gsl_rng_uniform_pos(rng_.r) - 0.5;
    force_[i] += kick*diffusion_;
  }
}

void BrWalker::UpdatePosition() {
  KickBead();
  ApplyInteractions();
  for (int i=0; i<n_dim_; ++i)
    position_[i] = position_[i] + force_[i] * delta_ / diameter_;
  UpdatePeriodic();
  ClearInteractions();
  ZeroForce();
}

void BrWalker::UpdatePositionMP() {
    KickBead();
    for (int i = 0; i < n_dim_; ++i) {
        position_[i] = position_[i] + force_[i] * delta_ / diameter_;
        dr_tot_[i] += position_[i] + force_[i] * delta_ / diameter_;
    }
    UpdatePeriodic();
}

//void BrWalkerSpecies::InitPotentials (system_parameters *params) {
  //AddPotential(SID::br_bead, SID::br_bead, 
      //new WCA(params->lj_epsilon,params->br_bead_diameter,
                //space_, pow(2.0, 1.0/6.0)*params->br_bead_diameter));
//}


