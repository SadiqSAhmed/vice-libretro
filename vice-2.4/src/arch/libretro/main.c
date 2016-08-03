#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "machine.h"

int skel_main(int argc, char *argv[])
{

  main_program(argc, argv);

#ifndef NO_LIBCO
  vice_main_exit();
#endif
 
  return(0);
}

void vice_main_exit()
{
   machine_shutdown();
}

