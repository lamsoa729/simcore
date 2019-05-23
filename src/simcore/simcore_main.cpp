 
#include <iostream>
#include <stdlib.h>
#include "simulation_manager.h"

/*************************
   ::SimCORE Main::
   Parse commandline flags and start simulation manager
**************************/
int main(int argc, char *argv[]) {

  // Parse input flags, see parse_flags.h for documentation
  run_options run_opts = parse_opts(argc, argv);

  // Initialize manager assets
  SimulationManager sim;
  sim.InitManager(run_opts);

  // Main control function
  sim.RunManager();

  return 0;
}

