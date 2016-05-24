#ifndef _CYTOSCORE_PARAMETERS_H_
#define _CYTOSCORE_PARAMETERS_H_

// parameters.h, generated automatically using make_params

struct system_parameters {

  long seed = 7859459105545;
  int n_dim = 3;
  int n_periodic = 0;
  int boundary_type = 0;
  double system_radius = 400;
  int insert_type = 0;
  int n_filaments_attached = 0;
  int n_filaments_free = 1;
  double particle_mass = 1;
  int n_spheres = 0;
  double particle_radius = 1;
  int n_particles = 0;
  int n_steps = 100000000;
  int n_graph = 1000;
  int force_induced_catastrophe_flag = 1;
  int dynamic_instability_flag = 1;
  int error_analysis_flag = 0;
  int grab_flag = 0;
  int graph_flag = 0;
  int pair_interaction_flag = 1;
  int theta_validation_flag = 0;
  int save_state_flag = 0;
  int time_flag = 0;
  int buckling_analysis_flag = 0;
  int n_save_state = 100000;
  int n_bins = 1000;
  int metric_forces = 1;
  int n_validate = 10000;
  int n_buckle_off = 1000000;
  int n_buckle_on = 1000000;
  double delta = 0.001;
  double friction_ratio = 2;
  double sphere_radius = 50;
  double mother_daughter_dist = 680;
  double daughter_radius = 300;
  double filament_diameter = 1;
  double max_length = 1000;
  double min_length = 10;
  double min_segment_length = 3;
  double max_segment_length = 5;
  double persistence_length = 5000;
  double spring_buckling_init = 0;
  double spring_filament_sphere = 80;
  double buckle_rate = 0.001;
  int cell_list_flag = 0;
  double buckle_parameter = 0.90;
  double r_cutoff_sphere = 1.1225;
  double r_cutoff_boundary = 1.1225;
  double f_shrink_to_grow = 0;
  double f_pause_to_grow = 0.01;
  double f_shrink_to_pause = 0.01;
  double f_pause_to_shrink = 0.01;
  double f_grow_to_shrink = 0;
  double f_grow_to_pause = 0.01;
  double v_poly = 0.01;
  double v_depoly = 0.02;
  int rigid_tether_flag = 0;
  double graph_diameter = 1;
  char *grab_file;
  int position_correlation_flag = 0;
  double dimer_k_spring = 10;
  int graph_background = 0;
  double dimer_eq_length = 5;
  double dimer_length = 5;
  double br_bead_diameter = 1;
  double dimer_diameter = 1;
  int n_br_bead = 0;
  int n_dimer = 0;
  int n_md_bead = 0;
  double md_bead_mass = 1;
  double md_bead_diameter = 1;
  double cell_length = 10;
  int n_update_cells = 10;

};

#endif // _CYTOSCORE_PARAMETERS_H_