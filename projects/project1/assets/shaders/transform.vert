#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

uniform mat4 uModel;
uniform mat4 uModelViewProjection;
uniform mat3 uNormalTransform;

// Captured before fixed-function clipping, in the ClipVertex record order.
precise out vec4 tfClip;
precise out vec3 tfWorldPosition;
precise out vec3 tfNormal;
out vec2 tfUv;

void main() {
    // TODO: apply mvp transformation for each vertices
    /*************************************** TODO BEGIN ******************************************/
    // World position comes from the model matrix alone; the clip position must keep the
    // homogeneous w so clipping still happens after this stage.
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    tfClip = uModelViewProjection * vec4(aPosition, 1.0);
    tfWorldPosition = worldPosition.xyz;
    // uNormalTransform is already inverse-transpose(model), matching transformVertices().
    tfNormal = normalize(uNormalTransform * aNormal);
    tfUv = aUv;
    gl_Position = tfClip;
    /**************************************** TODO END *******************************************/
}
