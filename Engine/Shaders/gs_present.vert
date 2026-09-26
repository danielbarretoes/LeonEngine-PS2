#version 330 core
// The GS emulator's presentation (Renderer: FGSOpenGLEmulator::Present): one triangle over the viewport.
void main()
{
    // One triangle over the viewport.
    vec2 Corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(Corner * 2.0 - 1.0, 0.0, 1.0);
}
