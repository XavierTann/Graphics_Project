

#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

#define BILLBOARD_UNIFORMS \
        "uniform mat4 view;\n" \
        "uniform mat4 projection;\n" \
        "uniform vec3 camRight;\n" \
        "uniform vec3 camUp;\n"

const char *particleVertexShaderSource =
        "#version 330\n"
        BILLBOARD_UNIFORMS
        "layout (location = 0) in vec2 quadPos;\n"
        "layout (location = 1) in vec2 quadUV;\n"
        "layout (location = 2) in vec3 instPos;\n"
        "layout (location = 3) in float instSize;\n"
        "layout (location = 4) in vec4 instColor;\n"
        "out vec2 vUV;\n"
        "out vec4 vColor;\n"
        "void main()\n"
        "{\n"
        "   vec3 worldPos = instPos + camRight * quadPos.x * instSize + camUp * quadPos.y * instSize;\n"
        "   gl_Position = projection * view * vec4(worldPos, 1.0);\n"
        "   vUV = quadUV;\n"
        "   vColor = instColor;\n"
        "}\n";

const char *particleFragmentShaderSource =
        "#version 330\n"
        "in vec2 vUV;\n"
        "in vec4 vColor;\n"
        "out vec4 Fragment;\n"
        "void main(){\n"
        "   vec2 p = vUV * 2.0 - 1.0;\n"
        "   float r = length(p);\n"
        "   float alpha = clamp(1.0 - r, 0.0, 1.0);\n"
        "   alpha = alpha * alpha;\n" 
        "   Fragment = vColor * vec4(1.0, 1.0, 1.0, alpha);\n"
        "}\n";


const char* smokeFragmentShaderSource =
        "#version 330\n"
        "in vec2 vUV;\n"
        "in vec4 vColor;\n"
        "out vec4 Fragment;\n"
        "void main(){\n"
        "   vec2 p = vUV * 2.0 - 1.0;\n"
        "   float r = length(p);\n"
        "   float alpha = clamp(1.0 - r, 0.0, 1.0);\n"
        "   alpha = alpha * alpha * alpha;\n" 
        "   alpha *= vColor.a;\n"
        "   Fragment = vec4(vColor.rgb, alpha);\n"
        "}\n";

#endif //SHADERSOURCE_H
