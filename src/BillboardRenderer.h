#pragma once
#include <glad/glad.h>
#include <vector>
#include "Particles.h"

class BillboardRenderer {
public:
    void init();
    void uploadInstances(const std::vector<InstanceAttrib>& data);
    void drawInstanced(int count);
private:
    GLuint vao = 0;
    GLuint vboQuad = 0;
    GLuint vboInstance = 0;
};
