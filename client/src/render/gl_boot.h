#pragma once
// the things a host that brings its own gl headers (qt, in the plasma plugin) needs from us without seeing glad:
// load the gl function pointers from its context, know what it got, and clear a framebuffer. nothing here includes glad/gl.h.
namespace chaos::glboot {
using ProcLoader = void* (*)(const char* name);
int load(ProcLoader getProc);                                    // gladLoadGL through the host's getProcAddress. returns major*10+minor, 0 on failure
constexpr int kNeeded = 43;                                      // the shaders are #version 430
void clearFramebuffer(unsigned fbo, float r, float g, float b);  // bind fbo, clear colour + depth
}
