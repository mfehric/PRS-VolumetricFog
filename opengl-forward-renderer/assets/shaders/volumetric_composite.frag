in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Fog;
void main()
{
    // Blend ONE, SRC_ALPHA: C_out = L_scattered + C_surface * T.
    FragColor = texture(u_Fog, v_TexCoord);
}
