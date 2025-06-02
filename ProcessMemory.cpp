#include <stdlib.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/types.h>

#include <QDebug>

#include "ProcessMemory.h"
#include "maps.h"


/* I shouldn't write to previous matches, instead return a new one and then compare in search_functions */
