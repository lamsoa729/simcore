#include <iostream>
#include <iomanip>
#include "filament.h"

void Filament::SetParameters(system_parameters *params) {
  length_ = params->rod_length;
  persistence_length_ = params->persistence_length;
  diameter_ = params->rod_diameter;
  max_length_ = params->max_rod_length;
  min_length_ = params->min_rod_length;
  max_child_length_ = 0.5*params->cell_length;
  dynamic_instability_flag_ = params->dynamic_instability_flag;
  force_induced_catastrophe_flag_ = params->force_induced_catastrophe_flag;
  p_g2s_ = params->f_grow_to_shrink*delta_;
  p_g2p_ = params->f_grow_to_pause*delta_;
  p_s2p_ = params->f_shrink_to_pause*delta_;
  p_s2g_ = params->f_shrink_to_grow*delta_;
  p_p2s_ = params->f_pause_to_shrink*delta_;
  p_p2g_ = params->f_pause_to_grow*delta_;
  v_depoly_ = params->v_depoly;
  v_poly_ = params->v_poly;
  gamma_ratio_ = params->gamma_ratio;
  metric_forces_ = params->metric_forces;
  theta_validation_flag_ = params->theta_validation_flag;
  diffusion_validation_flag_ = params->diffusion_validation_flag;
}

void Filament::InitElements(system_parameters *params, space_struct *space) {
  n_bonds_ = (int) ceil(length_/max_child_length_);
  if (n_bonds_ < 2) 
    n_bonds_++;
  n_sites_ = n_bonds_+1;
  child_length_ = length_/n_bonds_;
  // If validating conformation of filament, create
  // the usual test filament
  if (theta_validation_flag_) {
    length_ = 8.0;
    max_child_length_ = 1.0;
    child_length_  = 1.0;
    min_length_ = 1.0;
    n_bonds_ = 8;
    dynamic_instability_flag_ = 0;
    force_induced_catastrophe_flag_ = 0;
  }
  if (length_/n_bonds_ < min_length_) {
    error_exit("ERROR: min_length_ of flexible filament segments too large for filament length.\n");
  }
  // Initialize sites
  for (int i=0; i<n_sites_; ++i) {
    Site s(params, space, gsl_rng_get(rng_.r), GetSID());
    s.SetCID(GetCID());
    elements_.push_back(s);
  }
  // Initialize bonds
  for (int i=0; i<n_bonds_; ++i) {
    Bond b(params, space, gsl_rng_get(rng_.r), GetSID());
    b.SetCID(GetCID());
    b.SetRID(GetRID());
    v_elements_.push_back(b);
  }
  //Allocate control structures
  tensions_.resize(n_sites_-1); //max_sites -1
  g_mat_lower_.resize(n_sites_-2); //max_sites-2
  g_mat_upper_.resize(n_sites_-2); //max_sites-2
  g_mat_diag_.resize(n_sites_-1); //max_sites-1
  det_t_mat_.resize(n_sites_+1); //max_sites+1
  det_b_mat_.resize(n_sites_+1); //max_sites+1
  g_mat_inverse_.resize(n_sites_-2); //max_sites-2
  k_eff_.resize(n_sites_-2); //max_sites-2
  h_mat_diag_.resize(n_sites_-1); //max_sites-1
  h_mat_upper_.resize(n_sites_-2); //max_sites-2
  h_mat_lower_.resize(n_sites_-2); //max_sites-2
  gamma_inverse_.resize(n_sites_*n_dim_*n_dim_); //max_sites*ndim*ndim
  cos_thetas_.resize(n_sites_-2); //max_sites-2
}

void Filament::DiffusionInit() {
  for (int i=0; i<3; ++i) {
    position_[i] = 0.0;
    orientation_[i] = 0.0;
  }
  position_[n_dim_-1] = -0.5*length_;
  orientation_[n_dim_-1] = 1.0;
  for (auto site=elements_.begin(); site!=elements_.end(); ++site) {
    site->SetDiameter(diameter_);
    site->SetLength(child_length_);
    site->SetPosition(position_);
    site->SetOrientation(orientation_);
    for (int i=0; i<n_dim_; ++i)
      position_[i] = position_[i] + orientation_[i] * child_length_;
  }
  UpdatePrevPositions();
  CalculateAngles();
  UpdateBondPositions();
  SetDiffusion();
  poly_state_ = GROW;
}

void Filament::Init() {
  if (diffusion_validation_flag_) {
    DiffusionInit();
    return;
  }
  InsertRandom(length_+diameter_);
  generate_random_unit_vector(n_dim_, orientation_, rng_.r);
  for (auto site=elements_.begin(); site!=elements_.end(); ++site) {
    site->SetDiameter(diameter_);
    site->SetLength(child_length_);
    site->SetPosition(position_);
    site->SetOrientation(orientation_);
    for (int i=0; i<n_dim_; ++i)
      position_[i] = position_[i] + orientation_[i] * child_length_;
    GenerateProbableOrientation();
  }
  UpdatePrevPositions();
  CalculateAngles();
  UpdateBondPositions();
  SetDiffusion();
  poly_state_ = GROW;
}

void Filament::SetDiffusion() {
  double eps = log(2.0*length_);
  double gamma_0 = 4.0/3.0*eps*((1+0.64*eps)/(1-1.15*eps) + 1.659 * SQR(eps));
  gamma_perp_ = child_length_ * gamma_0;
  gamma_par_ = gamma_perp_ / gamma_ratio_;
  rand_sigma_perp_ = sqrt(24.0*gamma_perp_/delta_);
  rand_sigma_par_ = sqrt(24.0*gamma_par_/delta_);
}

void Filament::GenerateProbableOrientation() {

  /* This updates the current orientation with a generated probable orientation where
  we generate random theta pulled from probability distribution P(th) = exp(k cos(th))
  where k is the persistence_length of the filament
  If k is too large, there is enormous imprecision in this calculation since sinh(k)
  is very large so to fix this I introduce an approximate distribution that is valid 
  for large k */

  double theta;

  if (persistence_length_ == 0) {
    theta = gsl_rng_uniform_pos(rng_.r) * M_PI;
  }
  else if (persistence_length_ < 100) {
    theta = gsl_rng_uniform_pos(rng_.r) * M_PI;
    theta = acos( log( exp(-persistence_length_/child_length_) + 
          2.0*gsl_rng_uniform_pos(rng_.r)*sinh(persistence_length_/child_length_) ) 
        / (persistence_length_/child_length_) );
  }
  else {
    theta = acos( (log( 2.0*gsl_rng_uniform_pos(rng_.r)) - 
          log(2.0) + persistence_length_/child_length_)
        /(persistence_length_/child_length_) );
  }

  double new_orientation[3] = {0, 0, 0};
  if (n_dim_==2) {
    theta = (gsl_rng_uniform_int(rng_.r,2)==0 ? -1 : 1) * theta;
    new_orientation[0] = cos(theta);
    new_orientation[1] = sin(theta);
  }
  else {
    double phi = gsl_rng_uniform_pos(rng_.r) * 2.0 * M_PI;
    new_orientation[0] = sin(theta)*cos(phi);
    new_orientation[1] = sin(theta)*sin(phi);
    new_orientation[2] = cos(theta);
  }
  rotate_orientation_vector(n_dim_, new_orientation, orientation_);
  std::copy(new_orientation,new_orientation+3,orientation_);
}

void Filament::UpdatePosition(bool midstep) {
  UpdatePositionMP(midstep);
}

void Filament::UpdatePositionMP(bool midstep) {
  ZeroForce();
  ApplyForcesTorques();
  Integrate(midstep);
}

void Filament::Integrate(bool midstep) {
  CalculateAngles();
  CalculateTangents();
  if (midstep) {
    //if (theta_validation_flag_) {
      //ValidateThetaDistribution();
    //}
    //if (position_correlation_flag) {
      //ValidatePositionStepDistribution();
    //}
    GenerateRandomForces();
    ProjectRandomForces();
    UpdatePrevPositions();
  }
  AddRandomForces();
  CalculateBendingForces();
  CalculateTensions();
  UpdateSitePositions(midstep);
  UpdateBondPositions();
}

void Filament::CalculateAngles() {
  double cos_angle;
  //bool sharp_angle = false;
  for (int i_site=0; i_site<n_sites_-2; ++i_site) {
    double const * const u1 = elements_[i_site].GetOrientation();
    double const * const u2 = elements_[i_site+1].GetOrientation();
    cos_angle = dot_product(n_dim_, u1, u2);
    cos_thetas_[i_site] = cos_angle;
    //if (cos_angle <= 0 dynamic_instability_flag_)
      //error_exit("ERROR: Acute angle between adjoining segments detected with dynamic instabiliy on.\n \
          //Increase persistence length or turn off dynamic instability for floppy filaments\n");
    //if (cos_angle < 0.7 && params_->dynamic_instability_flag)
      //sharp_angle = true;
  }
  // If dynamic instability is on, make sure the angle between two adjoining segments is
  // less than 0.5*pi, or else the site rescaling will fail horribly.
  // This is only an issue for floppy filaments.
  //if (rescale && sharp_angle && midstep && length_/(n_segments_+1) > min_length_) {
    //AddSite();
    //CalculateAngles(true);
  //}
}

void Filament::CalculateTangents() {
  elements_[0].SetTangent(elements_[0].GetOrientation());
  elements_[n_sites_-1].SetTangent(elements_[n_sites_-2].GetOrientation());
  double u_tan_mag;
  double u_tan[3];
  for (int i_site=1;i_site < n_sites_-1; ++i_site) {
    double const * const u1 = elements_[i_site-1].GetOrientation();
    double const * const u2 = elements_[i_site].GetOrientation();
    u_tan_mag = 0.0;
    for (int i=0; i<n_dim_; ++i) {
      u_tan[i] = u2[i]+u1[i];
      u_tan_mag += SQR(u_tan[i]);
    }
    u_tan_mag = sqrt(u_tan_mag);
    for (int i=0; i<n_dim_; ++i)
      u_tan[i]/=u_tan_mag;
    elements_[i_site].SetTangent(u_tan);
  }
}

void Filament::UpdatePrevPositions() {
  for (auto site=elements_.begin(); site!=elements_.end(); ++site)
    site->SetPrevPosition(site->GetPosition());
}

void Filament::AddRandomForces() {
  for (auto site=elements_.begin(); site!=elements_.end(); ++site)
    site->AddRandomForce();
}

//void Filament::ValidateThetaDistribution() {
  //int n_bins = params_->n_bins;
  //int bin_number;
  //int **cos_dist = ctrl_->cos_theta_dist;
  //for (int i=0; i<n_segments_-1; ++i) {
    //bin_number = (int) floor( (1 + cos_thetas_[i]) * (n_bins/2) );
    //if (bin_number == n_bins) bin_number = n_bins-1;
    //else if (bin_number == -1) bin_number = 0;
    //else if (bin_number > n_bins && bin_number < 0) error_exit("Something went wrong in validate_theta_dist_sphero!\n");
    //cos_dist[i][bin_number]++;
  //}
//}

//void Filament::ValidatePositionStepDistribution() {
  //if (first_iteration_) {
    //first_iteration_ = false;
    //return; // FIXME
  //}
  //double *dr = new double[n_dim_];
  //double *r_old, *r_new;
  //int **pos_corr = ctrl_->position_step_correlation;
  //int n_site = 0;
  //for (site_iterator i_site = sites.begin(); i_site != sites.end(); ++i_site) {
    //r_old = i_site->GetPrevPosition();
    //r_new = i_site->GetPosition();
    //for (int i=0; i<n_dim_; ++i)
      //dr[i] = r_new[i] - r_old[i];
    //normalize_vector(dr, n_dim_);
    //double correlation = dot_product(n_dim_, i_site->GetOrientation(), dr);
    //int bin_number = (int) floor( (1 + correlation) * (params_->n_bins/2) );
    //pos_corr[n_site][bin_number]++;
    //n_site++;
  //}
//}

void Filament::GenerateRandomForces() {
  double xi[3], xi_term[3], f_rand[3];
  for (int i_site=0; i_site<n_sites_; ++i_site) {
    double const * const utan = elements_[i_site].GetTangent();
    for (int i=0; i<n_dim_; ++i) 
      //xi[i] = 0.1*(i+3) - 0.5;
      xi[i] = gsl_rng_uniform_pos(rng_.r) - 0.5;
    // Create unprojected forces, see J. Chem. Phys. 122, 084903 (2005), eqn. 40.
    if (n_dim_ == 2) {
      xi_term[0] = SQR(utan[0]) * xi[0] + utan[0] * utan[1] * xi[1];
      xi_term[1] = SQR(utan[1]) * xi[1] + utan[0] * utan[1] * xi[0];
    }
    else if (n_dim_==3) {
      xi_term[0] = SQR(utan[0]) * xi[0] + utan[0] * utan[1] * xi[1]
                  + utan[0] * utan[2] * xi[2];
      xi_term[1] = SQR(utan[1]) * xi[1] + utan[0] * utan[1] * xi[0]
                  + utan[1] * utan[2] * xi[2];
      xi_term[2] = SQR(utan[2]) * xi[2] + utan[0] * utan[2] * xi[0]
                  + utan[1] * utan[2] * xi[1];
    }
    for (int i=0; i<n_dim_; ++i) {
      f_rand[i] = rand_sigma_perp_ * xi[i] + 
          (rand_sigma_par_ - rand_sigma_perp_) * xi_term[i];
    }
    elements_[i_site].SetRandomForce(f_rand);
  }
}

void Filament::ProjectRandomForces() {

  double f_rand_temp[3];
  for (int i_site=0; i_site<n_sites_-1; ++i_site) {
    // First get the tensions_ elements
    double const * const f_rand1 = elements_[i_site].GetRandomForce();
    double const * const f_rand2 = elements_[i_site+1].GetRandomForce();
    double const * const u_site = elements_[i_site].GetOrientation();
    for (int i=0; i<n_dim_; ++i)
      f_rand_temp[i] = f_rand2[i] - f_rand1[i];
    tensions_[i_site] = dot_product(n_dim_, f_rand_temp, u_site);
    // Then get the G arrays (for inertialess case where m=1, see
    // ref. 15 of above paper)
    g_mat_diag_[i_site] = 2;
    if (i_site > 0) {
      g_mat_upper_[i_site-1] = - cos_thetas_[i_site-1];
      g_mat_lower_[i_site-1] = - cos_thetas_[i_site-1];
    }
  }

  // Now solve using tridiagonal solver
  tridiagonal_solver(&g_mat_lower_, &g_mat_diag_, &g_mat_upper_, &tensions_, n_sites_-1);
  // Update to the projected brownian forces
  // First the end sites:
  
  double f_proj[3];
  for (int i=0; i<n_dim_; ++i) {
    f_proj[i] = elements_[0].GetRandomForce()[i]
      + tensions_[0] * elements_[0].GetOrientation()[i];
  }
  elements_[0].SetRandomForce(f_proj);
  for (int i=0; i<n_dim_; ++i) {
    f_proj[i] = elements_[n_sites_-1].GetRandomForce()[i] 
    - tensions_[n_sites_-2] * elements_[n_sites_-2].GetOrientation()[i];
  }
  elements_[n_sites_-1].SetRandomForce(f_proj);
  // Then the rest
  //u_site2=u_site;
  for (int i_site=1; i_site<n_sites_-1; ++i_site) {
    double const * const u1 = elements_[i_site-1].GetOrientation();
    double const * const u2 = elements_[i_site].GetOrientation();
    for (int i=0; i<n_dim_; ++i) {
      f_proj[i] = elements_[i_site].GetRandomForce()[i] + tensions_[i_site]*u2[i] - tensions_[i_site-1] * u1[i];
    }
    elements_[i_site].SetRandomForce(f_proj);
  }
}

void Filament::CalculateBendingForces() {
  
  if (metric_forces_) {
    det_t_mat_[0] = 1;
    det_t_mat_[1] = 2;
    det_b_mat_[n_sites_] = 1;
    det_b_mat_[n_sites_-1] = 2;
    for (int i=2; i<n_sites_; ++i) {
      det_t_mat_[i] = 2 * det_t_mat_[i-1] - SQR(- cos_thetas_[i-2]) * det_t_mat_[i-2];
      det_b_mat_[n_sites_-i] = 2 * det_b_mat_[n_sites_-i+1] - SQR(- cos_thetas_[n_sites_-i-1]) * det_b_mat_[n_sites_-i+2];
    }
    double det_g = det_t_mat_[n_sites_-1];
    for(int i=0; i<n_sites_-2; ++i) {
      g_mat_inverse_[i] = cos_thetas_[i] * det_t_mat_[i] * det_b_mat_[i+3] / det_g;
    }
  }
  else {
    for(int i=0; i<n_sites_-2; ++i) {
      g_mat_inverse_[i] = 0;
    }
  }
  // Now calculate the effective rigidities
  for (int i=0; i<n_sites_-2; ++i) {
    k_eff_[i] = (persistence_length_ * (theta_validation_flag_ ? child_length_ : 1) + child_length_ * g_mat_inverse_[i])/SQR(child_length_);
  }
  // Using these, we can calculate the forces on each of the sites

  // These calculations were done by hand and are not particularly readable,
  // but are more efficient than doing it explicitly in the code for readability
  // If this ever needs fixed, you need to either check the indices very carefully
  // or redo the calculation by hand! 
  // See Pasquali and Morse, J. Chem. Phys. Vol 116, No 5 (2002)
  double f_site[3] = {0, 0, 0};
  if (n_dim_ == 2) {
    for (int k_site=0; k_site<n_sites_; ++k_site) {
      std::fill(f_site,f_site+3,0.0);
      if (k_site>1) {
        double const * const u1 = elements_[k_site-2].GetOrientation();
        double const * const u2 = elements_[k_site-1].GetOrientation();
        f_site[0] += k_eff_[k_site-2] * ( (1-SQR(u2[0]))*u1[0] - u2[0]*u2[1]*u1[1] );
        f_site[1] += k_eff_[k_site-2] * ( (1-SQR(u2[1]))*u1[1] - u2[0]*u2[1]*u1[0] );
      }
      if (k_site>0 && k_site<n_sites_-1) {
        double const * const u1 = elements_[k_site-1].GetOrientation();
        double const * const u2 = elements_[k_site].GetOrientation();
        f_site[0] += k_eff_[k_site-1] * ( (1-SQR(u1[0]))*u2[0] - u1[0]*u1[1]*u2[1]
                        -((1-SQR(u2[0]))*u1[0] - u2[0]*u2[1]*u1[1]) );
        f_site[1] += k_eff_[k_site-1] * ( (1-SQR(u1[1]))*u2[1] - u1[0]*u1[1]*u2[0]
                        -((1-SQR(u2[1]))*u1[1] - u2[0]*u2[1]*u1[0]) );
      }
      if (k_site<n_sites_-2) {
        double const * const u1 = elements_[k_site].GetOrientation();
        double const * const u2 = elements_[k_site+1].GetOrientation();
        f_site[0] -= k_eff_[k_site] * ( (1-SQR(u1[0]))*u2[0] - u1[0]*u1[1]*u2[1] );
        f_site[1] -= k_eff_[k_site] * ( (1-SQR(u1[1]))*u2[1] - u1[0]*u1[1]*u2[0] );
      }
      elements_[k_site].AddForce(f_site);
}
  }
  else if (n_dim_ == 3) {
    for (int k_site=0; k_site<n_sites_; ++k_site) {
      std::fill(f_site,f_site+3,0.0);
      if (k_site>1) {
        double const * const u1 = elements_[k_site-2].GetOrientation();
        double const * const u2 = elements_[k_site-1].GetOrientation();
        f_site[0] += k_eff_[k_site-2] * ( (1-SQR(u2[0]))*u1[0] - u2[0]*u2[1]*u1[1] - u2[0]*u2[2]*u1[2] );
        f_site[1] += k_eff_[k_site-2] * ( (1-SQR(u2[1]))*u1[1] - u2[1]*u2[0]*u1[0] - u2[1]*u2[2]*u1[2] );
        f_site[2] += k_eff_[k_site-2] * ( (1-SQR(u2[2]))*u1[2] - u2[2]*u2[0]*u1[0] - u2[2]*u2[1]*u1[1] );
      }
      if (k_site>0 && k_site<n_sites_-1) {
        double const * const u1 = elements_[k_site-1].GetOrientation();
        double const * const u2 = elements_[k_site].GetOrientation();
        f_site[0] += k_eff_[k_site-1] * ( (1-SQR(u1[0]))*u2[0] - u1[0]*u1[1]*u2[1] - u1[0]*u1[2]*u2[2] 
                        - ( (1-SQR(u2[0]))*u1[0] - u2[0]*u2[1]*u1[1] - u2[0]*u2[2]*u1[2] ) );
        f_site[1] += k_eff_[k_site-1] * ( (1-SQR(u1[1]))*u2[1] - u1[1]*u1[0]*u2[0] - u1[1]*u1[2]*u2[2]
                        - ( (1-SQR(u2[1]))*u1[1] - u2[1]*u2[0]*u1[0] - u2[1]*u2[2]*u1[2] ) );
        f_site[2] += k_eff_[k_site-1] * ( (1-SQR(u1[2]))*u2[2] - u1[2]*u1[0]*u2[0] - u1[2]*u1[1]*u2[1]
                        - ( (1-SQR(u2[2]))*u1[2] - u2[2]*u2[0]*u1[0] - u2[1]*u2[2]*u1[1] ) );
      }
      if(k_site<n_sites_-2) {
        double const * const u1 = elements_[k_site].GetOrientation();
        double const * const u2 = elements_[k_site+1].GetOrientation();
        f_site[0] -= k_eff_[k_site] * ( (1-SQR(u1[0]))*u2[0] - u1[0]*u1[1]*u2[1] - u1[0]*u1[2]*u2[2] );
        f_site[1] -= k_eff_[k_site] * ( (1-SQR(u1[1]))*u2[1] - u1[1]*u1[0]*u2[0] - u1[1]*u1[2]*u2[2] );
        f_site[2] -= k_eff_[k_site] * ( (1-SQR(u1[2]))*u2[2] - u1[2]*u1[0]*u2[0] - u1[2]*u1[1]*u2[1] );
      }
      elements_[k_site].AddForce(f_site);
    }
  }
}

void Filament::CalculateTensions() {

  // Calculate friction_inverse matrix
  int site_index = 0;
  int next_site = n_dim_*n_dim_;
  for (int i_site=0; i_site<n_sites_; ++i_site) {
    int gamma_index = 0;
    double const * const utan = elements_[i_site].GetTangent();
    for (int i=0; i<n_dim_; ++i) {
      for (int j=0; j<n_dim_; ++j) {
        gamma_inverse_[site_index+gamma_index] = 1.0/gamma_par_ * (utan[i]*utan[j])
                                     + 1.0/gamma_perp_ * ((i==j ? 1 : 0) - utan[i]*utan[j]);
        gamma_index++;
      }
    }
    site_index += next_site;
  }
  // Populate the H matrix and Q vector using tensions (p_vec) array
  double temp_a, temp_b;
  double f_diff[3];
  double utan1_dot_u2, utan2_dot_u2;
  site_index = 0;
  for (int i_site=0; i_site<n_sites_-1; ++i_site) {
    // f_diff is the term in par_entheses in equation 29 of J. Chem. Phys. 122, 084903 (2005)
    double const * const f1 = elements_[i_site].GetForce();
    double const * const f2 = elements_[i_site+1].GetForce();
    double const * const u2 = elements_[i_site].GetOrientation();
    double const * const utan1 = elements_[i_site].GetTangent();
    double const * const utan2 = elements_[i_site+1].GetTangent();
    for (int i=0; i<n_dim_; ++i) {
      temp_a = gamma_inverse_[site_index+n_dim_*i] * f1[0] + gamma_inverse_[site_index + n_dim_*i+1] * f1[1];
      if (n_dim_ == 3) temp_a += gamma_inverse_[site_index+n_dim_*i+2] * f1[2];
      temp_b = gamma_inverse_[site_index+next_site+n_dim_*i] * f2[0] + gamma_inverse_[site_index+next_site+n_dim_*i+1] * f2[1];
      if (n_dim_ == 3) temp_b += gamma_inverse_[site_index+next_site+n_dim_*i+2] * f2[2];
      f_diff[i] = temp_b - temp_a;
    }
    tensions_[i_site] = dot_product(n_dim_, u2, f_diff);
    utan1_dot_u2 = dot_product(n_dim_, utan1, u2);
    utan2_dot_u2 = dot_product(n_dim_, utan2, u2);
    h_mat_diag_[i_site] = 2.0/gamma_perp_ + (1.0/gamma_par_ - 1.0/gamma_perp_) *
      (SQR(utan1_dot_u2) + SQR(utan2_dot_u2));
    if (i_site>0) {
      double const * const u1 = elements_[i_site-1].GetOrientation();
      h_mat_upper_[i_site-1] = -1.0/gamma_perp_ * dot_product(n_dim_, u2, u1)
        - (1.0/gamma_par_ - 1.0/gamma_perp_) * (dot_product(n_dim_, utan1, u1) *
            dot_product(n_dim_, utan1, u2));
      h_mat_lower_[i_site-1] = h_mat_upper_[i_site-1];
    }
    site_index += next_site;
  }
  tridiagonal_solver(&h_mat_lower_, &h_mat_diag_, &h_mat_upper_, &tensions_, n_sites_-1);
}

double const * const Filament::GetDrTot() {return nullptr;}

void Filament::ApplyForcesTorques() {}

void Filament::Draw(std::vector<graph_struct*> * graph_array) {
  for (auto bond=v_elements_.begin(); bond!= v_elements_.end(); ++bond) {
    bond->SetColor(color_, draw_type_);
    bond->Draw(graph_array);
  }
}

void Filament::UpdateSitePositions(bool midstep) {

  double delta = (midstep ? 0.5*delta_ : delta_);
  double f_site[3];

  // First get total forces
  // Handle end sites first
  for (int i=0; i<n_dim_; ++i)
    f_site[i] = tensions_[0] * elements_[0].GetOrientation()[i];
  elements_[0].AddForce(f_site);
  for (int i=0; i<n_dim_; ++i)
    f_site[i] = -tensions_[n_sites_-2] * elements_[n_sites_-2].GetOrientation()[i];
  elements_[n_sites_-1].AddForce(f_site);
  
  // and then the rest
  for (int i_site=1; i_site<n_sites_-1; ++i_site) {
    //f_site = elements_[i_site].GetForce();
    double const * const u_site1 = elements_[i_site-1].GetOrientation();
    double const * const u_site2 = elements_[i_site].GetOrientation();
    for (int i=0; i<n_dim_; ++i) {
      f_site[i] = tensions_[i_site] * u_site2[i] - tensions_[i_site-1] * u_site1[i];
    }
    elements_[i_site].AddForce(f_site);
  }

  // Now update positions
  double f_term[3], r_new[3];
  int site_index = 0;
  int next_site = n_dim_*n_dim_;
  for (int i_site=0; i_site<n_sites_; ++i_site) {
    double const * const f_site1 = elements_[i_site].GetForce();
    double const * const r_site1 = elements_[i_site].GetPosition();
    double const * const r_prev = elements_[i_site].GetPrevPosition();
    for (int i=0; i<n_dim_; ++i) {
      f_term[i] = gamma_inverse_[site_index+n_dim_*i] * f_site1[0] + gamma_inverse_[site_index+n_dim_*i+1] * f_site1[1];
      if (n_dim_ == 3)
        f_term[i] += gamma_inverse_[site_index+n_dim_*i+2] * f_site1[2];
      r_new[i] = r_prev[i] + delta * f_term[i];
    }
    elements_[i_site].SetPosition(r_new);
    site_index += next_site;
  }

  // Next, update orientation vectors
  double u_mag, r_diff[3];
  for (int i_site=0; i_site<n_sites_-1; ++i_site) {
    double const * const r_site1 = elements_[i_site].GetPosition();
    double const * const r_site2 = elements_[i_site+1].GetPosition();
    u_mag = 0.0;
    for (int i=0; i<n_dim_; ++i) {
      r_diff[i] = r_site2[i] - r_site1[i];
      u_mag += SQR(r_diff[i]);
    }
    u_mag = sqrt(u_mag);
    //if (error_analysis_flag_) {
      //ctrl_->pos_error[ctrl_->pos_error_ind] = ABS(child_length_ - u_mag)/child_length_;
      //ctrl_->pos_error_ind++;
    //}
    for (int i=0; i<n_dim_; ++i)
      r_diff[i]/=u_mag;
    elements_[i_site].SetOrientation(r_diff);
  }
  elements_[n_sites_-1].SetOrientation(elements_[n_sites_-2].GetOrientation());

  // Finally, normalize site positions, making sure the sites are still rod-length apart
  for (int i_site=1; i_site<n_sites_; ++i_site) {
    double const * const r_site1 = elements_[i_site-1].GetPosition();
    double const * const u_site1 = elements_[i_site-1].GetOrientation();
    for (int i=0; i<n_dim_; ++i)
      r_diff[i] = r_site1[i] + child_length_ * u_site1[i];
    elements_[i_site].SetPosition(r_diff);
  }
}


void Filament::UpdateBondPositions() {
  double pos[3];
  int i_site = 0;
  if (elements_.size() != v_elements_.size() + 1)
    error_exit("ERROR: n_bonds != n_sites - 1 in flexible filament!\n");
  for (auto bond=v_elements_.begin(); bond!=v_elements_.end(); ++bond) {
    double const * const r = elements_[i_site].GetPosition();
    double const * const u = elements_[i_site].GetOrientation();
    for (int i=0; i<n_dim_; ++i)
      pos[i] = r[i] + 0.5 * child_length_ * u[i];
    bond->SetPosition(pos);
    bond->SetOrientation(u);
    bond->SetLength(child_length_);
    bond->UpdatePeriodic();
    i_site++;
  }
}


//void Filament::DynamicInstability() {
  //UpdatePolyState();
  //GrowFilament();
  //SetDiffusion();
//}

//void Filament::GrowFilament() {
  //// If the filament is paused, do nothing
  //if (poly_state_ == PAUSE)
    //return;
  //double delta_length;
  //if (poly_state_ == GROW) {
    //delta_length = params_->v_poly * delta_;
    //length_ += delta_length;
  //}
  //else if (poly_state_ == SHRINK) {
    //delta_length = params_->v_depoly * delta_;
    //length_ -= delta_length;
  //}
  //RescaleSegments();
  //if (child_length_ > max_child_length_) 
    //AddSite();
  //else if (child_length_ < min_length_) 
    //RemoveSite();
//}

//void Filament::RescaleSegments() {
  //UpdatePrevPositions();
  //int n_dim_ = params_->n_dim_;
  //double old_segment_length = child_length_;
  //child_length_ = length_/n_segments_;
  //double k, dl;
  //if (poly_state_ == SHRINK) {
    //double *r2=elements_[1].GetPosition();
    //double *r1=elements_[0].GetPrevPosition();
    //double *u1=elements_[0].GetOrientation();
    //double *r2_old;
    //for (int i=0; i<n_dim_; ++i) {
      //r2[i] = r1[i] + u1[i] * child_length_;
    //}
    //dl = old_segment_length - child_length_;
    //for (int i_site=2; i_site<n_sites_; ++i_site) {
      //r2 = elements_[i_site].GetPosition();
      //r2_old = elements_[i_site].GetPrevPosition();
      //r1 = elements_[i_site-1].GetPrevPosition();
      //u1 = elements_[i_site-1].GetOrientation();
      //k = SQR(dl*cos_thetas_[i_site-2]) - SQR(dl) + SQR(child_length_);
      //k = (k > 0 ? k : 0);
      //k = -cos_thetas_[i_site-2]*dl + sqrt(k);
      //for (int i=0; i<n_dim_; ++i)
        //r2[i] = r1[i] + k * u1[i];
      //dl = old_segment_length - k;
    //}
  //}
  //else if (poly_state_ == GROW) {
    //double *r, *r_old, *u;
    //dl = old_segment_length;
    //for (int i_site=1; i_site<n_sites_-1; ++i_site) {
      //r = elements_[i_site].GetPosition();
      //r_old = elements_[i_site].GetPrevPosition();
      //u = elements_[i_site].GetOrientation();
      //k = SQR(dl*cos_thetas_[i_site-1]) - SQR(dl) + SQR(child_length_);
      //k = (k > 0 ? k : 0);
      //k = -cos_thetas_[i_site-1]*dl + sqrt(k);
      //for (int i=0; i<n_dim_; ++i)
        //r[i] = r_old[i] + k * u[i];
      //dl = old_segment_length - k;
    //}
    //r = elements_[n_sites_-1].GetPosition();
    //u = elements_[n_sites_-2].GetOrientation();
    //r_old = elements_[n_sites_-2].GetPosition();
    //for (int i=0; i<n_dim_; ++i)
      //r[i] = r_old[i] + child_length_ * u[i];
  //}
  //for (site_iterator i_site=sites.begin();
      //i_site != sites.end();
      //++i_site)
    //i_site->SetLength(child_length_);
  //UpdateSiteOrientations();
  //CalculateAngles(false);
//}

//void Filament::UpdateSiteOrientations() {
  //for (site_iterator i_site = sites.begin();
      //i_site != sites.end()-1;
      //++i_site) {
    //double u_site[3];
    //double *r_site1 = i_site->GetPosition();
    //double *r_site2 = (i_site+1)->GetPosition();
    //for (int i=0; i<n_dim_; ++i)
      //u_site[i] = r_site2[i] - r_site1[i];
    //normalize_vector(u_site, n_dim_);
    //i_site->SetOrientation(u_site);
  //}
  //// Have the orientation of the last site be parallel to the orientation
  //// vector of the segment before it
  //elements_[n_sites_-1].SetOrientation(elements_[n_sites_-2].GetOrientation());
//}

//void Filament::AddSite() {
  //UpdatePrevPositions();
  //int n_dim_ = params_->n_dim_;
  //Site new_site(n_dim_);
  //new_site.SetDiameter(diameter_);
  //// Place the new site in the same position as the current last site
  //sites.push_back(new_site);
  //n_sites_++;
  //n_segments_++;
  //double old_segment_length = child_length_;
  //child_length_ = length_/n_segments_;
  //double error = 0;
  //double dl = 1;
  //int j=0;
  //double *r2, *r1, *u1, *r2_old, k;
  //do {
    //child_length_ = child_length_ + error/n_segments_;
    //r2=elements_[1].GetPosition();
    //r1=elements_[0].GetPrevPosition();
    //u1=elements_[0].GetOrientation();
    //r2_old;
    //k;
    //for (int i=0; i<n_dim_; ++i) {
      //r2[i] = r1[i] + u1[i] * child_length_;
    //}
    //dl = old_segment_length - child_length_;
    //for (int i_site=2; i_site<n_sites_-1; ++i_site) {
      //r2 = elements_[i_site].GetPosition();
      //r2_old = elements_[i_site].GetPrevPosition();
      //r1 = elements_[i_site-1].GetPrevPosition();
      //u1 = elements_[i_site-1].GetOrientation();
      //k = SQR(dl*cos_thetas_[i_site-2]) + SQR(child_length_) - SQR(dl);
      //k = (k>0 ? k : 0);
      //k = -cos_thetas_[i_site-2]*dl + sqrt(k);
      //dl = 0;
      //for (int i=0; i<n_dim_; ++i) {
        //r2[i] = r1[i] + k * u1[i];
        //dl += SQR(r2_old[i]-r2[i]);
      //}
      //dl = sqrt(dl);
    //}
    //r2 = elements_[n_sites_-1].GetPosition();
    //r2_old = elements_[n_sites_-2].GetPrevPosition();
    //r1 = elements_[n_sites_-2].GetPosition();
    //u1 = elements_[n_sites_-3].GetOrientation();
    //for (int i=0; i<n_dim_; ++i) 
      //r2[i] = r1[i] + child_length_ * u1[i];
    //error = old_segment_length - k - child_length_;
    //dl = ABS(error);
    //j++;
    //if (error!=error || j > 100) {
      //std::cout << j << std::endl;
      //std::cout << error << std::endl;
      //error_exit("Error while adding site\n");
    //}
  //} while (dl > 1e-3);
  //elements_[n_sites_-1].SetScaledPosition(elements_[n_sites_-1].GetPosition());
  //length_ = child_length_ * n_segments_;
  //for (site_iterator i_site=sites.begin();
      //i_site != sites.end();
      //++i_site)
    //i_site->SetLength(child_length_);
  //UpdateSiteOrientations();
  //CalculateAngles(false);
//}

//void Filament::RemoveSite() {
  //int n_dim_ = params_->n_dim_;
  //UpdatePrevPositions();
  //n_sites_--;
  //n_segments_--;
  //double old_segment_length = child_length_;
  //child_length_ = length_/n_segments_;
  //double error = 0;
  //double dl = 1;
  //int j=0;
  //do {
    //child_length_ = child_length_ + error/n_segments_;
    //double *r, *r_old, *u;
    //double k;
    //dl = old_segment_length;
    //for (int i_site=1; i_site<n_sites_; ++i_site) {
      //r = elements_[i_site].GetPosition();
      //r_old = elements_[i_site].GetPrevPosition();
      //u = elements_[i_site].GetOrientation();
      //k = SQR(dl*cos_thetas_[i_site-1]) + SQR(child_length_) - SQR(dl);
      //k = (k>0 ? k : 0);
      //k = -cos_thetas_[i_site-1]*dl + sqrt(k);
      //for (int i=0; i<n_dim_; ++i)
        //r[i] = r_old[i] + k * u[i];
      //dl = old_segment_length - k;
    //}
    //error = old_segment_length - k;
    //dl = ABS(error);
    //j++;
    //if (error!=error || j > 100) {
      //std::cout << j << std::endl;
      //std::cout << error << std::endl;
      //error_exit("Error while removing site\n");
    //}
  //} while (dl > 1e-3);
  //sites.pop_back();
  //length_ = child_length_ * n_segments_;
  //for (site_iterator i_site=sites.begin();
      //i_site != sites.end();
      //++i_site)
    //i_site->SetLength(child_length_);
  //UpdateSiteOrientations();
  //CalculateAngles(false);
//}

//void Filament::UpdatePolyState() {
  //double roll = gsl_rng_uniform_pos(rng_.r);
  //// temporary variables used for modification from
  //// force induced catastrophe flag
  //double p_g2s = p_g2s_;
  //double p_p2s = p_p2s_;
  //if (force_induced_catastrophe_flag_ && tip_force_ > 0.0) {
    //double p_factor = exp(0.0828*tip_force_);
    //p_g2s = (p_g2s_+p_g2p_)*p_factor;
    //p_p2s = p_p2s_*p_factor;
  //}
  //double p_norm;
  //// Filament shrinking
  //if (poly_state_ == SHRINK) {
    //p_norm = p_s2g_ + p_s2p_;
    //if (p_norm > 1.0) 
      //poly_state_ = (roll < p_s2g_/p_norm ? GROW : PAUSE);
    //else if (roll < p_s2g_) 
      //poly_state_ = GROW;
    //else if (roll < (p_s2g_ + p_s2p_)) 
      //poly_state_ = PAUSE;
  //}
  //// Filament growing
  //else if (poly_state_ == GROW) {
    //p_norm = p_g2s + p_g2p_;
    //if (p_norm > 1.0)
      //poly_state_ = (roll < p_g2s/p_norm ? SHRINK : PAUSE);
    //else if (roll < p_g2s) 
      //poly_state_ = SHRINK;
    //else if (roll < (p_g2s + p_g2p_)) 
      //poly_state_ = PAUSE;
  //}
  //// Filament paused
  //else if (poly_state_ == PAUSE) {
    //p_norm = p_p2g_ + p_p2s;
    //if (p_norm > 1) 
      //poly_state_ = (roll < p_p2g_/p_norm ? GROW : SHRINK);
    //else if (roll < p_p2g_) 
      //poly_state_ = GROW;
    //else if (roll < (p_p2g_ + p_p2s)) 
      //poly_state_ = SHRINK;
  //}

  //// Check to make sure the filament lengths stay in the correct ranges
  //if (length_ < min_length_) 
    //poly_state_ = GROW;
  //else if (length_ > max_length_) 
    //poly_state_ = SHRINK;
//}

// Species specifics, including insertion routines
void FilamentSpecies::Configurator() {
  char *filename = params_->config_file;

  std::cout << "Filament species\n";

  YAML::Node node = YAML::LoadFile(filename);

  std::cout << " Generic Properties:\n";

  // Check insertion type
  std::string insertion_type;
  insertion_type = node["filament"]["properties"]["insertion_type"].as<std::string>();
  std::cout << "   insertion type: " << insertion_type << std::endl;
  bool can_overlap = node["filament"]["properties"]["overlap"].as<bool>();
  std::cout << "   overlap:        " << (can_overlap ? "true" : "false") << std::endl;

  // Coloring
  double color[4] = {1.0, 0.0, 0.0, 1.0};
  int draw_type = 1; // default to orientation
  if (node["filament"]["properties"]["color"]) {
    for (int i = 0; i < 4; ++i) {
      color[i] = node["filament"]["properties"]["color"][i].as<double>();
    }
    std::cout << "   color: [" << color[0] << ", " << color[1] << ", " << color[2] << ", "
      << color[3] << "]\n";
  }
  if (node["filament"]["properties"]["draw_type"]) {
    std::string draw_type_s = node["filament"]["properties"]["draw_type"].as<std::string>();
    std::cout << "   draw_type: " << draw_type_s << std::endl;
    if (draw_type_s.compare("flat") == 0) {
      draw_type = 0;
    } else if (draw_type_s.compare("orientation") == 0) {
      draw_type = 1;
    }
  }

  if (insertion_type.compare("random") == 0) {
    int nfilaments          = node["filament"]["fil"]["num"].as<int>();
    double flength          = node["filament"]["fil"]["length"].as<double>();
    double plength          = node["filament"]["fil"]["persistence_length"].as<double>();
    double max_length       = node["filament"]["fil"]["max_length"].as<double>();
    double min_length       = node["filament"]["fil"]["min_length"].as<double>();
    double max_child_length = node["filament"]["fil"]["max_child_length"].as<double>();
    double diameter         = node["filament"]["fil"]["diameter"].as<double>();

    std::cout << std::setw(25) << std::left << "   n filaments:" << std::setw(10)
      << std::left << nfilaments << std::endl;
    std::cout << std::setw(25) << std::left << "   length:" << std::setw(10)
      << std::left << flength << std::endl;
    std::cout << std::setw(25) << std::left << "   persistence length:" << std::setw(10)
      << std::left << plength << std::endl;
    std::cout << std::setw(25) << std::left << "   max length:" << std::setw(10)
      << std::left << max_length << std::endl;
    std::cout << std::setw(25) << std::left << "   min length:" << std::setw(10)
      << std::left << min_length << std::endl;
    std::cout << std::setw(25) << std::left << "   max child length:" << std::setw(10)
      << std::left << max_child_length << std::endl;
    std::cout << std::setw(25) << std::left << "   diameter:" << std::setw(10)
      << std::left << diameter << std::endl;

    params_->n_filament = nfilaments;
    params_->rod_length = flength;
    params_->persistence_length = plength;
    params_->max_rod_length = max_length;
    params_->min_rod_length = min_length;
    //params_->max_child_length = max_child_length; XXX right now uses the cell size...
    params_->rod_diameter = diameter;

    n_members_ = 0;
    for (int i = 0; i < nfilaments; ++i) {
      Filament *member = new Filament(params_, space_, gsl_rng_get(rng_.r), GetSID());
      member->Init(); 

      if (can_overlap) {
        members_.push_back(member);
        n_members_++;
      } else {
        // XXX Check for overlaps
        std::cout << "Overlap not yet implemented for filaments, exiting\n";
        exit(1);
      }
    }

  } else {
    // XXX Check for exact insertion of filaments
    std::cout << "Nope, not yet for filaments!\n";
    exit(1);
  }
  theta_validation_ = params_->theta_validation_flag ? true : false;
  diffusion_validation_ = params_->diffusion_validation_flag ? true : false;
  if (theta_validation_ && diffusion_validation_) {
    warning("Diffusion validation and theta validation are incompatible! Disabling diffusion validation!");
    diffusion_validation_ = false;
  }
  n_dim_ = params_->n_dim;
  if (theta_validation_) {
    nbins_ = params_->n_bins;
    theta_distribution_ = new int**[n_members_];
    for (int i=0; i<n_members_; ++i) {
      theta_distribution_[i] = new int*[7]; // 7 angles to record for 8 bonds
      for (int j=0; j<7; ++j) {
        theta_distribution_[i][j] = new int[nbins_];
        std::fill(theta_distribution_[i][j],theta_distribution_[i][j]+nbins_,0.0);
      }
    }
  }
  else if (diffusion_validation_) {
    nbins_ =  (int) floor(params_->n_steps/params_->n_validate);
    nvalidate_ = params_->n_validate;
    orientations_ = new double**[n_members_];
    for (int i=0; i<n_members_; ++i) {
      orientations_[i] = new double*[2];
      for (int j=0; j<2; ++j) {
        orientations_[i][j] = new double[nbins_];
      }
    }
  }
  midstep_ = true;
  ibin_ = 0;
  ivalidate_ = 0;

  std::cout << "****************\n";
  std::cout << "Filament insertion not done yet, BE CAREFUL!!!!!!\n";
  std::cout << "****************\n";
}

void FilamentSpecies::WriteThetaValidation(std::string run_name) {
  std::ostringstream file_name;
  file_name << run_name << "-thetas.log";
  std::ofstream thetas_file(file_name.str().c_str(), std::ios_base::out);
  thetas_file << "cos_theta" << " ";
  int n_theta = 1;
  for (int j_member=0; j_member<n_members_; ++j_member) {
    for (int k_angle=0; k_angle<7; ++k_angle) {
      thetas_file << "fil_" << j_member+1 << "_theta_" << n_theta++ << n_theta << " ";
    }
    n_theta = 1;
  }
  thetas_file << "\n";
  for (int i_bin=0; i_bin<nbins_; ++i_bin) {
    double angle = 2.0*i_bin/ (double) nbins_ - 1.0;
    thetas_file << angle << " ";
    for (int j_member=0; j_member<n_members_; ++j_member) {
      for (int k_angle=0; k_angle<7; ++k_angle) {
        thetas_file << theta_distribution_[j_member][k_angle][i_bin] << " ";
      }
    }
    thetas_file << "\n";
  }
}


void FilamentSpecies::WriteDiffusionValidation(std::string run_name) {
  std::ostringstream file_name;
  file_name << run_name << "-diffusion.log";
  std::ofstream diffusion_file(file_name.str().c_str(), std::ios_base::out);
  diffusion_file << "timestep ";
  for (int i_member=0; i_member<n_members_; ++i_member) {
    diffusion_file << "fil_" << i_member+1 << "_theta" << " ";
    if (n_dim_ == 3)
      diffusion_file << "fil_" << i_member+1 << "_phi" << " ";
  }
  diffusion_file << "\n";
  for (int i_bin=0; i_bin<nbins_; ++i_bin) {
    int time = i_bin*nvalidate_;
    diffusion_file << time << " ";
    for (int i_member=0; i_member<n_members_; ++i_member) {
      diffusion_file << orientations_[i_member][0][i_bin] << " ";
      if (n_dim_ == 3)
        diffusion_file << orientations_[i_member][1][i_bin] << " ";
    }
    diffusion_file << "\n";
  }
}



void FilamentSpecies::WriteOutputs(std::string run_name) {
  if (theta_validation_) {
    WriteThetaValidation(run_name);
  }
  else if (diffusion_validation_) {
    WriteDiffusionValidation(run_name);
  }
}

void FilamentSpecies::ValidateThetaDistributions() {
  int i = 0;
  for (auto it=members_.begin(); it!=members_.end(); ++it) {
    std::vector<double> const * const thetas = (*it)->GetThetas();
    for (int j=0; j<7; ++j) {
      int bin_number = (int) floor( (1 + (*thetas)[j]) * (nbins_/2) );
      // Check boundaries
      if (bin_number == nbins_)
        bin_number = nbins_-1;
      else if (bin_number == -1)
        bin_number = 0;
      // Check for nonsensical values
      else if (bin_number > nbins_ || bin_number < 0) error_exit("Something went wrong in ValidateThetaDistributions!\n");
      theta_distribution_[i][j][bin_number]++;
    }
    i++;
  }
}

void FilamentSpecies::ValidateDiffusion() {
  int i_member = 0;
  double u[3];
  for (auto it=members_.begin(); it!=members_.end(); ++it) {
    (*it)->GetAvgOrientation(u);
    if (n_dim_ == 2) {
      orientations_[i_member][0][ibin_] = acos(u[1]);
      orientations_[i_member][1][ibin_] = 0;
    }
    if (n_dim_ == 3) {
      orientations_[i_member][0][ibin_] = acos(u[2]);
      orientations_[i_member][1][ibin_] = atan2(u[1],u[0]);
    }
    i_member++;
  }
  ibin_++;
}

void Filament::GetAvgOrientation(double * au) {
  double avg_u[3] = {0.0, 0.0, 0.0};
  int size=0;
  for (auto it=elements_.begin(); it!=elements_.end(); ++it) {
    double const * const u = it->GetOrientation();
    for (int i=0; i<n_dim_; ++i)
      avg_u[i] += u[i];
    size++;
  }
  if (size == 0)
    error_exit("ERROR! Something went wrong in GetAvgOrientation!\n");
  for (int i=0; i<n_dim_; ++i)
    avg_u[i]/=size;
  std::copy(avg_u, avg_u+3, au);
}

void Filament::GetAvgPosition(double * ap) {
  double avg_p[3] = {0.0, 0.0, 0.0};
  int size=0;
  for (auto it=elements_.begin(); it!=elements_.end(); ++it) {
    double const * const p = it->GetPosition();
    for (int i=0; i<n_dim_; ++i)
      avg_p[i] += p[i];
    size++;
  }
  if (size == 0)
    error_exit("ERROR! Something went wrong in GetAvgPosition!\n");
  for (int i=0; i<n_dim_; ++i)
    avg_p[i]/=size;
  std::copy(avg_p, avg_p+3, ap);
}

void Filament::DumpAll() {
  printf("tensions:\n  {");
  for (int i=0; i<n_sites_-1; ++i)
    printf(" %5.5f ",tensions_[i]);
  printf("}\n");
  printf("cos_thetas:\n  {");
  for (int i=0; i<n_sites_-2; ++i)
    printf(" %5.5f ",cos_thetas_[i]);
  printf("}\n");
  printf("g_mat_lower:\n  {");
  for (int i=0; i<n_sites_-2; ++i)
    printf(" %5.5f ",g_mat_lower_[i]);
  printf("}\n");
  printf("g_mat_upper:\n  {");
  for (int i=0; i<n_sites_-2; ++i)
    printf(" %5.5f ",g_mat_upper_[i]);
  printf("}\n");
  printf("g_mat_diag:\n  {");
  for (int i=0; i<n_sites_-1; ++i)
    printf(" %5.5f ",g_mat_diag_[i]);
  printf("}\n");
  printf("det_t_mat:\n  {");
  for (int i=0; i<n_sites_+1; ++i)
    printf(" %5.5f ",det_t_mat_[i]);
  printf("}\n");
  printf("det_b_mat:\n  {");
  for (int i=0; i<n_sites_+1; ++i)
    printf(" %5.5f ",det_b_mat_[i]);
  printf("}\n");
  printf("h_mat_diag:\n  {");
  for (int i=0; i<n_sites_-1; ++i)
    printf(" %5.5f ",h_mat_diag_[i]);
  printf("}\n");
  printf("h_mat_upper:\n  {");
  for (int i=0; i<n_sites_-2; ++i)
    printf(" %5.5f ",h_mat_upper_[i]);
  printf("}\n");
  printf("h_mat_lower:\n  {");
  for (int i=0; i<n_sites_-2; ++i)
    printf(" %5.5f ",h_mat_lower_[i]);
  printf("}\n");
  printf("k_eff:\n  {");
  for (int i=0; i<n_sites_-2; ++i)
    printf(" %5.5f ",k_eff_[i]);
  printf("}\n\n\n");
}

