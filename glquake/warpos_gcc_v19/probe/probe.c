#include <stdio.h>
#include <proto/minigl.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("gqprobe: alive before MiniGLOpen\n");
    if (!MiniGLOpen()) { printf("MiniGLOpen failed\n"); return 20; }
    printf("MiniGLOpen ok\n");
    mglChooseWindowMode(GL_TRUE);
    mglChoosePixelDepth(16);
    mglChooseVertexBufferSize(1000);
    if (!mglCreateContext(0, 0, 320, 240)) { printf("CreateContext failed\n"); MiniGLClose(); return 20; }
    printf("context ok\n");
    glViewport(0, 0, 320, 240);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBegin(GL_TRIANGLES);
    glColor4ub(255, 64, 64, 255); glVertex3f(-0.5f, -0.5f, 0.0f);
    glColor4ub(64, 255, 96, 255); glVertex3f( 0.5f, -0.5f, 0.0f);
    glColor4ub(64, 128, 255,255); glVertex3f( 0.0f,  0.5f, 0.0f);
    glEnd();
    glFinish();
    mglSwitchDisplay();
    printf("draw ok\n");
    mglDeleteContext();
    MiniGLClose();
    printf("gqprobe: clean exit\n");
    return 0;
}
