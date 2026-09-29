#include "engine/gl.h"

#include <SDL3/SDL.h>

namespace gl {

#define GL_DEFINE_FUNCTION(ret, name, params) PFN_##name name = nullptr;
GL_FUNCTION_LIST(GL_DEFINE_FUNCTION)
#undef GL_DEFINE_FUNCTION

bool load() {
    bool ok = true;

#define GL_LOAD_FUNCTION(ret, name, params)                                           \
    name = reinterpret_cast<PFN_##name>(SDL_GL_GetProcAddress("gl" #name));          \
    if (!name) {                                                                      \
        SDL_Log("Missing OpenGL function: gl" #name);                                 \
        ok = false;                                                                   \
    }
    GL_FUNCTION_LIST(GL_LOAD_FUNCTION)
#undef GL_LOAD_FUNCTION

    return ok;
}

}  // namespace gl
