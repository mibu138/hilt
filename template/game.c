#include "game.h"
#include "coal/m_math.h"
#include "coal/util.h"
#include "render.h"
#include "common.h"
#include "tanto/t_utils.h"
#include <coal/coal.h>
#include <assert.h>
#include <string.h>
#include <tanto/t_def.h>
#include <tanto/i_input.h>
#include <tanto/u_ui.h>
#include <tanto/s_scene.h>


static Vec2  mousePos;
Parms parms; 
struct ShaderParms* pShaderParms;

Tanto_U_Widget* slider0;

static Tanto_S_Scene   scene;

static float t;

static void updateCamera(float dt)
{
    Vec3 pos = (Vec3){0, 1, 2};
    pos = m_RotateY_Vec3(dt, &pos);
    Mat4 m = m_LookAt(&pos, &(Vec3){0, 0, 0}, &(Vec3){0, 1, 0});
    scene.camera.xform = m;
    scene.dirt |= TANTO_S_CAMERA_BIT;
}

static void updateLights(void)
{
    static float lastVal;
    const  float curVal = slider0->data.slider.sliderPos;
    if (curVal != lastVal)
    {
        scene.lights[0].intensity = curVal;
        scene.dirt |= TANTO_S_LIGHTS_BIT;
    }
    lastVal = curVal;
}

void g_Init(void)
{
    parms.shouldRun = true;
    t = 0.0;
    slider0 = tanto_u_CreateSlider(0, 40, NULL);
    tanto_s_CreateSimpleScene(&scene);
    r_BindScene(&scene);
}

bool g_Responder(const Tanto_I_Event *event)
{
    switch (event->type) 
    {
        case TANTO_I_KEYDOWN: switch (event->data.keyCode)
        {
            case TANTO_KEY_ESC: parms.shouldRun = false; break;
            default: return true;
        } break;
        case TANTO_I_KEYUP:   switch (event->data.keyCode)
        {
            default: return true;
        } break;
        case TANTO_I_MOTION: 
        {
            mousePos.x = (float)event->data.mouseData.x / TANTO_WINDOW_WIDTH;
            mousePos.y = (float)event->data.mouseData.y / TANTO_WINDOW_HEIGHT;
        } break;
        case TANTO_I_MOUSEDOWN: 
        {
        } break;
        case TANTO_I_MOUSEUP:
        {
        } break;
        default: break;
    }
    return false;
}

// currently game has ownership of the scene.
// therefore its this modules job to dirty and clean the scene on each frame
void g_Update(void)
{
    scene.dirt = 0; // clean
    t += 0.016;
    updateCamera(t);
    updateLights();
}

