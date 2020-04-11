#include <cstdio>
#include <cassert>
#include <cstring>

#include <../ext/svsc/inc/svsc.h>
#include <../src/AvatPort.h>
#include <../src/AvatDebug.h>

#define FILENAME     "./out_320x240.yuv"
#define WIDTH        320
#define HEIGHT       240
#define COLORSPACE   SCMN_CS_YUV420
#define PICURE_NUM   147
#define MAX_TYPE_CNT (SVSC_ANAL_MODE_MAX + SVSC_EVAL_MODE_MAX)

static int read_img(SCMN_IMGB * img, FILE *fp)
{
	unsigned char * p;
	int i;

	if (img->cs == SCMN_CS_YUV420 || img->cs == SCMN_CS_YV12)
	{
		/* Y */
		p = (unsigned char *)img->a[0];
		for (i = 0; i < img->h[0]; i++)
		{
			fread(p, img->w[0], 1, fp);
			p += img->w[0];
		}
		/* U (V, if YV12)*/
		p = (unsigned char *)img->a[1];
		for (i = 0; i < img->h[1]; i++)
		{
			fread(p, img->w[1], 1, fp);
			p += img->w[1];
		}
		/* V (U, if YV12) */
		p = (unsigned char *)img->a[2];
		for (i = 0; i < img->h[2]; i++)
		{
			fread(p, img->w[2], 1, fp);
			p += img->w[2];
		}
	}
	else if (img->cs == SCMN_CS_RGB888 || img->cs == SCMN_CS_BGR888)
	{
		p = (unsigned char *)img->a[0];
		for (i = 0; i < img->h[0]; i++)
		{
			fread(p, img->w[0], 3, fp);
			p += img->s[0];
		}
	}
	else if (img->cs == SCMN_CS_YUV400 || img->cs == SCMN_CS_GRAY)
	{
		/* Y */
		p = (unsigned char *)img->a[0];
		for (i = 0; i < img->h[0]; i++)
		{
			fread(p, img->w[0], 1, fp);
			p += img->s[0];
		}
	}
	else
	{
		printf("read_img() Error : Unknown color space type(%d)\n", img->cs);
	}

	return 0;
}

int store_info_into_db(int * db, SVSC_COMP_OUT * out, int type, int mode,
		int img_num, int total_num)
{
	int * db_str;
	int * cost;
	int offset = (type + mode) * (total_num + SVSC_CFG_MODE_MAX) + img_num;
	db_str = db + offset;
	cost = out->cost[mode] + img_num;
	*db_str = *cost;

	return 0;
}

bool useSvsc(int *svscDB, SVSC svsc, SVSC_CFG *config, int svscType,
		int imgCount, SVSC_COMP_IN *in, SVSC_COMP_OUT *out)
{
	int ret;

	in->cur_img_num = imgCount;
	ret = svsc_process(svscType, svsc, in, out);
	ASSERT_R(ret == SVSC_OK, false);
	store_info_into_db(svscDB, out, svscType, 0, imgCount, config->cfg_tot_img_cnt);

	return true;
}

int main()
{
	int error;
	FILE *fp;
	const char *fileName = FILENAME;
	const int   picNum   = PICURE_NUM;

	int         totalRow = picNum + SVSC_CFG_MODE_MAX;
	int         *svscDB  = NEW int[totalRow * MAX_TYPE_CNT];

	ASSERT_G(svscDB != NULL, finish);
	memset(svscDB, 0, totalRow * MAX_TYPE_CNT * sizeof(int));

	fp = fopen(fileName, "rb");
	ASSERT_G(fp != NULL, delete_db);

	// start the real test part
	error = SVSC_OK;
	SVSC svsc4Blur = svsc_create(&error);
	ASSERT_G(error == SVSC_OK, close_fp);

	int svscType = SVSC_TYPE_BLUR;

	SVSC_CFG config;
	memset(&config, 0, sizeof(SVSC_CFG));

	config.cfg_width         = WIDTH;
	config.cfg_height        = HEIGHT;
	config.cfg_tot_img_cnt   = picNum;
	config.cfg_db_pos        = svscDB;

	IMG_INFO *buf4Blur = svsc_create_img(WIDTH, HEIGHT, 0, 0, COLORSPACE);
	ASSERT_G(buf4Blur != NULL, delete_svsc);

	buf4Blur->cur_img_num = 1;

	SVSC_COMP_IN  in;
	memset(&in, 0, sizeof(SVSC_COMP_IN));

	in.config      = &config;
	in.src_img     = &buf4Blur;

	SVSC_COMP_OUT out;
	memset(&out, 0, sizeof(SVSC_COMP_OUT));

	svsc_config(svscType, svsc4Blur, &in, &out);
	error = svsc_init(svscType, svsc4Blur, &in, &out);
	ASSERT_G(error == SVSC_OK, delete_img);

	for (int i = 0; i < picNum; i++) {
		read_img(buf4Blur->img, fp);

		bool cont = useSvsc(svscDB, svsc4Blur, &config, svscType, i, &in, &out);
		if (!cont) {
			break;
		}
	}

	config.cfg_set_analyze_info_done = 1;

	svsc_deinit(svscType, svsc4Blur, &in, &out);

	// dump db, no error check
	FILE *dump_fp = fopen("db.dump", "wb");
	fwrite(svscDB, 1, totalRow * MAX_TYPE_CNT * sizeof(int), dump_fp);
	fclose(dump_fp);

delete_img:
	if (*in.src_img)
		svsc_del_img(*in.src_img);
delete_svsc:
	svsc_delete(svsc4Blur);
close_fp:
	fclose(fp);
delete_db:
	delete [] svscDB;
finish:

	return 0;
}
