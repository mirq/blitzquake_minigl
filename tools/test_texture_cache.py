#!/usr/bin/env python3
"""Exercise real binding helpers/pass setup with a CPU-only GL state model."""
import pathlib
import re
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
draw = (root / "glquake/gl_draw.c").read_text()
surf = (root / "glquake/gl_rsurf.c").read_text()


def function(name):
    start = draw.index("void " + name + " (")
    brace = draw.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (draw[end] == "{") - (draw[end] == "}")
        end += 1
    return draw[start:end]


setups = re.findall(
    r"  GL_SelectTexture\(GL_TEXTURE0_ARB\);\n"
    r"#ifndef MINIGL_DISPATCH_CLIENT\n  currenttexture = -1;\n#endif", surf)
if len(setups) != 2:
    raise SystemExit("review changed world/brush binding setup")

prefix = r'''
#include <stdio.h>
#include <stdlib.h>
typedef int GLenum;
#define GL_TEXTURE0_ARB 100
#define GL_TEXTURE1_ARB 101
#define GL_TEXTURE_2D 200
static int currenttexture = -1, cnttextures[2] = {-1,-1};
static GLenum currenttextureunit = GL_TEXTURE0_ARB;
static int active, bound[2] = {-1,-1}, calls;
static void glBindTexture(GLenum target, int texture)
{ if (target != GL_TEXTURE_2D) abort(); bound[active]=texture; ++calls; }
static void glActiveTextureARB(GLenum unit) { active=unit-GL_TEXTURE0_ARB; }
'''
test = r'''
int main(void)
{
    GL_Bind(7);
    BeginWorld(); GL_Bind(7);
    if (bound[0] != 7) return 1;
    BeginBrush(); GL_Bind(7);
    if (bound[0] != 7) return 2;
    GL_Bind(9);
    GL_SelectTexture(GL_TEXTURE1_ARB); GL_Bind(22);
    if (bound[1] != 22) return 3;
    GL_SelectTexture(GL_TEXTURE0_ARB);
    BeginWorld(); GL_Bind(9);
    if (bound[0] != 9 || bound[1] != 22) return 4;
    GL_SelectTexture(999); /* invalid selector leaves state alone */
    BeginBrush(); GL_Bind(9);
    if (active != 0 || bound[0] != 9) return 5;
#ifdef MINIGL_DISPATCH_CLIENT
    if (calls != 3) return 6;
#else
    if (calls != 7) return 7;
#endif
    printf("TEXTURE_CACHE bindings=%d final=%d/%d\n", calls, bound[0], bound[1]);
    return 0;
}
'''
source = prefix + function("GL_Bind") + function("GL_SelectTexture")
source += "\nstatic void BeginWorld(void) {\n" + setups[0] + "\n}\n"
source += "static void BeginBrush(void) {\n" + setups[1] + "\n}\n" + test
with tempfile.TemporaryDirectory(prefix="quake-bind-") as directory:
    path = pathlib.Path(directory)
    cfile = path / "check.c"
    cfile.write_text(source)
    for name, flags in (("dispatch", ["-DMINIGL_DISPATCH_CLIENT=1"]), ("classic", [])):
        output = path / name
        subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        "-fno-pie", "-no-pie", *flags, str(cfile), "-o", str(output)],
                       check=True)
        result = subprocess.run([str(output)], check=True, capture_output=True, text=True)
        if result.stderr:
            raise SystemExit(result.stderr)
        print(name, result.stdout.strip())
