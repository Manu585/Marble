#version 460 core
in  vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Screen;
uniform float     u_Time;
uniform float     u_VignetteStrength;
uniform float     u_Brightness;
uniform float     u_Contrast;
uniform float     u_Saturation;

void main() {
    // v_TexCoord is [0,1] over the post-process quad, which already occupies
    // exactly the letterbox viewport, no UV remapping needed.
    vec4 color = texture(u_Screen, v_TexCoord);

    color.rgb *= u_Brightness;
    color.rgb  = (color.rgb - 0.5) * u_Contrast + 0.5;

    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    color.rgb  = mix(vec3(gray), color.rgb, u_Saturation);

    // Vignette: circular smooth falloff, darkest at corners.
    // dist is 0 at center, 1.0 at screen edge midpoints, ~1.41 at corners.
    // smoothstep gives a soft transition instead of a hard linear cutoff.
    float dist     = length(v_TexCoord - 0.5) * 2.0;
    float vignette = 1.0 - u_VignetteStrength * smoothstep(0.5, 1.3, dist);
    color.rgb     *= vignette;

    FragColor = color;
}
