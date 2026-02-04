#version 430 core

layout(location = 0) in vec3 aPos; // Unit box [-0.5, 0.5]

struct Segment {
    vec4 p1_thickness1;
    vec4 p2_thickness2;
    vec4 color;
};

layout(std430, binding = 7) buffer Segments {
    Segment segments[];
};

uniform mat4 view;
uniform mat4 projection;

out vec3 vs_fragPos;
out vec3 vs_p1;
out vec3 vs_p2;
out float vs_r1;
out float vs_r2;
out vec3 vs_segmentColor;

void main() {
    Segment seg = segments[gl_InstanceID];
    if (seg.color.a == 0.0) {
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    vs_p1 = seg.p1_thickness1.xyz;
    vs_p2 = seg.p2_thickness2.xyz;
    vs_r1 = seg.p1_thickness1.w;
    vs_r2 = seg.p2_thickness2.w;
    vs_segmentColor = seg.color.rgb;

    vec3 dir = vs_p2 - vs_p1;
    float len = length(dir);
    vec3 center = (vs_p1 + vs_p2) * 0.5;

    float maxR = max(vs_r1, vs_r2);

    // Construct orientation matrix where Z is the segment direction
    vec3 zAxis = (len > 1e-6) ? dir / len : vec3(0, 0, 1);
    vec3 up = (abs(zAxis.y) < 0.99) ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 xAxis = normalize(cross(up, zAxis));
    vec3 yAxis = cross(zAxis, xAxis);

    mat3 rotation = mat3(xAxis, yAxis, zAxis);

    // Scale the unit box to bound the capsule
    // Width and height should be at least 2*maxR
    // Depth should be at least len + 2*maxR
    vec3 scale = vec3(maxR * 2.2, maxR * 2.2, len + maxR * 2.2);

    vec3 worldPos = rotation * (aPos * scale) + center;
    vs_fragPos = worldPos;

    gl_Position = projection * view * vec4(worldPos, 1.0);
}
