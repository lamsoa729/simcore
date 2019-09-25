/* Error handling routine. Used exactly like printf. */

#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <iostream>

void error_exit(const char *error_msg, ...) {
  va_list args;
  va_start(args, error_msg);
  /* Print error message to standard output and exit. */
  fprintf(stderr, "\nERROR: ");
  vfprintf(stderr, error_msg, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

void warning(const char *warning_msg, ...) {
  va_list args;
  va_start(args, warning_msg);
  /* Print warning message to standard output. */
  fprintf(stderr, "\nWARNING: ");
  vfprintf(stderr, warning_msg, args);
  fprintf(stderr, "\n");
  va_end(args);
}

void break_here() {
  printf("This is a good place to put a breakpoint!\n");
}
