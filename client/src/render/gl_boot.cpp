#include "render/gl_boot.h"
#include <glad/gl.h>
namespace chaos::glboot {
int load(ProcLoader getProc) {
    int v = gladLoadGL(reinterpret_cast<GLADloadfunc>(getProc));
    if (!v) return 0;
    return GLAD_VERSION_MAJOR(v) * 10 + GLAD_VERSION_MINOR(v);
}
void clearFramebuffer(unsigned fbo, float r, float g, float b) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(r, g, b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
}
