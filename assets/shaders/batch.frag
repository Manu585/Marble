#version 460 core
in vec4  v_Color;
in vec2  v_TexCoord;
in float v_TexIndex;

uniform sampler2D u_Textures[16];

out vec4 FragColor;

void main() {
    int index = int(v_TexIndex);
    FragColor  = texture(u_Textures[index], v_TexCoord) * v_Color;
}