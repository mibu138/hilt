#ifndef RENDER_H
#define RENDER_H

#include <tanto/r_geo.h>
#include <tanto/s_scene.h>
#include "common.h"

void  r_InitRenderer(void);
void  r_ClearMesh(void);
void  r_Render(void);
void  r_CleanUp(void);
void  r_BindScene(const Tanto_S_Scene* scene);

#endif /* end of include guard: R_RENDER_H */
