#include "camera.h"
#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"
#include <SDL3/SDL.h>

void processSDLEvent(SDL_Event *e, Camera *camera)
{
    if (e->type == SDL_EVENT_KEY_DOWN)
    {
        if (e->key.scancode == SDL_SCANCODE_W) camera->velocity.Z = -1.0f;
        if (e->key.scancode == SDL_SCANCODE_S) camera->velocity.Z =  1.0f;
        if (e->key.scancode == SDL_SCANCODE_A) camera->velocity.X = -1.0f;
        if (e->key.scancode == SDL_SCANCODE_D) camera->velocity.X =  1.0f;
    }

    if (e->type == SDL_EVENT_KEY_UP)
    {
        if (e->key.scancode == SDL_SCANCODE_W) camera->velocity.Z = 0.0f;
        if (e->key.scancode == SDL_SCANCODE_S) camera->velocity.Z = 0.0f;
        if (e->key.scancode == SDL_SCANCODE_A) camera->velocity.X = 0.0f;
        if (e->key.scancode == SDL_SCANCODE_D) camera->velocity.X = 0.0f;
    }

    if (e->type == SDL_EVENT_MOUSE_MOTION)
    {
        camera->yaw   += (float)e->motion.xrel / 200.0f;
        camera->pitch -= (float)e->motion.yrel / 200.0f;

        if (camera->pitch >  1.5f) camera->pitch =  1.5f;
        if (camera->pitch < -1.5f) camera->pitch = -1.5f;
    }
}

HMM_Mat4 getRotationMatrix(Camera *camera)
{
    HMM_Quat pitchQuat = HMM_QFromAxisAngle_LH(HMM_V3(1.0f, 0.0f, 0.0f), camera->pitch);
    HMM_Quat yawQuat   = HMM_QFromAxisAngle_LH(HMM_V3(0.0f, 1.0f, 0.0f), -camera->yaw);

    HMM_Quat cameraQuat = HMM_MulQ(yawQuat, pitchQuat);

    return HMM_QToM4(cameraQuat);
}

HMM_Mat4 getViewMatrix(Camera *camera)
{
    HMM_Mat4 rotation    = getRotationMatrix(camera);
    HMM_Mat4 translation = HMM_Translate(HMM_V3(-camera->position.X, -camera->position.Y, -camera->position.Z));

    return HMM_MulM4(translation, rotation);
}

void update(Camera *camera)
{
    HMM_Mat4 cameraRotation = getRotationMatrix(camera);

    HMM_Vec4 localMove = HMM_V4(camera->velocity.X * 0.5f, 
                                camera->velocity.Y * 0.5f, 
                                camera->velocity.Z * 0.5f, 0.0f);

    HMM_Vec4 worldMove = HMM_MulM4V4(cameraRotation, localMove);

    camera->position = HMM_AddV3(camera->position, HMM_V3(worldMove.X, worldMove.Y, worldMove.Z));
}

