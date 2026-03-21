#include "BillboardRenderer.h"

void BillboardRenderer::init() {
    float quad[16] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,
         0.5f,  0.5f, 1.0f, 1.0f
    };
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vboQuad);
    glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glGenBuffers(1, &vboInstance);
    glBindBuffer(GL_ARRAY_BUFFER, vboInstance);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceAttrib), (void*)0);
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceAttrib), (void*)(sizeof(float) * 3));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceAttrib), (void*)(sizeof(float) * 4));
    glVertexAttribDivisor(4, 1);
    glBindVertexArray(0);
}

void BillboardRenderer::uploadInstances(const std::vector<InstanceAttrib>& data) {
    glBindBuffer(GL_ARRAY_BUFFER, vboInstance);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(InstanceAttrib), data.data(), GL_DYNAMIC_DRAW);
}

void BillboardRenderer::drawInstanced(int count) {
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);
    glBindVertexArray(0);
}