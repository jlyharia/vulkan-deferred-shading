#version 450

// Fullscreen triangle — no vertex buffer needed.
// Generates a triangle that covers the entire screen from gl_VertexIndex (0,1,2).

layout (location = 0) out vec2 outUV;

void main() {
    // Vertices: (-1,-1), (3,-1), (-1,3) — single triangle covering clip space
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}