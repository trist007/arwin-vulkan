#include "camera.h"
#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"
#include <SDL3/SDL.h>

void Camera::update()
{
    HMM_Mat4 cameraRotation = getRotationMatrix();

    HMM_Vec4 localMove = HMM_V4(velocity.X * 0.5f, 
                                velocity.Y * 0.5f, 
                                velocity.Z * 0.5f, 0.0f);

    HMM_Vec4 worldMove = HMM_MulM4V4(cameraRotation, localMove);

    position = HMM_AddV3(position, HMM_V3(worldMove.X, worldMove.Y, worldMove.Z));
}

void Camera::processSDLEvent(SDL_Event& e)
{
    if (e.type == SDL_EVENT_KEY_DOWN)
    {
        if (e.key.scancode == SDL_SCANCODE_W) velocity.Z = -1.0f;
        if (e.key.scancode == SDL_SCANCODE_S) velocity.Z =  1.0f;
        if (e.key.scancode == SDL_SCANCODE_A) velocity.X = -1.0f;
        if (e.key.scancode == SDL_SCANCODE_D) velocity.X =  1.0f;
    }

    if (e.type == SDL_EVENT_KEY_UP)
    {
        if (e.key.scancode == SDL_SCANCODE_W) velocity.Z = 0.0f;
        if (e.key.scancode == SDL_SCANCODE_S) velocity.Z = 0.0f;
        if (e.key.scancode == SDL_SCANCODE_A) velocity.X = 0.0f;
        if (e.key.scancode == SDL_SCANCODE_D) velocity.X = 0.0f;
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION)
    {
        yaw   += (float)e.motion.xrel / 200.0f;
        pitch -= (float)e.motion.yrel / 200.0f;

        if (pitch >  1.5f) pitch =  1.5f;
        if (pitch < -1.5f) pitch = -1.5f;
    }
}

HMM_Mat4 Camera::getRotationMatrix()
{
    HMM_Quat pitchQuat = HMM_QFromAxisAngle_LH(HMM_V3(1.0f, 0.0f, 0.0f), pitch);
    HMM_Quat yawQuat   = HMM_QFromAxisAngle_LH(HMM_V3(0.0f, 1.0f, 0.0f), -yaw);

    HMM_Quat cameraQuat = HMM_MulQ(yawQuat, pitchQuat);

    return HMM_QToM4(cameraQuat);
}

HMM_Mat4 Camera::getViewMatrix()
{
    HMM_Mat4 rotation    = getRotationMatrix();
    HMM_Mat4 translation = HMM_Translate(HMM_V3(-position.X, -position.Y, -position.Z));

    return HMM_MulM4(translation, rotation);
}