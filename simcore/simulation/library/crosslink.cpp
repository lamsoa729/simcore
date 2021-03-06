#include "crosslink.hpp"

Crosslink::Crosslink() : Object() { SetSID(species_id::crosslink); }

void Crosslink::Init(MinimumDistance *mindist, LookupTable *lut) {
  mindist_ = mindist;
  lut_ = lut;
  length_ = -1;
  diameter_ = params_->crosslink.tether_diameter;
  color_ = params_->crosslink.tether_color;
  draw_ = draw_type::_from_string(params_->crosslink.tether_draw_type.c_str());
  rest_length_ = params_->crosslink.rest_length;
  k_on_ = params_->crosslink.k_on;       // k_on for unbound to singly
  k_off_ = params_->crosslink.k_off;     // k_off for singly to unbound
  k_on_d_ = params_->crosslink.k_on_d;   // k_on for singly to doubly
  k_off_d_ = params_->crosslink.k_off_d; // k_off for doubly to singly
  k_spring_ = params_->crosslink.k_spring;
  k_align_ = params_->crosslink.k_align;
  f_spring_max_ = params_->crosslink.f_spring_max;
  rcapture_ = params_->crosslink.r_capture;
  fdep_factor_ = params_->crosslink.force_dep_factor;
  polar_affinity_ = params_->crosslink.polar_affinity;
  /* TODO generalize crosslinks to more than two anchors */
  Anchor anchor1;
  Anchor anchor2;
  anchors_.push_back(anchor1);
  anchors_.push_back(anchor2);
  anchors_[0].Init();
  anchors_[1].Init();
  SetSID(species_id::crosslink);
  SetSingly();
  Logger::Trace("Initializing crosslink %d with anchors %d and %d", GetOID(),
      anchors_[0].GetOID(), anchors_[1].GetOID());
}

/* Function used to set anchor[0] position etc to xlink position etc */
void Crosslink::UpdatePosition() {}

void Crosslink::GetAnchors(std::vector<Object *> &ixors) {
  if (IsUnbound())
    return;
  ixors.push_back(&anchors_[0]);
  if (IsDoubly()) {
    ixors.push_back(&anchors_[1]);
  }
}

/* Perform kinetic monte carlo step of protein with 1 head attached. */
void Crosslink::SinglyKMC() {
  double roll = gsl_rng_uniform_pos(rng_.r);
  int head_bound = 0;
  // Set up KMC objects and calculate probabilities
  double unbind_prob = k_off_ * delta_;
  /* Must populate filter with 1 for every neighbor, since KMC
  expects a mask. We already guarantee uniqueness, so we won't overcount. */
  int n_neighbors = anchors_[0].GetNNeighbors();
  std::vector<int> kmc_filter(n_neighbors, 1);
  /* Initialize KMC calculation */
  KMC<Object> kmc_bind(anchors_[0].pos, n_neighbors, rcapture_, delta_, lut_);

  /* Initialize periodic boundary conditions */
  kmc_bind.SetPBCs(n_dim_, space_->n_periodic, space_->unit_cell);

  /* Calculate probability to bind */
  double kmc_bind_prob = 0;
  std::vector<double> kmc_bind_factor(n_neighbors, k_on_d_);
  if (n_neighbors > 0) {
    kmc_bind.CalcTotProbsSD(anchors_[0].GetNeighborListMem(), kmc_filter,
                            anchors_[0].GetBoundOID(), 0, k_spring_, 1.0,
                            rest_length_, kmc_bind_factor);
    kmc_bind_prob = kmc_bind.getTotProb();
  }
  // Find out whether we bind, unbind, or neither.
  int head_activate = choose_kmc_double(unbind_prob, kmc_bind_prob, roll);
  // Change status of activated head
  if (head_activate == 0) {
    // Unbind bound head
    anchors_[0].Unbind();
    SetUnbound();
    Logger::Trace("Crosslink %d came unbound", GetOID());
  } else if (head_activate == 1) {
    // Bind unbound head
    /* Position on rod where protein will bind with respect to center of rod,
     * passed by reference */
    double bind_lambda;
    /* Find out which rod we are binding to */
    int i_bind = kmc_bind.whichRodBindSD(bind_lambda, roll);
    if (i_bind < 0) { // || bind_lambda < 0) {
      printf("i_bind = %d\nbind_lambda = %2.2f\n", i_bind, bind_lambda);
      Logger::Error("kmc_bind.whichRodBindSD in Crosslink::SinglyKMC"
                 " returned an invalid result!");
    }
    Object *bind_obj = anchors_[0].GetNeighbor(i_bind);
    double obj_length = bind_obj->GetLength();
    /* KMC returns bind_lambda to be with respect to center of rod. We want it
       to be specified from the tail of the rod to be consistent */
    bind_lambda += 0.5 * obj_length;
    /* KMC can return values that deviate a very small amount from the true rod
       length. Bind to ends if lambda < 0 or lambda > bond_length. */
    if (bind_lambda > obj_length) {
      bind_lambda = obj_length;
    } else if (bind_lambda < 0) {
      bind_lambda = 0;
    }
    anchors_[1].AttachObjLambda(bind_obj, bind_lambda);
    SetDoubly();
    Logger::Trace("Crosslink %d became doubly bound to obj %d", GetOID(),
        bind_obj->GetOID());
  }
  kmc_filter.clear();
}

/* Perform kinetic monte carlo step of protein with 2 heads of protein
 * object attached. */
void Crosslink::DoublyKMC() {
  /* Calculate force-dependent unbinding for each head */
  double tether_stretch = length_ - rest_length_;
  tether_stretch = (tether_stretch > 0 ? tether_stretch : 0);
  double fdep = fdep_factor_ * 0.5 * k_spring_ * SQR(tether_stretch);
  double unbind_prob = k_off_d_ * delta_ * exp(fdep);
  double roll = gsl_rng_uniform_pos(rng_.r);
  // Each head has an equal likelihood to unbind (half the total probability)
  int head_activate =
      choose_kmc_double(0.5 * unbind_prob, 0.5 * unbind_prob, roll);
  if (head_activate == 0) {
    Logger::Trace("Doubly-bound crosslink %d came unbound from %d", GetOID(),
        anchors_[0].GetBoundOID());
    anchors_[0] = anchors_[1];
    anchors_[1].Unbind();
    SetSingly();
  } else if (head_activate == 1) {
    Logger::Trace("Doubly-bound crosslink %d came unbound from %d", GetOID(),
        anchors_[1].GetBoundOID());
    anchors_[1].Unbind();
    SetSingly();
  }
}

void Crosslink::CalculateBinding() {
  if (IsSingly()) {
    SinglyKMC();
  } else if (IsDoubly()) {
    DoublyKMC();
  }
  ClearNeighbors();
}

/* Only singly-bound crosslinks interact */
void Crosslink::GetInteractors(std::vector<Object *> &ixors) {
  ClearNeighbors();
  if (IsSingly()) {
    ixors.push_back(&anchors_[0]);
  }
}

void Crosslink::ClearNeighbors() { anchors_[0].ClearNeighbors(); }

void Crosslink::UpdateAnchorsToMesh() {
  anchors_[0].UpdateAnchorPositionToMesh();
  anchors_[1].UpdateAnchorPositionToMesh();
}

void Crosslink::UpdateAnchorPositions() {
  anchors_[0].UpdatePosition();
  anchors_[1].UpdatePosition();
}

void Crosslink::ApplyTetherForces() {
  if (!IsDoubly())
    return;
  anchors_[0].ApplyAnchorForces();
  anchors_[1].ApplyAnchorForces();
}

void Crosslink::UpdateCrosslinkForces() {
  /* Update anchor positions in space to calculate tether forces */
  UpdateAnchorsToMesh();
  /* Check if an anchor became unbound due to diffusion, etc */
  UpdateXlinkState();
  /* If we are doubly-bound, calculate and apply tether forces */
  CalculateTetherForces();
}

void Crosslink::UpdateCrosslinkPositions() {
  /* Have anchors diffuse/walk along mesh */
  UpdateAnchorPositions();
  /* Check if an anchor became unbound do to diffusion, etc */
  UpdateXlinkState();
  /* Check for binding/unbinding events using KMC */
  CalculateBinding();
}

/* This function ensures that singly-bound crosslinks have anchor[0] bound and
   anchor[1] unbound. */
void Crosslink::UpdateXlinkState() {
  if (!anchors_[0].IsBound() && !anchors_[1].IsBound()) {
    SetUnbound();
    return;
  }
  if (IsDoubly() && !anchors_[1].IsBound()) {
    SetSingly();
  } else if (IsDoubly() && !anchors_[0].IsBound()) {
    anchors_[0] = anchors_[1];
    SetSingly();
  }
  if (IsSingly() && anchors_[1].IsBound()) {
    SetDoubly();
  }
}

void Crosslink::ZeroForce() {
  std::fill(force_, force_ + 3, 0.0);
  anchors_[0].ZeroForce();
  anchors_[1].ZeroForce();
}

void Crosslink::CalculateTetherForces() {
  ZeroForce();
  if (!IsDoubly())
    return;
  Interaction ix(&anchors_[0], &anchors_[1]);
  mindist_->ObjectObject(ix);
  /* Check stretch of tether. No penalty for having a stretch < rest_length. ie
   * the spring does not resist compression. */
  length_ = sqrt(ix.dr_mag2);
  double stretch = length_ - rest_length_;
  // We also update the tether's position etc, stored in the xlink position etc
  for (int i = 0; i < params_->n_dim; ++i) {
    orientation_[i] = ix.dr[i] / length_;
    position_[i] = ix.midpoint[i];
  }
  if (stretch > 0) {
    tether_force_ = k_spring_ * stretch;
    for (int i = 0; i < params_->n_dim; ++i) {
      force_[i] = tether_force_ * orientation_[i];
    }
    anchors_[0].AddForce(force_);
    anchors_[1].SubForce(force_);
  }
  // Update xlink's position (for drawing)
  UpdatePeriodic();
}

/* Attach a crosslink anchor to object in a random fashion. Currently only
 * bonds are used, but can be extended to sites (sphere-like objects) */
void Crosslink::AttachObjRandom(Object *obj) {
  /* Attaching to random bond implies first anchor binding from solution, so
   * this crosslink should be new and should not be singly or doubly bound */
  if (obj->GetType() == +obj_type::bond) {
    anchors_[0].AttachObjRandom(obj);
    SetMeshID(obj->GetMeshID());
  } else {
    /* TODO: add binding to sphere or site-like objects */
    Logger::Error("Crosslink binding to non-bond objects not yet implemented.");
  }
}

void Crosslink::Draw(std::vector<graph_struct *> *graph_array) {
  /* Draw anchors */
  anchors_[0].Draw(graph_array);
  anchors_[1].Draw(graph_array);
  /* Draw tether */
  if (IsDoubly() && length_ > 0) {
    std::copy(scaled_position_, scaled_position_ + 3, g_.r);
    for (int i = space_->n_periodic; i < n_dim_; ++i) {
      g_.r[i] = position_[i];
    }
    // std::copy(position_, position_+3, g_.r);
    std::copy(orientation_, orientation_ + 3, g_.u);
    g_.color = color_;
    if (params_->graph_diameter > 0) {
      g_.diameter = params_->graph_diameter;
    } else {
      g_.diameter = diameter_;
    }
    g_.length = length_;
    g_.draw = draw_;
    graph_array->push_back(&g_);
  }
}

void Crosslink::SetDoubly() { state_ = bind_state::doubly; }

void Crosslink::SetSingly() { state_ = bind_state::singly; }

void Crosslink::SetUnbound() { state_ = bind_state::unbound; }

bool Crosslink::IsDoubly() { return state_ == +bind_state::doubly; }

bool Crosslink::IsSingly() { return state_ == +bind_state::singly; }

bool Crosslink::IsUnbound() { return state_ == +bind_state::unbound; }

void Crosslink::WriteSpec(std::fstream &ospec) {
  if (IsUnbound()) {
    Logger::Error("Unbound crosslink tried to WriteSpec!");
  }
  bool is_doubly = IsDoubly();
  ospec.write(reinterpret_cast<char *>(&is_doubly), sizeof(bool));
  ospec.write(reinterpret_cast<char *>(&diameter_), sizeof(double));
  ospec.write(reinterpret_cast<char *>(&length_), sizeof(double));
  for (int i = 0; i < 3; ++i) {
    ospec.write(reinterpret_cast<char *>(&position_[i]), sizeof(double));
  }
  for (int i = 0; i < 3; ++i) {
    ospec.write(reinterpret_cast<char *>(&orientation_[i]), sizeof(double));
  }
  anchors_[0].WriteSpec(ospec);
  anchors_[1].WriteSpec(ospec);
}

void Crosslink::ReadSpec(std::fstream &ispec) {
  if (ispec.eof())
    return;
  SetSingly();
  bool is_doubly;
  ispec.read(reinterpret_cast<char *>(&is_doubly), sizeof(bool));
  ispec.read(reinterpret_cast<char *>(&diameter_), sizeof(double));
  ispec.read(reinterpret_cast<char *>(&length_), sizeof(double));
  for (int i = 0; i < 3; ++i) {
    ispec.read(reinterpret_cast<char *>(&position_[i]), sizeof(double));
  }
  for (int i = 0; i < 3; ++i) {
    ispec.read(reinterpret_cast<char *>(&orientation_[i]), sizeof(double));
  }
  UpdatePeriodic();
  anchors_[0].ReadSpec(ispec);
  anchors_[1].ReadSpec(ispec);
  if (is_doubly) {
    SetDoubly();
  }
}

void Crosslink::WriteCheckpoint(std::fstream &ocheck) {
  void *rng_state = gsl_rng_state(rng_.r);
  size_t rng_size = gsl_rng_size(rng_.r);
  ocheck.write(reinterpret_cast<char *>(&rng_size), sizeof(size_t));
  ocheck.write(reinterpret_cast<char *>(rng_state), rng_size);
  WriteSpec(ocheck);
}

void Crosslink::ReadCheckpoint(std::fstream &icheck) {
  if (icheck.eof())
    return;
  void *rng_state = gsl_rng_state(rng_.r);
  size_t rng_size;
  icheck.read(reinterpret_cast<char *>(&rng_size), sizeof(size_t));
  icheck.read(reinterpret_cast<char *>(rng_state), rng_size);
  ReadSpec(icheck);
  Logger::Trace("Reloading anchor from checkpoint with mid %d", anchors_[0].GetMeshID());
  if (IsDoubly()) {
    Logger::Trace("Reloading anchor from checkpoint with mid %d", anchors_[1].GetMeshID());
  }
}
