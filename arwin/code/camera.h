#include <SDL3/SDL_events.h>
#include "HandmadeMath.h"

typedef struct Camera {
    HMM_Vec3 position;
    HMM_Vec3 velocity;
    float pitch;
    float yaw;
} Camera;

void update(Camera *camera);
void processSDLEvent(SDL_Event *e, Camera *camera);
HMM_Mat4 getViewMatrix(Camera *camera);
HMM_Mat4 getRotationMatrix(Camera *camera);