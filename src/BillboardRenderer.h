//used for rendering billboards 

#include <glad/glad.h>
#include <vector>
#include "Particles.h"

class BillboardRenderer {
public:
	void init(); // Call this once to set up the VAO and VBOs
	void uploadInstances(const std::vector<InstanceAttrib>& data); // Call this every frame to update the instance data
	void drawInstanced(int count); // Call this to draw the billboards, 'count' is the number of instances to draw
private:
    GLuint vao = 0; 
    GLuint vboQuad = 0; 
    GLuint vboInstance = 0; 
};
