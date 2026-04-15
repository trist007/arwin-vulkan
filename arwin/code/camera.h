#include <SDL3/SDL_events.h>
#include "HandmadeMath.h"

struct Camera {
    HMM_Vec3 position;
    HMM_Vec3 velocity;

    float pitch = 0.0f;
    float yaw = 0.0f;

    void update();
    void processSDLEvent(SDL_Event& e);

    HMM_Mat4 getViewMatrix();
    HMM_Mat4 getRotationMatrix();

};