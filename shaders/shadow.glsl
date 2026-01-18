float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective
    float closestDepth = texture(shadow_map, projCoords.xy).r;
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // calculate bias to prevent shadow acne
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    // check whether current fragment is in shadow
    float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;

    // keep object visible if it is out of the light's frustum
    if(projCoords.z > 1.0)
        shadow = 0.0;

    return shadow;
}
