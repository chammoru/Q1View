/*
 * Regenerates the committed raw-format fixtures under Tests/fixtures/raw/.
 *
 * The fixtures back the Qt viewer smoke check (Tests/run_qt_smoke.sh and the
 * CMake `q1view_qt_smoke_*` tests). Each file holds two 16x16 frames whose byte
 * layout and size come straight from the shared core's per-color-space
 * cs_load_info, so the bytes match exactly what MainWindow::openRawFile expects
 * -- no hand-computed plane sizes that could drift from the decoders.
 *
 * Run from the repo root after changing the format set or the core layout:
 *
 *   cc -std=c11 -I QCommon/inc -I QVisionCore \
 *      Tests/fixtures/gen_raw_fixtures.c \
 *      QVisionCore/qimage_cs.c QVisionCore/qimage_metrics.c \
 *      -lm -o /tmp/gen_raw_fixtures && /tmp/gen_raw_fixtures
 *
 * The output files are deterministic, so re-running produces no diff unless the
 * format set or the core layout actually changed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "QCommon.h"
#include "qimage_cs.h"

/* Representative spread across the decode paths: 8-bit planar/semi-planar YUV,
 * 10-bit planar/semi-planar YUV, packed RGB with and without alpha, packed
 * 16-bit, and single-plane grayscale. */
static const char *const kFormats[] = {
	"yuv420",
	"nv12",
	"nv21",
	"yuv420p10le",
	"p010",
	"grayscale",
	"rgb888",
	"bgr888",
	"rgba8888",
	"bgr565",
};

#define FIXTURE_W 16
#define FIXTURE_H 16
#define FIXTURE_FRAMES 2
#define OUTPUT_DIR "Tests/fixtures/raw"

static const struct qcsc_info *find_color_space(const char *name)
{
	for (size_t i = 0; i < ARRAY_SIZE(qcsc_info_table); ++i) {
		if (strcmp(qcsc_info_table[i].name, name) == 0)
			return &qcsc_info_table[i];
	}
	return NULL;
}

int main(void)
{
	int failures = 0;

	for (size_t i = 0; i < ARRAY_SIZE(kFormats); ++i) {
		const char *name = kFormats[i];
		const struct qcsc_info *cs = find_color_space(name);
		if (!cs || !cs->cs_load_info) {
			fprintf(stderr, "unknown color space: %s\n", name);
			++failures;
			continue;
		}

		int off2 = 0, off3 = 0;
		const int frameSize = cs->cs_load_info(FIXTURE_W, FIXTURE_H, &off2, &off3);
		if (frameSize <= 0) {
			fprintf(stderr, "invalid frame size for %s\n", name);
			++failures;
			continue;
		}

		const size_t total = (size_t)frameSize * FIXTURE_FRAMES;
		unsigned char *buf = malloc(total);
		if (!buf) {
			fprintf(stderr, "out of memory for %s\n", name);
			++failures;
			continue;
		}

		/* A simple deterministic gradient so the decoders process varying data
		 * rather than a flat constant, while staying reproducible. */
		for (size_t b = 0; b < total; ++b)
			buf[b] = (unsigned char)((b * 7u + 13u) & 0xFFu);

		char path[256];
		snprintf(path, sizeof(path), "%s/gradient_%dx%d.%s",
			OUTPUT_DIR, FIXTURE_W, FIXTURE_H, name);

		FILE *fp = fopen(path, "wb");
		if (!fp) {
			fprintf(stderr, "cannot write %s\n", path);
			free(buf);
			++failures;
			continue;
		}
		const size_t wrote = fwrite(buf, 1, total, fp);
		fclose(fp);
		free(buf);

		if (wrote != total) {
			fprintf(stderr, "short write for %s\n", path);
			++failures;
			continue;
		}

		printf("wrote %s (%zu bytes, %d frames of %d)\n",
			path, total, FIXTURE_FRAMES, frameSize);
	}

	if (failures) {
		fprintf(stderr, "%d fixture(s) failed\n", failures);
		return 1;
	}
	return 0;
}
