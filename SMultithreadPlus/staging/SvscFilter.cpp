#include <simgp.h>

#include "Defines.h"
#include "AvatDebug.h"
#include "NodeBuffer.h"
#include "ReqListener.h"
#include "SvscFilter.h"

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

	SCMN_IMGB   *mImgb;

private:
	SMutex       mLock;
	int          mRefCount;
};

class SvscModule : public ReqListener {
public:
	SvscModule(int svscType, const char *name, size_t queueMax = QUEUE_MAX) :
		ReqListener(name, queueMax),
		mSvscType(svscType),
		mpYuvBuf(&mYuvBuf) {
		init();
	}

	~SvscModule() {
		if (!mReady)
			return;

		if (mSvscType == SVSC_TYPE_CLUSTERING) {
			svsc_del_img_info(mRgbBuf);
		}

		svsc_deinit(mSvscType, mSvsc, &mSvscIn, &mSvscOut);
		svsc_delete(mSvsc);

		init();
	}

	bool setup(SVSC_CFG *config) {
		int error = SVSC_OK;
		int area;

		mSvsc = svsc_create(&error);
		ASSERT_G(error == SVSC_OK, setup_failed);

		mSvscIn.config = config;

		error = svsc_config(mSvscType, mSvsc, &mSvscIn, &mSvscOut);
		ASSERT_G(error == SVSC_OK, delete_svsc);
		error = svsc_init(mSvscType, mSvsc, &mSvscIn, &mSvscOut);
		ASSERT_G(error == SVSC_OK, delete_svsc);
		if (mSvscType == SVSC_TYPE_CLUSTERING) {
			mRgbBuf = svsc_create_img_info(config->cfg_width, config->cfg_height,
				0, 0, SCMN_CS_RGB888);
			ASSERT_G(mRgbBuf != NULL, deinit_svsc);
		}

		area = config->cfg_width * config->cfg_height;

		mYuvBuf.img_sz = area + (area >> 1);
		mYuvBuf.plane_cnt = 3;

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
		SCMN_IMGB     *img;
		SParcel       *p = &msgRcv.p;
		BufContainer  *bufContainer = NULL;

		SVSC_CFG      *config = mSvscIn.config;

		ret = p->findInt(kSvscImageCount, &imgCount);
		ASSERT_G(ret, delete_img);

		bufContainer = (BufContainer *)msgRcv.u.push_message.data;

		img = bufContainer->mImgb;
		if (mSvscType == SVSC_TYPE_CLUSTERING) {
			simgp_csc(img, mRgbBuf->img);
			mSvscIn.src_img = &mRgbBuf;
		} else {
			mYuvBuf.img = img;
			mSvscIn.src_img = &mpYuvBuf;
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
			svsc_del_img(bufContainer->mImgb);
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
		memset(&mYuvBuf, 0, sizeof(IMG_INFO));
	}

	SVSC          mSvsc;
	SVSC_COMP_IN  mSvscIn;
	SVSC_COMP_OUT mSvscOut;
	int           mSvscType;
	bool          mReady;
	IMG_INFO     *mRgbBuf;
	IMG_INFO      mYuvBuf;
	IMG_INFO     *mpYuvBuf;
};

SvscFilter::SvscFilter(char* name)
	: Filter(name),
	mPosition(NULL),
	mTableQueue(NULL)
{
}

SvscFilter::~SvscFilter()
{
	if (mTableQueue) {
		delete mTableQueue;
		mTableQueue = NULL;
	}

	if (mPosition) {
		delete [] mPosition;
		mPosition = NULL;
	}
}

AvatError SvscFilter::start(MediaConfig* mediaConfig)
{
	LOGVBS("%s", mName);
	AvatError ret = AVAT_OK;

	mFaceBinPath = mediaConfig->faceBinPath;

	int degree = mediaConfig->degree[0];
	if (degree == 90 || degree == 270) {
		mWidth  = RESOL_QVGA_H;
		mHeight = RESOL_QVGA_W;
	} else {
		mWidth  = RESOL_QVGA_W;
		mHeight = RESOL_QVGA_H;
	}

	ret = startBySide(mediaConfig);
	ASSERT_R(ret == AVAT_OK, ret);

	mTableQueue = NEW TableQueue;

	setState(STATE_STARTED);

	return ret;
}

AvatError SvscFilter::stop()
{
	LOGVBS("%s", mName);

	stopBySide();

	setState(STATE_STOPPED);

	return AVAT_OK;
}

AvatError SvscFilter::process(NodeBuffer* buffer)
{
	AvatError   error = AVAT_OK;
	NodeBuffer *svscBuffer = buffer;
	NodeBuffer  procBuffer;

	procBuffer.setFormat(FORMAT_KEY_TABLE);
	processBySide(&procBuffer);

	ASSERT_R(procBuffer.format() == FORMAT_KEY_TABLE, AVAT_FAIL);

	mTotalFrame = mTotalCount = mRemainCount = procBuffer.size();
	if (!mRemainCount) {
		return AVAT_OK;
	}

	KeyElement *keyElement = (KeyElement*)procBuffer.data();

	int totalRow = mTotalFrame + SVSC_CFG_MODE_MAX;
	int *svscDb = NEW int[totalRow * (SVSC_ANAL_MODE_MAX + SVSC_EVAL_MODE_MAX)];
	ASSERT_R(svscDb != NULL, AVAT_NO_MEMORY);

	memset(svscDb, -1, totalRow * (SVSC_ANAL_MODE_MAX + SVSC_EVAL_MODE_MAX) * sizeof(int));

	SVSC_CFG config;
	memset(&config, 0, sizeof(SVSC_CFG));

	config.cfg_width       = mWidth;
	config.cfg_height      = mHeight;
	config.cfg_tot_img_cnt = mTotalFrame;
	config.cfg_db_pos      = svscDb;

	config.svsc_face.face_detection_bin_path = (char *)mFaceBinPath;

	{
		bool ok;

		// We can adjust the size of each message queue.
		SvscModule smFace(SVSC_TYPE_FACE, "SvscFace");
		SvscModule smBlur(SVSC_TYPE_BLUR, "SvscBlur");
		SvscModule smCluster(SVSC_TYPE_CLUSTERING, "SvscCluster");
		SvscModule smBnw(SVSC_TYPE_BNW, "SvscBnw");
		SvscModule smBlock(SVSC_TYPE_BLOCK, "SvscBlock");

		ok = smFace.setup(&config);
		ASSERT_GV(ok, error, AVAT_FAIL, delete_db);
		ok = smBlur.setup(&config);
		ASSERT_GV(ok, error, AVAT_FAIL, delete_db);
		ok = smCluster.setup(&config);
		ASSERT_GV(ok, error, AVAT_FAIL, delete_db);
		ok = smBnw.setup(&config);
		ASSERT_GV(ok, error, AVAT_FAIL, delete_db);
		ok = smBlock.setup(&config);
		ASSERT_GV(ok, error, AVAT_FAIL, delete_db);

		// We can adjust the priority of each thread.
		smFace.run();
		smBlur.run();
		smCluster.run();
		smBnw.run();
		smBlock.run();

		while (mRemainCount > 0)
		{
			procBuffer.setFormat(FORMAT_IMGB);
			processBySide(&procBuffer);

			SCMN_IMGB *img = (SCMN_IMGB *)procBuffer.data();

			ReqMsg msg;

			// NOTE: The number of thread using a buffer
			const int refCount = 5;
			BufContainer *bufContainer = NEW BufContainer(refCount);
			if (bufContainer == NULL) {
				svsc_del_img(img);
				LOGERR("new BufContainer(MODULES) failed");

				smFace.finish();
				smBlur.finish();
				smCluster.finish();
				smBnw.finish();
				smBlock.finish();

				break;
			}
			bufContainer->mImgb = img;

			msg.type = ReqMsg::PUSH;
			msg.u.push_message.data = bufContainer;
			SParcel *p = &msg.p;

			p->setInt(kSvscImageCount, mTotalCount - mRemainCount);

			smFace.request(msg);
			smBlur.request(msg);
			smCluster.request(msg);
			smBnw.request(msg);
			smBlock.request(msg);

			mRemainCount--;
		}

		smFace.join();
		smBlur.join();
		smCluster.join();
		smBnw.join();
		smBlock.join();
	}

	config.cfg_set_analyze_info_done = 1;

	mPosition = NEW long long[mTotalFrame];
	for (int i = 0; i < mTotalFrame; i++) {
		mPosition[i] = keyElement[i].timeStamp;
	}

	// create and update db data
	mDbFrameTable = NEW DbTable;
	mDbFrameTable->name = TABLE_FRAME;
	mDbFrameTable->rowCount = totalRow;
	mDbFrameTable->align64 = false;
	mDbFrameTable->rowCount = mTotalFrame;
	mDbFrameTable->align64 = false;
	mTableQueue->push(mDbFrameTable); // push frame table to table queue

	mDbFaceTable = NEW DbTable;
	mDbFaceTable->name = TABLE_FACE;
	mDbFaceTable->rowCount = mTotalFrame;
	mDbFaceTable->align64 = false;

	mTableQueue->push(mDbFaceTable); // push face table to table queue

	svscBuffer->setData(mTableQueue, 1);
	svscBuffer->setFormat(FORMAT_DB_TABLE_QUEUE);
	svscBuffer->setLast(mRemainCount == 0);

delete_db:
	if (svscDb) {
		delete [] svscDb;
	}

	return error;
}

