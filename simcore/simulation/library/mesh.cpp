#include "mesh.hpp"

/**************************
** Mesh member functions **
**************************/
int Mesh::_next_mesh_id_ = 0;
std::mutex Mesh::_mesh_mtx_;

Mesh::Mesh() : Object() {
  InitMeshID();
  n_sites_ = n_bonds_ = n_bonds_max_ = 0;
  is_mesh_ = true;
  midstep_ = true;
  anchored_ = false;
  posits_only_ = false;
}

void Mesh::InitMeshID() {
  std::lock_guard<std::mutex> lk(_mesh_mtx_);
  SetMeshID(++_next_mesh_id_);
}

void Mesh::Reserve(int n_bonds) {
  sites_.reserve(n_bonds + 1);
  bonds_.reserve(n_bonds);
  n_bonds_max_ = n_bonds;
}

Site *Mesh::GetSite(int i) { return &(sites_[i]); }
Bond *Mesh::GetBond(int i) { return &(bonds_[i]); }
void Mesh::AddSite(Site s) {
  if (n_sites_ == n_bonds_max_ + 1) {
    Logger::Error("Attempting to add site beyond allocated maximum.\n"
                  "n_bonds_max_: %d , n_sites_: %d",
                  n_bonds_max_, n_sites_);
  }
  sites_.push_back(s);
  sites_.back().SetColor(color_, draw_);
  sites_.back().SetMeshID(GetMeshID());
  n_sites_++;
}
void Mesh::AddBond(Bond b) {
  if (n_bonds_ == n_bonds_max_) {
    Logger::Error("Attempting to add bond beyond allocated maximum.\n");
  }
  bonds_.push_back(b);
  bonds_.back().SetColor(color_, draw_);
  bonds_.back().SetMeshID(GetMeshID());
  bonds_.back().SetSID(GetSID());
  bonds_.back().SetMeshPtr(this);
  bonds_.back().SetBondNumber(n_bonds_);
  n_bonds_++;
  /* Anytime we change the number of bonds, which are interactors, we signal
     that interactors must be updated */
  interactor_update_ = true;
}

void Mesh::Clear() {
  bonds_.clear();
  sites_.clear();
  n_bonds_ = n_sites_ = 0;
  interactor_update_ = true;
}

void Mesh::RemoveBondFromTip() {
  if (n_bonds_ == 0)
    return;
  sites_.pop_back();
  n_sites_--;
  sites_.back().RemoveBond(bonds_.back().GetOID());
  bonds_.pop_back();
  n_bonds_--;
  /* Anytime we change the number of bonds, which are interactors, we signal
     that interactors must be updated */
  interactor_update_ = true;
}

// Doubles number of bonds in graph while keeping same shape
// currently only works for linear objects like filaments
void Mesh::DoubleGranularityLinear() {
  Logger::Trace("Mesh %d doubling bonds for dynamic instability, n_bonds: %d ->"
                " %d, bond_length: %2.2f -> %2.2f",
                GetMeshID(), n_bonds_, 2 * n_bonds_, bond_length_,
                0.5 * bond_length_);
  int n_bonds_old = n_bonds_;
  bond_length_ /= 2;
  // First record positions of currently existing graph
  UpdatePrevPositions();
  // Then double the number of bonds in the system, adding random
  for (int i = 0; i < n_bonds_old; ++i) {
    AddRandomBondToTip(bond_length_);
  }
  // Now update site positions based on old graph positions
  int i_site = 1;
  for (int i_bond_old = 0; i_bond_old < n_bonds_old; ++i_bond_old) {
    sites_[i_site++].SetPosition(bonds_[i_bond_old].GetPrevPosition());
    sites_[i_site++].SetPosition(sites_[i_bond_old + 1].GetPrevPosition());
  }
  // Then update bond positions
  UpdateBondPositions();
}

// Halves number of bonds in graph while attempting to keep same shape,
// currently only works for linear objects like filaments with even
// number of bonds
void Mesh::HalfGranularityLinear() {
  if (n_bonds_ % 2 != 0) {
    Logger::Error(
        "HalfGranularityLinear called on mesh with odd number of bonds: %d",
        n_bonds_);
  }
  Logger::Trace("Mesh %d halving bonds for dynamic instability, n_bonds: %d ->"
                " %d, bond_length: %2.2f -> %2.2f",
                GetMeshID(), n_bonds_, n_bonds_ / 2, bond_length_,
                2 * bond_length_);

  int n_bonds_new = n_bonds_ / 2;
  bond_length_ *= 2;
  // First record positions of currently existing graph
  // Note: this is fine, since UpdatePrevPositions is just going to be called
  // again by the filament's Integrate step before prev positions are used
  UpdatePrevPositions();
  // Update first half of current site positions based on old graph positions
  for (int i_bond_new = 0; i_bond_new < n_bonds_new; ++i_bond_new) {
    sites_[i_bond_new + 1].SetPosition(
        sites_[2 * (i_bond_new + 1)].GetPrevPosition());
  }
  // Now prune remaining bonds
  for (int i = 0; i < n_bonds_new; ++i) {
    RemoveBondFromTip();
  }
  // Then update bond positions
  UpdateBondPositions();
}

/* Move COM of mesh to new position and orientation */
void Mesh::RelocateMesh(double *pos, double *u) {
  std::copy(pos, pos + 3, position_);
  std::copy(u, u + 3, orientation_);
  normalize_vector(orientation_, n_dim_);
  for (int i = 0; i < n_dim_; ++i) {
    position_[i] -= 0.5 * length_ * orientation_[i];
  }
  for (int i = 0; i < n_sites_; ++i) {
    sites_[i].SetPosition(position_);
    for (int i = 0; i < n_dim_; ++i) {
      position_[i] += bond_length_ * orientation_[i];
    }
  }
  UpdateBondPositions();
  UpdatePrevPositions();
}

void Mesh::UpdateSiteOrientations() {
  for (int i = 0; i < n_sites_ - 1; ++i) {
    sites_[i].SetOrientation(bonds_[i].GetOrientation());
  }
  sites_[n_sites_ - 1].SetOrientation(bonds_[n_bonds_ - 1].GetOrientation());
}

void Mesh::UpdatePrevPositions() {
  for (auto site = sites_.begin(); site != sites_.end(); ++site) {
    site->SetPrevPosition(site->GetPosition());
  }
  for (auto bond = bonds_.begin(); bond != bonds_.end(); ++bond) {
    bond->SetPrevPosition(bond->GetPosition());
  }
}

void Mesh::InitSiteAt(double *pos, double d) {
  Logger::Trace("Mesh %d inserting site at [%2.2f %2.2f %2.2f]", GetMeshID(),
                pos[0], pos[1], pos[2]);
  Site s;
  s.SetPosition(pos);
  s.SetDiameter(d);
  AddSite(s);
}

void Mesh::SetPosition(double const *const pos) {
  std::fill(position_, position_ + 3, 0.0);
  std::fill(orientation_, orientation_ + 3, 0.0);
  for (auto site_it = sites_.begin(); site_it != sites_.end(); ++site_it) {
    double const *const site_pos = site_it->GetPosition();
    double const *const site_u = site_it->GetOrientation();
    for (int i = 0; i < n_dim_; ++i) {
      position_[i] += site_pos[i];
      orientation_[i] += site_u[i];
    }
  }
  normalize_vector(orientation_, n_dim_);
  for (int i = 0; i < n_dim_; ++i) {
    position_[i] /= n_sites_;
  }
  double dr[3] = {0, 0, 0};
  for (int i = 0; i < n_dim_; ++i) {
    dr[i] = pos[i] - position_[i];
  }
  for (auto site_it = sites_.begin(); site_it != sites_.end(); ++site_it) {
    double new_pos[3] = {0, 0, 0};
    double const *const site_pos = site_it->GetPosition();
    for (int i = 0; i < n_dim_; ++i) {
      new_pos[i] = site_pos[i] + dr[i];
    }
    site_it->SetPosition(new_pos);
  }
  UpdateBondPositions();
}

void Mesh::InitBondAt(double *pos, double *u, double l, double d) {
  Site s1;
  Site s2;
  s1.SetDiameter(d);
  s2.SetDiameter(d);
  for (int i = 0; i < n_dim_; ++i) {
    pos[i] -= 0.5 * l * u[i];
  }
  s1.SetPosition(pos);
  for (int i = 0; i < n_dim_; ++i) {
    pos[i] += l * u[i];
  }
  s2.SetPosition(pos);
  AddSite(s1);
  AddSite(s2);
  Bond b;
  AddBond(b);
  bonds_[n_bonds_ - 1].Init(&sites_[n_sites_ - 2], &sites_[n_sites_ - 1]);
}
void Mesh::InitRandomSite(double d) {
  InsertRandom();
  // double pos[3];
  // get_random_coordinate(pos,params_->n_dim,params_->system_radius,params_->boundary,rng_.r);
  InitSiteAt(position_, d);
}
// Default d=1
void Mesh::AddRandomBondAnywhere(double l, double d) {
  if (n_sites_ < 1) {
    InitRandomSite(d);
  }
  int i_site = gsl_rng_uniform_int(rng_.r, n_sites_);
  AddRandomBondToSite(l, i_site);
}
void Mesh::AddRandomBondToSite(double l, int i_site) {
  if (i_site > n_sites_ || i_site < 0) {
    Logger::Error("Site index out of range in AddRandomBondToSite!\n");
  }
  double const d = sites_[i_site].GetDiameter();
  double const *const pos0 = sites_[i_site].GetPosition();
  double pos[3] = {0, 0, 0};
  generate_random_unit_vector(n_dim_, pos, rng_.r);
  for (int i = 0; i < n_dim_; ++i) {
    pos[i] = pos0[i] + l * pos[i];
  }
  InitSiteAt(pos, d);
  Bond b;
  AddBond(b);
  bonds_.back().Init(&sites_[i_site], &sites_[n_sites_ - 1]);
}

void Mesh::AddRandomBondToTip(double l) {
  AddRandomBondToSite(l, n_sites_ - 1);
}

void Mesh::AddBondToTip(double *u, double l) {
  AddBondToSite(u, l, n_sites_ - 1);
}

void Mesh::AddBondToSite(double *u, double l, int i_site) {
  if (i_site > n_sites_ || i_site < 0) {
    Logger::Error("Site index out of range in AddBondToSite!\n");
  }
  double const d = sites_[i_site].GetDiameter();
  double const *const pos0 = sites_[i_site].GetPosition();
  double pos[3];
  for (int i = 0; i < n_dim_; ++i) {
    pos[i] = pos0[i] + l * u[i];
  }
  InitSiteAt(pos, d);
  AddBondBetweenSites(&sites_[i_site], &sites_[n_sites_ - 1]);
}

void Mesh::AddBondBetweenSites(Site *site1, Site *site2) {
  Bond b;
  AddBond(b);
  bonds_[n_bonds_ - 1].Init(site1, site2);
}
void Mesh::UpdateBondPositions() {
  int i = 0;
  for (bond_iterator it = bonds_.begin(); it != bonds_.end(); ++it) {
    it->ReInit();
    it->SetBondNumber(i++);
  }
  /* This always needs to get called afterwards to remain consistent with bonds
   */
  UpdateSiteOrientations();
}
void Mesh::ReportSites() {
  for (site_iterator it = sites_.begin(); it != sites_.end(); ++it) {
    it->Report();
    std::cerr << "      mem: " << &(*it) << "\n";
  }
}
void Mesh::ReportBonds() {
  for (bond_iterator it = bonds_.begin(); it != bonds_.end(); ++it) {
    it->Report();
    std::cerr << "      mem: " << &(*it) << "\n";
  }
}
void Mesh::Report() {
  fprintf(stderr, "Mesh: \n");
  fprintf(stderr, "  OID: %d\n", GetOID());
  fprintf(stderr, "  n_sites: %d\n", n_sites_);
  fprintf(stderr, "  n_bonds: %d\n", n_bonds_);
  ReportSites();
  ReportBonds();
}
void Mesh::SubReport() {
  fprintf(stderr, "Mesh SubReport: \n");
  for (bond_iterator it = bonds_.begin(); it != bonds_.end(); ++it) {
    it->ReportSites();
  }
  for (site_iterator it = sites_.begin(); it != sites_.end(); ++it) {
    it->ReportBonds();
  }
}
void Mesh::SetBondLength(double l) { bond_length_ = l; }
void Mesh::Draw(std::vector<graph_struct *> *graph_array) {
  for (bond_iterator it = bonds_.begin(); it != bonds_.end(); ++it) {
    it->Draw(graph_array);
  }
}

void Mesh::ZeroForce() {
  std::fill(force_, force_ + 3, 0.0);
  std::fill(torque_, torque_ + 3, 0.0);
  p_energy_ = 0;
  for (auto it = sites_.begin(); it != sites_.end(); ++it) {
    it->ZeroForce();
  }
  for (auto it = bonds_.begin(); it != bonds_.end(); ++it) {
    it->ZeroForce();
  }
}

void Mesh::UpdateInteractors() {
  interactors_.clear();
  for (auto it = bonds_.begin(); it != bonds_.end(); ++it) {
    interactors_.push_back(&(*it));
  }
}

const bool Mesh::CheckInteractorUpdate() {
  return Object::CheckInteractorUpdate();
}

void Mesh::GetInteractors(std::vector<Object *> *ix) {
  UpdateInteractors();
  ix->insert(ix->end(), interactors_.begin(), interactors_.end());
}

int Mesh::GetCount() { return n_bonds_; }

void Mesh::ReadPosit(std::fstream &ip) {
  int size;
  Site s;
  Bond b;
  ip.read(reinterpret_cast<char *>(&size), sizeof(size));
  sites_.resize(size, s);
  ip.read(reinterpret_cast<char *>(&size), sizeof(size));
  bonds_.resize(size, b);
  for (auto &s : sites_)
    s.ReadPosit(ip);
  for (auto &b : bonds_)
    b.ReadPosit(ip);
}

void Mesh::WritePosit(std::fstream &op) {
  int size;
  size = sites_.size();
  op.write(reinterpret_cast<char *>(&size), sizeof(size));
  size = bonds_.size();
  op.write(reinterpret_cast<char *>(&size), sizeof(size));
  for (auto &s : sites_)
    s.WritePosit(op);
  for (auto &b : bonds_)
    b.WritePosit(op);
}

void Mesh::ReadSpec(std::fstream &ip) {
  int nsites = 0;
  int mid = 0;
  ip.read(reinterpret_cast<char *>(&mid), sizeof(mid));
  SetMeshID(mid);
  ip.read(reinterpret_cast<char *>(&diameter_), sizeof(diameter_));
  ip.read(reinterpret_cast<char *>(&length_), sizeof(length_));
  ip.read(reinterpret_cast<char *>(&bond_length_), sizeof(bond_length_));
  ip.read(reinterpret_cast<char *>(&nsites), sizeof(int));
  if (nsites == n_sites_) {
    for (auto it = sites_.begin(); it != sites_.end(); ++it) {
      it->ReadSpec(ip);
    }
    for (auto it = bonds_.begin(); it != bonds_.end(); ++it) {
      it->ReInit();
    }
  } else {
    Clear();
    for (int i = 0; i < nsites; ++i) {
      for (int j = 0; j < 3; ++j) {
        ip.read(reinterpret_cast<char *>(&position_[j]), sizeof(double));
      }
      InitSiteAt(position_, diameter_);
    }
    if (n_sites_ != nsites || n_sites_ < 2) {
      Logger::Error("Improper number of site positions read in"
                    " Mesh::ReadSpec");
    }
    for (int i = 0; i < n_sites_ - 1; ++i) {
      AddBondBetweenSites(&sites_[i], &sites_[i + 1]);
      bonds_[n_bonds_ - 1].SetEquilLength(bond_length_);
    }
  }
  if (n_bonds_ != n_sites_ - 1) {
    Logger::Error("Incorrect number of bonds initialized in Mesh::ReadSpec");
  }
}

void Mesh::WriteSpec(std::fstream &op) {
  int mid = GetMeshID();
  Logger::Trace("Writing specs for mesh id %d", mid);
  op.write(reinterpret_cast<char *>(&mid), sizeof(mid));
  op.write(reinterpret_cast<char *>(&diameter_), sizeof(diameter_));
  op.write(reinterpret_cast<char *>(&length_), sizeof(length_));
  op.write(reinterpret_cast<char *>(&bond_length_), sizeof(bond_length_));
  op.write(reinterpret_cast<char *>(&n_sites_), sizeof(int));
  for (auto it = sites_.begin(); it != sites_.end(); ++it) {
    // WriteSpec for sites only writes the site position
    it->WriteSpec(op);
  }
}

void Mesh::ReadCheckpoint(std::fstream &ip) {
  if (ip.eof())
    return;
  void *rng_state = gsl_rng_state(rng_.r);
  size_t rng_size;
  ip.read(reinterpret_cast<char *>(&rng_size), sizeof(size_t));
  ip.read(reinterpret_cast<char *>(rng_state), rng_size);
  Clear();
  ReadSpec(ip);
  Logger::Trace("Reloading mesh from checkpoint with mid %d", GetMeshID());
}

void Mesh::WriteCheckpoint(std::fstream &op) {
  void *rng_state = gsl_rng_state(rng_.r);
  size_t rng_size = gsl_rng_size(rng_.r);
  op.write(reinterpret_cast<char *>(&rng_size), sizeof(size_t));
  op.write(reinterpret_cast<char *>(rng_state), rng_size);
  WriteSpec(op);
}

void Mesh::ScalePosition() {
  for (site_iterator site = sites_.begin(); site != sites_.end(); ++site) {
    site->ScalePosition();
  }
  for (bond_iterator bond = bonds_.begin(); bond != bonds_.end(); ++bond) {
    bond->ScalePosition();
  }
}

Bond *Mesh::GetRandomBond() {
  if (n_bonds_ == 0) {
    return nullptr;
  }
  int i_bond = gsl_rng_uniform_int(rng_.r, n_bonds_);
  return &bonds_[i_bond];
}

void Mesh::UpdateDrTot() {
  if (midstep_) {
    return;
  }
  for (site_iterator site = sites_.begin(); site != sites_.end(); ++site) {
    site->UpdateDrTot();
    double const dr = site->GetDrTot();
    if (dr > dr_tot_) {
      dr_tot_ = dr;
    }
  }
}

void Mesh::ZeroDrTot() {
  dr_tot_ = 0;
  for (site_iterator site = sites_.begin(); site != sites_.end(); ++site) {
    site->ZeroDrTot();
  }
}

double const Mesh::GetDrTot() {
  UpdateDrTot();
  return dr_tot_;
}

std::vector<Interaction *> *Mesh::GetInteractions() {
  for (bond_iterator bond = bonds_.begin(); bond != bonds_.end(); ++bond) {
    std::vector<Interaction *> *bond_ixs = bond->GetInteractions();
    ixs_.insert(ixs_.end(), bond_ixs->begin(), bond_ixs->end());
  }
  return &ixs_;
}

void Mesh::ClearInteractions() {
  for (bond_iterator bond = bonds_.begin(); bond != bonds_.end(); ++bond) {
    bond->ClearInteractions();
  }
}

void Mesh::GetAvgPosition(double *ap) {
  double avg_p[3] = {0.0, 0.0, 0.0};
  int size = 0;
  for (auto it = sites_.begin(); it != sites_.end(); ++it) {
    double const *const p = it->GetPosition();
    for (int i = 0; i < n_dim_; ++i)
      avg_p[i] += p[i];
    size++;
  }
  if (size == 0)
    Logger::Error("Something went wrong in GetAvgPosition!");
  for (int i = 0; i < n_dim_; ++i)
    avg_p[i] /= size;
  std::copy(avg_p, avg_p + 3, ap);
}

void Mesh::GetAvgOrientation(double *au) {
  double avg_u[3] = {0.0, 0.0, 0.0};
  int size = 0;
  for (auto it = sites_.begin(); it != sites_.end(); ++it) {
    double const *const u = it->GetOrientation();
    for (int i = 0; i < n_dim_; ++i)
      avg_u[i] += u[i];
  }
  normalize_vector(avg_u, n_dim_);
  std::copy(avg_u, avg_u + 3, au);
}

void Mesh::SetAvgPosition() {
  posits_only_ = true;
  double avg_pos[3] = {0, 0, 0};
  double avg_u[3] = {0, 0, 0};
  GetAvgPosition(avg_pos);
  GetAvgOrientation(avg_u);
  std::copy(avg_pos, avg_pos + 3, position_);
  for (int i = 0; i < n_dim_; ++i)
    avg_pos[i] = avg_pos[i] - 0.5 * length_ * avg_u[i];
  for (int i_bond = 0; i_bond < n_bonds_; ++i_bond) {
    sites_[i_bond].SetPosition(avg_pos);
    sites_[i_bond].SetOrientation(avg_u);
    for (int i = 0; i < n_dim_; ++i) {
      avg_pos[i] += 0.5 * bond_length_ * avg_u[i];
    }
    bonds_[i_bond].SetPosition(avg_pos);
    bonds_[i_bond].SetOrientation(avg_u);
    bonds_[i_bond].SetDiameter(diameter_);
    bonds_[i_bond].UpdatePeriodic();
    // Set next bond position
    for (int i = 0; i < n_dim_; ++i) {
      avg_pos[i] += 0.5 * bond_length_ * avg_u[i];
    }
  }
  sites_[n_bonds_].SetPosition(avg_pos);
  sites_[n_bonds_].SetOrientation(avg_u);
  SetOrientation(avg_u);
  UpdatePeriodic();
}

void Mesh::GetContactNumbers(std::vector<double> *cn) {
  for (auto it = bonds_.begin(); it != bonds_.end(); ++it) {
    cn->push_back(it->GetContactNumber());
  }
}

void Mesh::GetPolarOrders(std::vector<double> *po) {
  for (auto it = bonds_.begin(); it != bonds_.end(); ++it) {
    po->push_back(it->GetPolarOrder());
  }
}

std::pair<double, double> Mesh::GetAvgOrientationCorrelation() {
  std::pair<double, double> corr_err;
  corr_err.first = 0;
  corr_err.second = 0;
  for (auto it = bonds_.begin(); it != bonds_.end(); ++it) {
    double corr = it->GetOrientationCorrelation();
    corr_err.first += corr;
    corr_err.second += corr * corr;
  }
  corr_err.first /= n_bonds_;
  corr_err.second /= n_bonds_;
  corr_err.second = corr_err.second - SQR(corr_err.first);
  return corr_err;
}

Bond *Mesh::GetBondAtLambda(double lambda) {
  if (lambda < 0) {
    return GetBond(0);
  }
  if (lambda >= length_) {
    return GetBond(n_bonds_ - 1);
  }
  return GetBond((int)floor(lambda / bond_length_));
}

void Mesh::ZeroOrientationCorrelations() {
  for (auto it = bonds_.begin(); it != bonds_.end(); ++it) {
    it->ZeroOrientationCorrelation();
  }
}

double const Mesh::GetBondLength() { return bond_length_; }
