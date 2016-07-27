// Implementation for simple binding and unbinding

#include <cassert>

#include "xlink_kmc.h"

#include "xlink.h"
#include "xlink_head.h"
#include "xlink_helpers.h"
#include "br_rod.h"

void XlinkKMC::Init(space_struct *pSpace,
                        ParticleTracking *pTracking,
                        SpeciesBase *spec1,
                        SpeciesBase *spec2,
                        int ikmc,
                        YAML::Node &node,
                        long seed) {
  KMCBase::Init(pSpace, pTracking, spec1, spec2, ikmc, node, seed);

  // Grab our specific claims
  // eps effective
  switch (node["kmc"][ikmc]["concentration_0_1"].Type()) {
    case YAML::NodeType::Scalar:
      eps_eff_0_1_[0] = eps_eff_0_1_[1] =
        0.5 * node["kmc"][ikmc]["concentration_0_1"].as<double>();
      break;
    case YAML::NodeType::Sequence:
      eps_eff_0_1_[0] = node["kmc"][ikmc]["concentration_0_1"][0].as<double>();
      eps_eff_0_1_[1] = node["kmc"][ikmc]["concentration_0_1"][1].as<double>();
      break;
  }
  switch (node["kmc"][ikmc]["concentration_1_2"].Type()) {
    case YAML::NodeType::Scalar:
      eps_eff_1_2_[0] = eps_eff_1_2_[1] =
        0.5 * node["kmc"][ikmc]["concentration_1_2"].as<double>();
      break;
    case YAML::NodeType::Sequence:
      eps_eff_1_2_[0] = node["kmc"][ikmc]["concentration_1_2"][0].as<double>();
      eps_eff_1_2_[1] = node["kmc"][ikmc]["concentration_1_2"][1].as<double>();
      break;
  }

  // on rates
  switch (node["kmc"][ikmc]["on_rate_0_1"].Type()) {
    case YAML::NodeType::Scalar:
      on_rate_0_1_[0] = on_rate_0_1_[1] =
        node["kmc"][ikmc]["on_rate_0_1"].as<double>();
      break;
    case YAML::NodeType::Sequence:
      on_rate_0_1_[0] = node["kmc"][ikmc]["on_rate_0_1"][0].as<double>();
      on_rate_0_1_[1] = node["kmc"][ikmc]["on_rate_0_1"][1].as<double>();
      break;
  }
  switch (node["kmc"][ikmc]["on_rate_1_2"].Type()) {
    case YAML::NodeType::Scalar:
      on_rate_1_2_[0] = on_rate_1_2_[1] =
        node["kmc"][ikmc]["on_rate_1_2"].as<double>();
      break;
    case YAML::NodeType::Sequence:
      on_rate_1_2_[0] = node["kmc"][ikmc]["on_rate_1_2"][0].as<double>();
      on_rate_1_2_[1] = node["kmc"][ikmc]["on_rate_1_2"][1].as<double>();
      break;
  }

  alpha_    = node["kmc"][ikmc]["alpha"].as<double>();
  rcutoff_0_1_    = node["kmc"][ikmc]["rcut"].as<double>();
  velocity_ = node["kmc"][ikmc]["velocity"].as<double>();
  barrier_weight_ = node["kmc"][ikmc]["barrier_weight"].as<double>();
  k_stretch_ = node["kmc"][ikmc]["spring_constant"].as<double>();
  r_equil_ = node["kmc"][ikmc]["equilibrium_length"].as<double>();

  CalcCutoff();

  BuildTables();
}

void XlinkKMC::CalcCutoff() {
  BrRodSpecies *prspec = dynamic_cast<BrRodSpecies*>(spec2_);
  // XXX FIXME set back to the correct value
  max_length_ = 110;
  //max_length_ = prspec->GetMaxLength();
  rcutoff_1_2_ = 0.0;
  const double temp = 1.0;
  const double smalleps = 1E-3;
  double eps_eff = eps_eff_1_2_[0] + eps_eff_1_2_[1];
  double rc_0 = sqrt(2.0 / ( (1-barrier_weight_) * k_stretch_) * temp * log(eps_eff * max_length_ / smalleps * sqrt(2.0 * temp / k_stretch_)));
  rcutoff_1_2_ = r_equil_ + rc_0;
}

double XlinkKMC::XKMCErfinv(double x) {
  // See: A handy approximation for the error function and its inverse
  // (Winitzki 2008) (google it).  This isn't a great approximation, but
  // it will do the trick.  It's not programmed for efficiency since it should
  // only be called during program startup once. If you need better prevision
  // or performance, boost apparently has a version (but then we need boot)
  const double a = 0.147;
  double t1 = -2/M_PI/a;
  double t2 = -log(1-x*x)/2;
  double t3 = 2/M_PI/a + log(1-x*x)/2;
  double t4 = -log(1-x*x)/a;

  return sqrt(t1 + t2 + sqrt(t3*t3 + t4));
}

void XlinkKMC::BuildTables() {
  std::vector<double> x[2];
  double bin_size = 0.05;
  double alpha = k_stretch_ * (1 - barrier_weight_) / 2;
  double const smalleps = 1E-5;
  double a_cutoff = 1/sqrt(alpha) * XKMCErfinv(1 - 4.0*sqrt(alpha/M_PI)*smalleps) +
    r_equil_;
  double y_cutoff = rcutoff_1_2_;
  
  xlh::xlink_params params;
  params.alpha = alpha;
  params.r0 = r_equil_;

  for (double a = 0.0; a <= a_cutoff; a += bin_size)
    x[0].push_back(a);
  for (double y0 = 0.0; y0 <= y_cutoff; y0 += bin_size)
    x[1].push_back(y0);

  n_exp_lookup_.Init(2, x, &xlh::prob_1_2, &params);
}

void XlinkKMC::Print() {
  printf("Xlink - BR Rod KMC Module\n");
  KMCBase::Print();
  printf("\t {eps_eff 0 -> 1}: [%2.2f, %2.2f]\n", eps_eff_0_1_[0], eps_eff_0_1_[1]);
  printf("\t {eps_eff 1 -> 2}: [%2.2f, %2.2f]\n", eps_eff_1_2_[0], eps_eff_1_2_[1]);
  printf("\t {on_rate 0 -> 1}: [%2.8f, %2.8f]\n", on_rate_0_1_[0], on_rate_0_1_[1]);
  printf("\t {on_rate 1 -> 2}: [%2.8f, %2.8f]\n", on_rate_1_2_[0], on_rate_1_2_[1]);
  printf("\t {barrier_weight: %2.10f}\n", barrier_weight_);
  printf("\t {equilibrium_length: %2.4f}\n", r_equil_);
  printf("\t {k_spring: %2.4f}\n", k_stretch_);
  printf("\t {rcutoff_0_1: %2.8f}\n", rcutoff_0_1_);
  printf("\t {rcutoff_1_2: %2.8f}\n", rcutoff_1_2_);
  printf("\t {alpha: %2.4f}\n", alpha_);
}

void XlinkKMC::PrepKMC() {
  simples_ = tracking_->GetSimples();
  nsimples_ = tracking_->GetNSimples();
  oid_position_map_ = tracking_->GetOIDPositionMap();
  neighbors_ = tracking_->GetNeighbors();

  // Prepare each composite particle for the upcoming kmc step
  if (!spec1_->IsKMC()) return;
  XlinkSpecies* pxspec = dynamic_cast<XlinkSpecies*>(spec1_);
  double ntot_0_1 = 0.0;
  double ntot_1_2 = 0.0;
  auto xlinks = pxspec->GetXlinks(); 

  for (auto xit = xlinks->begin(); xit != xlinks->end(); ++xit) {
    // Determine what state we're in so that we can do the appropriate thing
    // XXX Do the rest of this
    switch((*xit)->GetBoundState()) {
      case unbound:
        Update_0_1(*xit);
        ntot_0_1 += (*xit)->GetNExp_0_1();
        break;
      case singly:
        Update_1_2(*xit);
        ntot_1_2 += (*xit)->GetNExp_1_2();
        break;
    }
  }

  pxspec->SetNExp_0_1(ntot_0_1);
  pxspec->SetNExp_1_2(ntot_1_2);
}

void XlinkKMC::Update_0_1(Xlink* xit) {
  double nexp_xlink = 0.0;
  auto heads = xit->GetHeads();

  for (int i = 0; i < heads->size(); ++i) {
    auto head = &(*heads)[i];
    double nexp = 0.0;
    double binding_affinity = eps_eff_0_1_[i] * on_rate_0_1_[i] * alpha_ * xit->GetDelta();
    auto idx = (*oid_position_map_)[head->GetOID()];
    for (auto nldx = neighbors_[idx].begin(); nldx != neighbors_[idx].end(); ++nldx) {
      nexp += binding_affinity * nldx->kmc_; 
    }
    head->SetNExp_0_1(nexp);
    nexp_xlink += nexp;
  }

  xit->SetNExp_0_1(nexp_xlink);
}

void XlinkKMC::Update_1_2(Xlink *xit) {
  auto heads = xit->GetHeads();
  auto head0 = heads->begin();
  auto head1 = heads->begin()+1;
  int free_i, attc_i;
  XlinkHead *attachedhead;
  XlinkHead *freehead;
  if (head0->GetBound()) {
    attc_i = 0;
    free_i = 1;
    attachedhead = &(*head0);
    freehead = &(*head1);
  } else {
    attc_i = 1;
    free_i = 0;
    attachedhead = &(*head1);
    freehead = &(*head0);
  }
  double binding_affinity = eps_eff_1_2_[free_i] * on_rate_1_2_[free_i];
  auto free_idx = (*oid_position_map_)[freehead->GetOID()];
  auto attc_idx = (*oid_position_map_)[attachedhead->GetOID()];
  // Get the attached rod
  auto attach_info = attachedhead->GetAttach();
  auto mrod_attached = (*simples_)[attach_info.first];
  if (binding_affinity > 0.0) {
    double n_exp = 0.0;
    //xit->Dump();
    //xit->DumpKMC();
    //mrod_attached->Dump();
    //printf("attc head {idx:%d,head:%d}[%d] -> [rid:%d]\n", attc_idx, attc_i, attachedhead->GetOID(),
    //    mrod_attached->GetRID());
    //printf("free head {idx:%d,head:%d}[%d]\n", free_idx, free_i, freehead->GetOID());

    // We have to look at all of our neighbors withint the mrcut
    for (auto nldx = neighbors_[free_idx].begin(); nldx != neighbors_[free_idx].end(); ++nldx) {
      auto mrod = (*simples_)[nldx->idx_];
      //printf("Checking against [rid:%d]\n", mrod->GetRID());
      // Check to see if it's really a rod, and if it's the same one we're already attached to
      if (mrod->GetSID() != sid2_) continue;
      if (mrod->GetRID() == mrod_attached->GetRID()) {
        //printf("\tExcluding self attachment\n");
        continue;
      }
      //printf("Adding contribution from neighbor [%d,rid:%d]\n", nldx->idx_, mrod->GetRID());
      // Calculate center to center displacement
      // XXX FIXME CJE possibly don't do this, and store the minimum distance calculation from
      // earlier point point calculation

      // XXX FIXME Polar Affinity
      double polar_affinity = 1.0;
      double r_x[3];
      double r_rod[3];
      double s_rod[3];
      double u_rod[3];
      std::copy(freehead->GetRigidPosition(), freehead->GetRigidPosition()+ndim_, r_x);
      std::copy(mrod->GetRigidPosition(), mrod->GetRigidPosition()+ndim_, r_rod);
      std::copy(mrod->GetRigidScaledPosition(), mrod->GetRigidScaledPosition()+ndim_, s_rod);
      std::copy(mrod->GetRigidOrientation(), mrod->GetRigidOrientation()+ndim_, u_rod);
      double l_rod = mrod->GetRigidLength();
      double *s_1 = NULL; // FIXME from robert
      double rcontact[3];
      double dr[3];
      double mu0 = 0.0;
      min_distance_point_carrier_line(ndim_, nperiodic_,
                                      space_->unit_cell, r_x, s_1,
                                      r_rod, s_rod, u_rod, l_rod,
                                      dr, rcontact, &mu0);
      //printf("{dr: (%2.4f, %2.4f)}, {rcontact: (%2.4f, %2.4f)}, {mu: %2.4f}\n",
      //    dr[0], dr[1], rcontact[0], rcontact[1], mu0);

      // Now do the integration over the limits on the MT
      // Check the cutoff distance
      double lim0 = -mu0 - 0.5 * l_rod;
      double lim1 = -mu0 + 0.5 * l_rod;
      double r_min_mag2 = 0.0;
      for (int i = 0; i < ndim_; ++i) {
        r_min_mag2 += SQR(dr[i]);
      }
      double r_min_mag = sqrt(r_min_mag2);
      double x[2] = {fabs(lim0), r_min_mag};
      double term0 = n_exp_lookup_.Lookup(x) * ((lim0 < 0) ? -1.0 : 1.0);
      x[0] = fabs(lim1);
      double term1 = n_exp_lookup_.Lookup(x) * ((lim1 < 0) ? -1.0 : 1.0);
      // OVERRIDE the kmc_ value of this neighbor list
      nldx->kmc_ = (term1 - term0) * polar_affinity;
      //printf("{nexp cont: %2.8f}\n", nldx->kmc_);
      n_exp += nldx->kmc_;
    } // loop over local neighbors of xlink

    freehead->SetNExp_1_2(n_exp);
    xit->SetNExp_1_2(n_exp);
    //xit->DumpKMC();
  }
}

void XlinkKMC::StepKMC() {
  simples_ = tracking_->GetSimples();
  nsimples_ = tracking_->GetNSimples();
  oid_position_map_ = tracking_->GetOIDPositionMap();
  neighbors_ = tracking_->GetNeighbors();

  // Run the bind unbind
  int g[4] = {0, 1, 2, 3};
  for (int i = 0; i < 4; ++i) {
    int j = gsl_rng_uniform_int(rng_.r, 4);
    int swapme = g[i];
    g[i] = g[j];
    g[j] = swapme;
  }

  if (debug_trace)
    printf("XlinkKMC module %d -> %d -> %d -> %d\n", g[0], g[1], g[2], g[3]);

  for (int i = 0; i < 4; ++i) {
    switch (g[i]) {
      case 0:
        KMC_0_1();
        break;
      case 1:
        KMC_1_0();
        break;
      case 2:
        KMC_1_2();
        break;
      case 3:
        KMC_2_1();
        break;
    }
  }
}

void XlinkKMC::KMC_0_1() {
  // Loop over the xlinks to see who binds
  XlinkSpecies* pxspec = dynamic_cast<XlinkSpecies*>(spec1_);
  auto xlinks = pxspec->GetXlinks();
  for (auto xit = xlinks->begin(); xit != xlinks->end(); ++xit) {
    // Only take free ones
    if ((*xit)->GetBoundState() != unbound) continue;
    auto nexp = (*xit)->GetNExp_0_1();
    if (nexp <  std::numeric_limits<double>::epsilon() &&
        nexp > -std::numeric_limits<double>::epsilon()) nexp = 0.0;
    // IF we have some probability to fall onto a neighbor, check it
    if (nexp > 0.0) {
      auto mrng = (*xit)->GetRNG();
      double roll = gsl_rng_uniform(mrng->r);
      if (roll < nexp) {
        int head_type = gsl_rng_uniform(mrng->r) < ((eps_eff_0_1_[1])/(eps_eff_0_1_[0]+eps_eff_0_1_[1]));
        auto heads = (*xit)->GetHeads();
        auto head = heads->begin() + head_type;
        double binding_affinity = (eps_eff_0_1_[0] * on_rate_0_1_[0] + eps_eff_0_1_[1] * on_rate_0_1_[1]) *
          alpha_ * head->GetDelta();
        if (debug_trace)
          printf("[%d] Successful KMC move {0 -> 1}, {nexp: %2.4f}, {roll: %2.4f}, {head: %d}\n", (*xit)->GetOID(),
              nexp, roll, head_type);
        double pos = 0.0;
        int idx = (*oid_position_map_)[head->GetOID()];
        for (auto nldx = neighbors_[idx].begin(); nldx != neighbors_[idx].end(); ++nldx) {
          auto part2 = (*simples_)[nldx->idx_];
          if (part2->GetSID() != sid2_) continue;
          pos += binding_affinity * nldx->kmc_;
          if (pos > roll) {
            if (debug_trace)
              printf("[%d,%d] Attaching to [%d,%d]\n", idx, head->GetOID(), nldx->idx_, part2->GetOID());

            // Here, we do more complicated stuff.  Calculate the coordinate along
            // the rod s.t. the line vector is perpendicular to the separation vec
            // (closest point along carrier line).  In this frame, the position
            // of the xlink should be gaussian distributed
            double r_x[3];
            double r_rod[3];
            double s_rod[3];
            double u_rod[3];
            std::copy(head->GetRigidPosition(), head->GetRigidPosition()+ndim_, r_x);
            std::copy(part2->GetRigidPosition(), part2->GetRigidPosition()+ndim_, r_rod);
            std::copy(part2->GetRigidScaledPosition(), part2->GetRigidScaledPosition()+ndim_, s_rod);
            std::copy(part2->GetRigidOrientation(), part2->GetRigidOrientation()+ndim_, u_rod);
            double l_rod = part2->GetRigidLength();
            double *s_1 = NULL; // FIXME from robert
            double rcontact[3];
            double dr[3];
            double mu = 0.0;
            min_distance_point_carrier_line(ndim_, nperiodic_,
                                            space_->unit_cell, r_x, s_1,
                                            r_rod, s_rod, u_rod, l_rod,
                                            dr, rcontact, &mu);

            double r_min[3];
            double r_min_mag2 = 0.0;
            for (int i = 0; i < ndim_; ++i) {
              r_min[i] = -mu * u_rod[i] - dr[i];
              r_min_mag2 += SQR(r_min[i]);
            }
            mrcut2_ = rcutoff_0_1_*rcutoff_0_1_;
            double a = sqrt(mrcut2_ - r_min_mag2);
            //double a = sqrt(1.0 - r_min_mag2); //FIXME is this right for 1.0? or mrcut2?
            if (isnan(a))
              a = 0.0;

            double crosspos = 0.0;
            for (int i = 0; i < 100; ++i) {
              double uroll = gsl_rng_uniform(mrng->r);
              crosspos = (uroll - 0.5)*a + mu + 0.5*l_rod;

              if (crosspos >= 0 && crosspos <= l_rod)
                break;
              if (i == 99) {
                crosspos = -mu + 0.5*l_rod;
                if (crosspos < 0)
                  crosspos = 0;
                else if (crosspos > l_rod)
                  crosspos = l_rod;
              }
            }
            head->Attach(nldx->idx_, crosspos);
            if (debug_trace)
              printf("\t{mu: %2.4f}, {crosspos: %2.4f}\n", mu, crosspos);
            head->SetBound(true);
            (*xit)->CheckBoundState();
            break;
          } // the actual neighbor to fall on
        } // loop over neighbors to figure out which to attach to
      } // successfully bound
    } // found an xlink with some expectation of binding
  } // loop over all xlinks
}

void XlinkKMC::KMC_1_0() {
  XlinkSpecies* pxspec = dynamic_cast<XlinkSpecies*>(spec1_);
  auto xlinks = pxspec->GetXlinks();
  int nbound1[2];
  std::copy(pxspec->GetNBound1(), pxspec->GetNBound1()+2, nbound1);
  double poff_single_a = on_rate_0_1_[0] * alpha_ * pxspec->GetDelta();
  double poff_single_b = on_rate_0_1_[1] * alpha_ * pxspec->GetDelta();

  int noff[2] = {(int)gsl_ran_binomial(rng_.r, poff_single_a, nbound1[0]),
                 (int)gsl_ran_binomial(rng_.r, poff_single_b, nbound1[1])};
  if (debug_trace)
    printf("[Xlink] {poff_single: (%2.8f, %2.8f)}, {noff: (%d, %d)}\n",
        poff_single_a, poff_single_b, noff[0], noff[1]);

  for (int i = 0; i < (noff[0] + noff[1]); ++i) {
    int head_type = i < noff[1];
    int idxloc = -1;
    int idxoff = gsl_rng_uniform_int(rng_.r, nbound1[head_type]);
    bool foundidx = false;

    // Find the one to remove
    for (auto xit = xlinks->begin(); xit != xlinks->end(); ++xit) {
      if ((*xit)->GetBoundState() != singly) continue;
      auto heads = (*xit)->GetHeads();
      // Check to increment only in the case we have the correctly
      // bound head
      if (!(*heads)[head_type].GetBound()) continue;
      idxloc++;
      if (idxloc == idxoff) {
        auto head0 = heads->begin();
        auto head1 = heads->begin()+1;

        // Figure out which head attached
        // Do some fancy aliasing to make this easier
        XlinkHead *attachedhead;
        XlinkHead *nonattachead;
        if (head_type == 0) {
          attachedhead = &(*head0);
          nonattachead = &(*head1);
        } else {
          attachedhead = &(*head1);
          nonattachead = &(*head0);
        }

        if (debug_trace)
          printf("[x:%d,head:%d] Successful KMC move {1 -> 0}, {idxoff=idxloc=%d}, {head: %d}\n",
              (*xit)->GetOID(), attachedhead->GetOID(), idxloc, head_type);
        // Place withint some random distance of the attach point
        double randr[3];
        double mag2 = 0.0;
        mrcut2_ = rcutoff_0_1_*rcutoff_0_1_;
        auto mrng = attachedhead->GetRNG();
        double prevpos[3];
        std::copy(attachedhead->GetRigidPosition(), attachedhead->GetRigidPosition()+ndim_, prevpos);
        do {
          mag2 = 0.0;
          for (int i = 0; i < ndim_; ++i) {
            double mrand = gsl_rng_uniform(mrng->r);
            randr[i] = 2*rcutoff_0_1_*(mrand - 0.5);
            mag2 += SQR(randr[i]);
          }
        } while(mag2 > mrcut2_);
        // Randomly set position based on randr
        for (int i = 0; i < ndim_; ++i) {
          randr[i] = randr[i] + prevpos[i];
        }
        attachedhead->SetPosition(randr);
        attachedhead->SetPrevPosition(prevpos);
        nonattachead->SetPosition(randr);
        nonattachead->SetPrevPosition(prevpos);
        attachedhead->SetBound(false);
        (*xit)->CheckBoundState();

        if (debug_trace) {
          auto attachid = attachedhead->GetAttach();
          auto idx = (*oid_position_map_)[attachedhead->GetOID()];
          auto part2 = (*simples_)[attachid.first];
          printf("[%d,%d] Detached from [%d,%d] (%2.8f, %2.8f) -> (%2.8f, %2.8f)\n",
             idx, attachedhead->GetOID(), attachid.first, part2->GetOID(),
             prevpos[0], prevpos[1],
             attachedhead->GetRigidPosition()[0], attachedhead->GetRigidPosition()[1]);
        }


        attachedhead->Attach(-1, 0.0);
        foundidx = true;
        break;

      } // found it!
    } // find the one to remove

  } // How many to remove
}

void XlinkKMC::KMC_1_2() {
  //printf("XlinkKMC::KMC_1_2 begin\n");
  XlinkSpecies *pxspec = dynamic_cast<XlinkSpecies*>(spec1_);
  auto xlinks = pxspec->GetXlinks();
  double nexp_1_2 = pxspec->GetNExp_1_2();
  int nattach = gsl_ran_poisson(rng_.r, nexp_1_2 * pxspec->GetDelta());
  for (int itrial = 0; itrial < nattach; ++itrial) {
    double ran_loc = gsl_rng_uniform(rng_.r) * nexp_1_2;
    double loc = 0.0;
    for (auto xit = xlinks->begin(); xit != xlinks->end(); ++xit) {
      // Only take singly bound
      if ((*xit)->GetBoundState() != singly) continue;
      loc += (*xit)->GetNExp_1_2();
      if (loc > ran_loc) {
        if (debug_trace)
          printf("[%d] Successful KMC move {1 -> 2}, {nexp_1_2: %2.4f}, {ran_loc: %2.4f}, {loc: %2.4f}\n", (*xit)->GetOID(),
              nexp_1_2, ran_loc, loc);
        // reset loc back to beginning of this xlink
        loc -= (*xit)->GetNExp_1_2();

        // Look at my neighbors and figure out which to fall on
        // Also, get the heads
        auto heads = (*xit)->GetHeads();
        auto head0 = heads->begin();
        auto head1 = heads->begin()+1;

        // Figure out which head attached
        // Do some fancy aliasing to make this easier
        XlinkHead *attachedhead;
        XlinkHead *nonattachead;
        if (head0->GetBound()) {
          attachedhead = &(*head0);
          nonattachead = &(*head1);
        } else {
          attachedhead = &(*head1);
          nonattachead = &(*head0);
        }
        int idx = (*oid_position_map_)[nonattachead->GetOID()];

        // Loop over my neighbors to figure out which to fall on
        for (auto nldx = neighbors_[idx].begin(); nldx != neighbors_[idx].end(); ++nldx) {
          auto mrod = (*simples_)[nldx->idx_];
          // Make sure it's the right species to attach to
          if (mrod->GetSID() != sid2_) continue;

          // Ignore our own rod
          //auto free_idx = (*oid_position_map_)[nonattachead->GetOID()];
          auto free_idx = idx;
          auto attc_idx = (*oid_position_map_)[attachedhead->GetOID()];
          // Get the attached rod
          auto attach_info = attachedhead->GetAttach();
          auto mrod_attached = (*simples_)[attach_info.first];
          if (mrod->GetRID() == mrod_attached->GetRID()) {
            //printf("\tExcluding self attachment\n");
            continue;
          }

          loc += nldx->kmc_;
          if (loc > ran_loc) {
            // Found the neighbor to fall onto!!!!
            if (debug_trace) {
              printf("[%d,%d] Attaching to [%d,%d] {loc: %2.4f}\n", idx, nonattachead->GetOID(), nldx->idx_, mrod->GetOID(), loc);
            }

            // What location (crosspos) do we fall onto?
            double crosspos = 0.0;

            // Here, we do more complicated stuff.  Calculate the coordinate along
            // the rod s.t. the line vector is perpendicular to the separation vec
            // (closest point along carrier line).  In this frame, the position
            // of the xlink should be gaussian distributed
            double r_x[3];
            double r_rod[3];
            double s_rod[3];
            double u_rod[3];
            std::copy(nonattachead->GetRigidPosition(), nonattachead->GetRigidPosition()+ndim_, r_x);
            std::copy(mrod->GetRigidPosition(), mrod->GetRigidPosition()+ndim_, r_rod);
            std::copy(mrod->GetRigidScaledPosition(), mrod->GetRigidScaledPosition()+ndim_, s_rod);
            std::copy(mrod->GetRigidOrientation(), mrod->GetRigidOrientation()+ndim_, u_rod);
            double l_rod = mrod->GetRigidLength();
            double *s_1 = NULL; // FIXME from robert
            double rcontact[3];
            double dr[3];
            double mu = 0.0;
            min_distance_point_carrier_line(ndim_, nperiodic_,
                                            space_->unit_cell, r_x, s_1,
                                            r_rod, s_rod, u_rod, l_rod,
                                            dr, rcontact, &mu);

            // Find it
            auto mrng = (*xit)->GetRNG();
            double y02 = 0.0;
            for (int i = 0; i < ndim_; ++i) {
              y02 += SQR(dr[i]);
            }
            int itrial_loc = 0;
            do {
              itrial_loc++;
              double uroll = gsl_rng_uniform(mrng->r);
              double xvec[2] = {0.0, sqrt(y02)};
              double mpos = ((gsl_rng_uniform(mrng->r) < 0.5) ? -1.0 : 1.0) *
                  n_exp_lookup_.Invert(0, uroll, xvec) + mu + 0.5 * l_rod;
              if (mpos >= 0 && mpos <= l_rod) {
                crosspos = mpos;
                break;
              }
            } while (itrial_loc < 100);

            nonattachead->Attach(nldx->idx_, crosspos);
            nonattachead->SetBound(true);
            (*xit)->CheckBoundState();

            break;
          } // got the neighbor to fall onto
        }

      } // found it!
    }
  }
}

void XlinkKMC::KMC_2_1() {

}

void XlinkKMC::UpdateKMC() {
  simples_ = tracking_->GetSimples();
  nsimples_ = tracking_->GetNSimples();
  oid_position_map_ = tracking_->GetOIDPositionMap();
  neighbors_ = tracking_->GetNeighbors();

  nfree_ = 0.0;
  nbound1_[0] = nbound1_[1] = 0.0;
  nbound2_ = 0.0;

  // Do a switch on the type that we're examining
  // Loop over the xlinks to see who  does what
  XlinkSpecies* pxspec = dynamic_cast<XlinkSpecies*>(spec1_);
  auto xlinks = pxspec->GetXlinks();
  for (auto xit = xlinks->begin(); xit != xlinks->end(); ++xit) {
    switch((*xit)->GetBoundState()) {
      case unbound:
        nfree_++;
        break;
      case singly:
        UpdateStage1(*xit);
        break;
      case doubly:
        UpdateStage2(*xit);
        break;
    }
  }

  pxspec->SetNFree(nfree_);
  pxspec->SetNBound1(nbound1_[0], nbound1_[1]);
  pxspec->SetNBound2(nbound2_);
}

void XlinkKMC::UpdateStage1(Xlink *xit) {

  // Set nexp to zero for all involved
  xit->SetNExp_0_1(0.0);
  auto heads = xit->GetHeads();
  auto head0 = heads->begin();
  auto head1 = heads->begin()+1;
  head0->SetNExp_0_1(0.0);
  head1->SetNExp_0_1(0.0);
  
  // Figure out which head attached
  // Do some fancy aliasing to make this easier
  XlinkHead *attachedhead;
  XlinkHead *nonattachead;
  if (head0->GetBound()) {
    attachedhead = &(*head0);
    nonattachead = &(*head1);
    nbound1_[0]++;
  } else if (head1->GetBound()) {
    attachedhead = &(*head1);
    nonattachead = &(*head0);
    nbound1_[1]++;
  } else {
    printf("Something has gone horribly wrong\n");
    exit(1);
  }
  auto aidx = attachedhead->GetAttach().first;
  auto cross_pos = attachedhead->GetAttach().second; // relative to the -end of the rod!!
  auto r_x = attachedhead->GetRigidPosition();
  
  auto part2 = (*simples_)[aidx];
  auto r_rod = part2->GetRigidPosition();
  auto u_rod = part2->GetRigidOrientation();
  auto l_rod = part2->GetRigidLength();

  // If we are moving with some velocity, do that
  // XXX FIXME check for end pausing
  cross_pos += velocity_ * attachedhead->GetDelta();
  if (cross_pos > l_rod) {
    cross_pos = l_rod;
  } else if (cross_pos < 0.0) {
    cross_pos = 0.0;
  }
  attachedhead->Attach(aidx, cross_pos);

  double rxnew[3];
  for (int i = 0; i < ndim_; ++i) {
    rxnew[i] = r_rod[i] - 0.5 * u_rod[i] * l_rod + cross_pos * u_rod[i];
  }
  auto idx = (*oid_position_map_)[attachedhead->GetOID()];
  if (debug_trace)
    printf("[%d,%d] attached [%d,%d], (%2.4f, %2.4f) -> setting -> (%2.4f, %2.4f)\n",
           idx, attachedhead->GetOID(), aidx, part2->GetOID(), r_x[0], r_x[1],
           rxnew[0], rxnew[1]);
  attachedhead->SetPrevPosition(r_x);
  attachedhead->SetPosition(rxnew);
  nonattachead->SetPrevPosition(r_x);
  nonattachead->SetPosition(rxnew);
  xit->SetPrevPosition(r_x);
  xit->SetPosition(rxnew);
}

void XlinkKMC::UpdateStage2(Xlink *xit) {
  // Set nexp to zero for all involved
  xit->SetNExp_1_2(0.0);
  double oldxitpos[3];
  std::copy(xit->GetPosition(), xit->GetPosition()+ndim_, oldxitpos);
  double avgpos[3] = {0.0, 0.0, 0.0};
  auto heads = xit->GetHeads();
  for (auto head = heads->begin(); head != heads->end(); ++head) {
    head->SetNExp_1_2(0.0);

    auto aidx = head->GetAttach().first;
    auto crosspos = head->GetAttach().second;
    auto rx = head->GetRigidPosition();

    auto mrod = (*simples_)[aidx];
    auto rrod = mrod->GetRigidPosition();
    auto urod = mrod->GetRigidOrientation();
    auto lrod = mrod->GetRigidLength();

    // If we are moving at some velocity, do it
    // XXX Check for end pausing
    crosspos += velocity_ * head->GetDelta();
    if (crosspos > lrod) {
      crosspos = lrod;
    } else if (crosspos < 0.0) {
      crosspos = 0.0;
    }
    head->Attach(aidx, crosspos);

    double rxnew[3];
    for (int i = 0; i < ndim_; ++i) {
      rxnew[i] = rrod[i] - 0.5 * urod[i] * lrod + crosspos * urod[i];
      avgpos[i] += rxnew[i];
    }
    auto idx = (*oid_position_map_)[head->GetOID()];
    if (debug_trace) {
      printf("[%d,%d] attached [%d,%d], (%2.4f, %2.4f) -> setting -> (%2.4f, %2.4f)\n",
          idx, head->GetOID(), aidx, mrod->GetOID(), rx[0], rx[1],
          rxnew[0], rxnew[1]);
    }
    head->SetPrevPosition(rx);
    head->SetPosition(rxnew);
  }

  for (int i = 0; i < ndim_; ++i) {
    avgpos[i] *= 0.5;
  }

  xit->SetPosition(avgpos);
  xit->SetPrevPosition(oldxitpos);

  nbound2_++;
}

void XlinkKMC::Dump() {
  // print out the information appropriate to kmc
  if (debug_trace) {
    XlinkSpecies *pxspec = dynamic_cast<XlinkSpecies*>(spec1_);
    printf("XlinkKMC -> dump\n");
    printf("\t{n_exp_0_1: %2.4f, n_exp_1_2: %2.4f}\n", pxspec->GetNExp_0_1(), pxspec->GetNExp_1_2());
    printf("\t{nfree:  %d}\n", pxspec->GetNFree());
    printf("\t{nbound1: %d,%d}\n", pxspec->GetNBound1()[0], pxspec->GetNBound1()[1]);
    printf("\t{nbound2: %d}\n", pxspec->GetNBound2());
    pxspec->DumpKMC();
  }
}

void XlinkKMC::PrepOutputs() {
  std::ostringstream file_name;
  file_name << "sc-kmc-XlinkKMC.log";
  kmc_file.open(file_name.str().c_str(), std::ios_base::out);
  kmc_file << "#ntot #nfree #nbound1[0] #nbound1[1] #nbound2\n";
  kmc_file.close();
}

void XlinkKMC::WriteOutputs() {
  XlinkSpecies *pxspec = dynamic_cast<XlinkSpecies*>(spec1_);
  std::ostringstream file_name;
  file_name << "sc-kmc-XlinkKMC.log";
  kmc_file.open(file_name.str().c_str(), std::ios_base::out | std::ios_base::app);
  kmc_file.precision(16);
  kmc_file.setf(std::ios::fixed, std::ios::floatfield);
  kmc_file << pxspec->GetNMembers() << " " << pxspec->GetNFree() << " " <<
    pxspec->GetNBound1()[0] << " " << pxspec->GetNBound1()[1] << " " << 
    pxspec->GetNBound2() << "\n";
  kmc_file.close();
}
