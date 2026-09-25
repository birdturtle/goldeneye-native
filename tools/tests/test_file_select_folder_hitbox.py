#!/usr/bin/env python3
"""Compile the patched production hit test with synthetic model/projection inputs."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
PATCH = ROOT / "getv/patches/0034-file-select-folder-hitbox.patch"
lines = PATCH.read_text().splitlines()
start = next(i for i, line in enumerate(lines)
             if line.startswith("+static s32 frontCursorOnFolder("))
end = next(i for i in range(start, len(lines)) if lines[i] == "+#endif")
production = "\n".join(line[1:] for line in lines[start:end])
assert all(line.startswith("+") for line in lines[start:end])

source = r'''
#include <stdio.h>
typedef int s32;
typedef float f32;
typedef struct Model { float xmax, xmin, ymax, ymin; } Model;
struct coord2d { float f[2]; };
struct coord3d { float f[3]; };

static float observed[4];
static void modelGetXYExtents(Model *model, f32 *xmax, f32 *xmin,
                              f32 *ymax, f32 *ymin)
{
    *xmax = model->xmax; *xmin = model->xmin;
    *ymax = model->ymax; *ymin = model->ymin;
}
static void projectRectCornersTo2D(struct coord3d *position,
                                    struct coord2d *xlimits,
                                    struct coord2d *ylimits,
                                    struct coord2d *a, struct coord2d *b)
{
    observed[0] = xlimits->f[0]; observed[1] = xlimits->f[1];
    observed[2] = ylimits->f[0]; observed[3] = ylimits->f[1];
    /* Native screen projection reverses both axes in this fixture. */
    a->f[0] = 480.0f - xlimits->f[0] + position->f[0];
    b->f[0] = 480.0f - xlimits->f[1] + position->f[0];
    a->f[1] = 320.0f - ylimits->f[1] + position->f[1];
    b->f[1] = 320.0f - ylimits->f[0] + position->f[1];
}
'''+production+r'''
int main(void)
{
    Model folder = {160.0f, 100.0f, 70.0f, 30.0f};
    struct coord3d center = {{0.0f, 0.0f, 0.0f}};
    int checks = 0;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL: %s\n", #expr); return 1; } checks++; } while (0)
    CHECK(frontCursorOnFolder(&folder, &center, 350.0f, 270.0f));
    CHECK(observed[0] == 100.0f && observed[1] == 160.0f);
    CHECK(observed[2] == 30.0f && observed[3] == 70.0f);
    CHECK(frontCursorOnFolder(&folder, &center, 320.0f, 250.0f));
    CHECK(frontCursorOnFolder(&folder, &center, 380.0f, 290.0f));
    CHECK(!frontCursorOnFolder(&folder, &center, 319.0f, 270.0f));
    CHECK(!frontCursorOnFolder(&folder, &center, 381.0f, 270.0f));
    CHECK(!frontCursorOnFolder(&folder, &center, 350.0f, 249.0f));
    CHECK(!frontCursorOnFolder(&folder, &center, 350.0f, 291.0f));
    center.f[0] = -200.0f; center.f[1] = -100.0f;
    CHECK(frontCursorOnFolder(&folder, &center, 150.0f, 170.0f));
    CHECK(!frontCursorOnFolder(&folder, &center, 350.0f, 270.0f));
    printf("file-select folder hitbox: %d checks passed\n", checks);
    return 0;
}
'''

with tempfile.TemporaryDirectory() as directory:
    c_file = Path(directory) / "folder_hitbox.c"
    binary = Path(directory) / "folder_hitbox"
    c_file.write_text(source)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(c_file), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
