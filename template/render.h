#ifndef VIEWER_R_COMMANDS_H
#define VIEWER_R_COMMANDS_H

#include <tanto/r_geo.h>
#include "common.h"

typedef struct {
    Mat4 matModel;
    Mat4 matView;
    Mat4 matProj;
} UniformBuffer;

void  r_InitRenderer(void);
void  r_LoadMesh(Tanto_R_Mesh mesh);
void  r_ClearMesh(void);
void  r_Render(void);
void  r_CleanUp(void);
const Tanto_R_Mesh* r_GetMesh(void);

#endif /* end of include guard: R_COMMANDS_H */
