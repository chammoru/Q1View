#include <cstdio>
#include <cassert>
#include <cstring>

#include <simgp.h>
#include <svsc.h>
#include <../src/AvatPort.h>
#include <../src/AvatDebug.h>
#include <../src/ReqListener.h>

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
	size_t read;

	if (img->cs == SCMN_CS_YUV420 || img->cs == SCMN_CS_YV12)
	{
		/* Y */
		p = (unsigned char *)img->a[0];
		for (i = 0; i < img->h[0]; i++)
		{
			read = fread(p, img->w[0], 1, fp);
			p += img->w[0];
		}
		/* U (V, if YV12)*/
		p = (unsigned char *)img->a[1];
		for (i = 0; i < img->h[1]; i++)
		{
			read = fread(p, img->w[1], 1, fp);
			p += img->w[1];
		}
		/* V (U, if YV12) */
		p = (unsigned char *)img->a[2];
		for (i = 0; i < img->h[2]; i++)
		{
			read = fread(p, img->w[2], 1, fp);
			p += img->w[2];
		}
	}
	else if (img->cs == SCMN_CS_RGB888 || img->cs == SCMN_CS_BGR888)
	{
		p = (unsigned char *)img->a[0];
		for (i = 0; i < img->h[0]; i++)
		{
			read = fread(p, img->w[0], 3, fp);
			p += img->s[0];
		}
	}
	else if (img->cs == SCMN_CS_YUV400 || img->cs == SCMN_CS_GRAY)
	{
		/* Y */
		p = (unsigned char *)img->a[0];
		for (i = 0; i < img->h[0]; i++)
		{
			read = fread(p, img->w[0], 1, fp);
			p += img->s[0];
		}
	}
	else
	{
		printf("read_img() Error : Unknown color space type(%d)\n", img->cs);
	}

	return 0;
}

enum {
	kSvscImageCount = 'ssic',
};

class BufContainer {
public:
	BufContainer(int refCount) {
		mRefCount = refCount;
	}

	void atomic_dec() {
		SMutex::Autolock autoLock(mLock);
		mRefCount--;
	}

	bool notRefered() {
		return mRefCount == 0;
	}

	IMG_INFO    *buf;

private:
	SMutex       mLock;
	int          mRefCount;
};

class SvscModule : public ReqListener {
public:
	SvscModule(int svscType, const char *name, size_t queueMax = QUEUE_MAX) :
		ReqListener(name, queueMax),
		mSvscType(svscType) {
		init();
	}

	~SvscModule() {
		if (!mReady)
			return;

		if (mSvscType == SVSC_TYPE_CLUSTERING) {
			svsc_del_img(mRgbBuf);
		}

		svsc_deinit(mSvscType, mSvsc, &mSvscIn, &mSvscOut);
		svsc_delete(mSvsc);

		init();
	}

	bool setup(SVSC_CFG *config) {
		int error = SVSC_OK;

		mSvsc = svsc_create(&error);
		ASSERT_G(error == SVSC_OK, setup_failed);

		mSvscIn.config = config;

		error = svsc_config(mSvscType, mSvsc, &mSvscIn, &mSvscOut);
		ASSERT_G(error == SVSC_OK, delete_svsc);
		error = svsc_init(mSvscType, mSvsc, &mSvscIn, &mSvscOut);
		ASSERT_G(error == SVSC_OK, delete_svsc);
		if (mSvscType == SVSC_TYPE_CLUSTERING) {
			mRgbBuf = svsc_create_img(config->cfg_width, config->cfg_height,
				0, 0, SCMN_CS_RGB888);
			ASSERT_G(mRgbBuf != NULL, deinit_svsc);
		}
		mReady = true;

		return true;

deinit_svsc:
		svsc_deinit(mSvscType, mSvsc, &mSvscIn, &mSvscOut);
delete_svsc:
		svsc_delete(mSvsc);
setup_failed:
		init();

		return false;
	}

	bool organize(ReqMsg &msgRcv, ReqMsg &msgSnd) {
		bool           ret;
		int            error;
		int            imgCount;
		SParcel       *p = &msgRcv.p;
		BufContainer  *bufContainer = NULL;

		SVSC_CFG      *config = mSvscIn.config;

		ret = p->findInt(kSvscImageCount, &imgCount);
		ASSERT_G(ret, delete_img);

		LOGVBS("%15s: <-- %04d", mName.c_str(), imgCount);
		bufContainer = (BufContainer *)msgRcv.u.push_message.data;

		if (mSvscType == SVSC_TYPE_CLUSTERING) {
			IMG_INFO *buf = bufContainer->buf;

			simgp_rsz_csc(buf->img, mRgbBuf->img);
			write_img(mRgbBuf->img);
			mSvscIn.src_img = &mRgbBuf;
		} else {
			mSvscIn.src_img = &bufContainer->buf;
		}
		mSvscIn.cur_img_num = imgCount;

		error = svsc_process(mSvscType, mSvsc, &mSvscIn, &mSvscOut);
		if (mSvscType != SVSC_TYPE_CLUSTERING && error != SVSC_OK) {
			ret = false;
			LOGERR("svsc_process() failed");
			goto delete_img;
		}

		if (imgCount == config->cfg_tot_img_cnt - 1) {
			ret = false; // finish
		}

delete_img:
		bufContainer->atomic_dec();
		if (bufContainer->notRefered()) {
			svsc_del_img(bufContainer->buf);
			delete bufContainer;
		}

		return ret;
	}

private:
	void init() {
		mSvsc = NULL;
		mReady = false;
		memset(&mSvscIn, 0, sizeof(SVSC_COMP_IN));
		memset(&mSvscOut, 0, sizeof(SVSC_COMP_OUT));
	}

	SVSC          mSvsc;
	SVSC_COMP_IN  mSvscIn;
	SVSC_COMP_OUT mSvscOut;
	int           mSvscType;
	bool          mReady;
	IMG_INFO     *mRgbBuf;
};

int main()
{
	bool        ret;
	FILE        *fp;
	const char *fileName = FILENAME;
	const int   picNum   = PICURE_NUM;

	int         totalRow = picNum + SVSC_CFG_MODE_MAX;
	int         *svscDB  = NEW int[totalRow * MAX_TYPE_CNT];

	ASSERT_G(svscDB != NULL, finish);
	memset(svscDB, -1, totalRow * MAX_TYPE_CNT * sizeof(int));

	fp = fopen(fileName, "rb");
	ASSERT_G(fp != NULL, delete_db);

	SVSC_CFG config;
	memset(&config, 0, sizeof(SVSC_CFG));

	config.cfg_width         = WIDTH;
	config.cfg_height        = HEIGHT;
	config.cfg_tot_img_cnt   = picNum;
	config.cfg_db_pos        = svscDB;

	{
		// We can adjust the size of each message queue.
		SvscModule smFace(SVSC_TYPE_FACE, "SvscFace");
		SvscModule smBlur(SVSC_TYPE_BLUR, "SvscBlur");
		SvscModule smCluster(SVSC_TYPE_CLUSTERING, "SvscCluster");
		SvscModule smBnw(SVSC_TYPE_BNW, "SvscBnw");
		SvscModule smBlock(SVSC_TYPE_BLOCK, "SvscBlock");

		ret = smFace.setup(&config);
		ASSERT_G(ret, close_fp);
		ret = smBlur.setup(&config);
		ASSERT_G(ret, close_fp);
		ret = smCluster.setup(&config);
		ASSERT_G(ret, close_fp);
		ret = smBnw.setup(&config);
		ASSERT_G(ret, close_fp);
		ret = smBlock.setup(&config);
		ASSERT_G(ret, close_fp);

		// We can adjust the priority of each thread.
		smFace.run();
		smBlur.run();
		smCluster.run();
		smBnw.run();
		smBlock.run();

		for (int i = 0; i < picNum; i++) {
			IMG_INFO *buf = svsc_create_img(WIDTH, HEIGHT, 0, 0, COLORSPACE);
			if (buf == NULL) {
				LOGERR("svsc_create_img() failed");

				smFace.finish();
				smBlur.finish();
				smCluster.finish();
				smBnw.finish();
				smBlock.finish();

				break;
			}
			read_img(buf->img, fp);

			ReqMsg msg;

			// NOTE: The number of thread using a buffer
			const int refCount = 5;
			BufContainer *bufContainer = NEW BufContainer(refCount);
			if (bufContainer == NULL) {
				svsc_del_img(buf);
				LOGERR("new BufContainer(MODULES) failed");

				smFace.finish();
				smBlur.finish();
				smCluster.finish();
				smBnw.finish();
				smBlock.finish();

				break;
			}
			bufContainer->buf = buf;

			msg.type = ReqMsg::PUSH;
			msg.u.push_message.data = bufContainer;
			SParcel *p = &msg.p;

			p->setInt(kSvscImageCount, i);

			LOGVBS("%15s: --> %04d", "main", i);

			smFace.request(msg);
			smBlur.request(msg);
			smCluster.request(msg);
			smBnw.request(msg);
			smBlock.request(msg);
		}

		smFace.join();
		smBlur.join();
		smCluster.join();
		smBnw.join();
		smBlock.join();
	}

	config.cfg_set_analyze_info_done = 1;

	{
		// dump db, no error check
		FILE *dump_fp = fopen("db.dump", "wb");
		fwrite(svscDB, 1, totalRow * MAX_TYPE_CNT * sizeof(int), dump_fp);
		fclose(dump_fp);
	}

close_fp:
	fclose(fp);
delete_db:
	delete [] svscDB;
finish:

	return 0;
}

