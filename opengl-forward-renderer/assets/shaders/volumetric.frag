in vec2 v_TexCoord;
out vec4 FragColor;
uniform sampler2D u_Depth;
uniform sampler2DArray u_ShadowDepth;
uniform mat4 u_InverseViewProjection;
uniform mat4 u_View;
uniform vec3 u_CameraPosition;
uniform vec3 u_LightDirection;
uniform vec3 u_LightRadiance;
uniform vec3 u_AmbientRadiance;
uniform mat4 u_ShadowMatrices[4];
uniform float u_ShadowSplits[4];
uniform bool u_HasShadows;
uniform float u_ShadowBias;
uniform float u_Density;
uniform float u_Albedo;
uniform float u_Anisotropy;
uniform float u_MaxDistance;
uniform int u_Steps;

float visibility(vec3 position)
{
    if (!u_HasShadows) return 1.0;
    float viewDepth = -(u_View * vec4(position, 1.0)).z;
    int cascade = 0;
    while (cascade < 4 && viewDepth > u_ShadowSplits[cascade]) ++cascade;
    if (cascade == 4) return 1.0;
    vec4 clip = u_ShadowMatrices[cascade] * vec4(position, 1.0);
    if (abs(clip.w) < 1e-8) return 1.0;
    vec3 uvz = clip.xyz / clip.w * 0.5 + 0.5;
    if (uvz.x < 0.0 || uvz.x >= 1.0 || uvz.y < 0.0 || uvz.y >= 1.0 ||
        uvz.z < 0.0 || uvz.z > 1.0) return 1.0;
    ivec2 size = textureSize(u_ShadowDepth, 0).xy;
    ivec2 texel = min(ivec2(uvz.xy * vec2(size)), size - 1);
    float depth = texelFetch(u_ShadowDepth, ivec3(texel, cascade), 0).r;
    return uvz.z - u_ShadowBias <= depth ? 1.0 : 0.0;
}

void main()
{
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    if (u_Density == 0.0) return;
    ivec2 size = textureSize(u_Depth, 0);
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec2 uv = (vec2(pixel) + 0.5) / vec2(size);
    float depth = texelFetch(u_Depth, pixel, 0).r;
    vec4 world = u_InverseViewProjection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec3 delta = world.xyz / world.w - u_CameraPosition;
    float surfaceDistance = length(delta);
    if (surfaceDistance < 1e-6) return;
    vec3 ray = delta / surfaceDistance;
    float distance = min(surfaceDistance, u_MaxDistance);
    float stepLength = distance / float(u_Steps);
    float tau = u_Density * stepLength;
    float stepT = exp(-tau);
    float removed = tau < 0.001 ? tau * (1.0 - 0.5 * tau + tau * tau / 6.0) : 1.0 - stepT;
    float g = u_Anisotropy;
    float denominator = 1.0 + g * g - 2.0 * g * dot(normalize(u_LightDirection), -ray);
    float phase = (1.0 - g * g) / (12.566370614359172 * denominator * sqrt(denominator));
    vec3 source = u_LightRadiance * (phase * u_Albedo * removed);
    vec3 scattered = vec3(0.0);
    float transmittance = 1.0;
    for (int sampleIndex = 0; sampleIndex < u_Steps; ++sampleIndex)
    {
        vec3 position = u_CameraPosition + ray * ((float(sampleIndex) + 0.5) * stepLength);
        scattered += transmittance * visibility(position) * source;
        transmittance *= stepT;
    }
    float totalT = exp(-u_Density * distance);
    scattered += u_AmbientRadiance * (u_Albedo * (1.0 - totalT));
    FragColor = vec4(scattered, totalT);
}
