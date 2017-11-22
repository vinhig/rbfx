#ifdef BGFX_SHADER
#include "varying_scenepass.def.sc"
#include "urho3d_compatibility.sh"
#ifdef BGFX_SHADER_TYPE_VERTEX == 1
    $input a_position, a_texcoord0, a_color0
    $output vTexCoord, vColor
#endif
#ifdef BGFX_SHADER_TYPE_FRAGMENT == 1
    $input vTexCoord, vColor
#endif

#include "common.sh"

#include "uniforms.sh"
#include "samplers.sh"
#include "transform.sh"

#else

#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"

varying vec2 vTexCoord;
varying vec4 vColor;

#endif

void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
    
    vTexCoord = iTexCoord;
    vColor = iColor;
}

void PS()
{
    vec4 diffColor = cMatDiffColor * vColor;
    vec4 diffInput = texture2D(sDiffMap, vTexCoord);
    gl_FragColor = diffColor * diffInput;
}
