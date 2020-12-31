#ifndef VIEWER_COMMON_H
#define VIEWER_COMMON_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool    shouldRun;
    bool    reload;
} Parms;

extern Parms parms; 

#endif /* end of include guard: VIEWER_COMMON_H */

