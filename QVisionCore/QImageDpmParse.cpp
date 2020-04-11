// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#include <string.h>

#include "QImageDPM.h"
#include "QImageDpmParse.h"

#define MODEL    1
#define P        2
#define COMP     3
#define SCORE    4
#define RATIO    5
#define RFILTER  100
#define PFILTERs 101
#define PFILTER  200
#define SIZEX    150
#define SIZEY    151
#define WEIGHTS  152
#define TAGV     300
#define Vx       350
#define Vy       351
#define TAGD     400
#define Dx       451
#define Dy       452
#define Dxx      453
#define Dyy      454
#define BTAG     500

#define STEP_END 1000

#define EMODEL    (STEP_END + MODEL)
#define EP        (STEP_END + P)
#define ECOMP     (STEP_END + COMP)
#define ESCORE    (STEP_END + SCORE)
#define ERATIO    (STEP_END + RATIO)
#define ERFILTER  (STEP_END + RFILTER)
#define EPFILTERs (STEP_END + PFILTERs)
#define EPFILTER  (STEP_END + PFILTER)
#define ESIZEX    (STEP_END + SIZEX)
#define ESIZEY    (STEP_END + SIZEY)
#define EWEIGHTS  (STEP_END + WEIGHTS)
#define ETAGV     (STEP_END + TAGV)
#define EVx       (STEP_END + Vx)
#define EVy       (STEP_END + Vy)
#define ETAGD     (STEP_END + TAGD)
#define EDx       (STEP_END + Dx)
#define EDy       (STEP_END + Dy)
#define EDxx      (STEP_END + Dxx)
#define EDyy      (STEP_END + Dyy)
#define EBTAG     (STEP_END + BTAG)

using namespace std;
using namespace cv;

static int isMODEL    (char *str) {
    char stag [] = "<Model>";
    char etag [] = "</Model>";
    if (strcmp(stag, str) == 0)return  MODEL;
    if (strcmp(etag, str) == 0)return EMODEL;
    return 0;
}
static int isP        (char *str) {
    char stag [] = "<P>";
    char etag [] = "</P>";
    if (strcmp(stag, str) == 0)return  P;
    if (strcmp(etag, str) == 0)return EP;
    return 0;
}
static int isSCORE        (char *str) {
    char stag [] = "<ScoreThreshold>";
    char etag [] = "</ScoreThreshold>";
    if (strcmp(stag, str) == 0)return  SCORE;
    if (strcmp(etag, str) == 0)return ESCORE;
    return 0;
}
static int isRATIO        (char *str) {
    char stag [] = "<RatioThreshold>";
    char etag [] = "</RatioThreshold>";
    if (strcmp(stag, str) == 0)return  RATIO;
    if (strcmp(etag, str) == 0)return ERATIO;
    return 0;
}
static int isCOMP     (char *str) {
    char stag [] = "<Component>";
    char etag [] = "</Component>";
    if (strcmp(stag, str) == 0)return  COMP;
    if (strcmp(etag, str) == 0)return ECOMP;
    return 0;
}
static int isRFILTER  (char *str) {
    char stag [] = "<RootFilter>";
    char etag [] = "</RootFilter>";
    if (strcmp(stag, str) == 0)return  RFILTER;
    if (strcmp(etag, str) == 0)return ERFILTER;
    return 0;
}
static int isPFILTERs (char *str) {
    char stag [] = "<PartFilters>";
    char etag [] = "</PartFilters>";
    if (strcmp(stag, str) == 0)return  PFILTERs;
    if (strcmp(etag, str) == 0)return EPFILTERs;
    return 0;
}
static int isPFILTER  (char *str) {
    char stag [] = "<PartFilter>";
    char etag [] = "</PartFilter>";
    if (strcmp(stag, str) == 0)return  PFILTER;
    if (strcmp(etag, str) == 0)return EPFILTER;
    return 0;
}
static int isSIZEX    (char *str) {
    char stag [] = "<sizeX>";
    char etag [] = "</sizeX>";
    if (strcmp(stag, str) == 0)return  SIZEX;
    if (strcmp(etag, str) == 0)return ESIZEX;
    return 0;
}
static int isSIZEY    (char *str) {
    char stag [] = "<sizeY>";
    char etag [] = "</sizeY>";
    if (strcmp(stag, str) == 0)return  SIZEY;
    if (strcmp(etag, str) == 0)return ESIZEY;
    return 0;
}
static int isWEIGHTS  (char *str) {
    char stag [] = "<Weights>";
    char etag [] = "</Weights>";
    if (strcmp(stag, str) == 0)return  WEIGHTS;
    if (strcmp(etag, str) == 0)return EWEIGHTS;
    return 0;
}
static int isV        (char *str) {
    char stag [] = "<V>";
    char etag [] = "</V>";
    if (strcmp(stag, str) == 0)return  TAGV;
    if (strcmp(etag, str) == 0)return ETAGV;
    return 0;
}
static int isVx       (char *str) {
    char stag [] = "<Vx>";
    char etag [] = "</Vx>";
    if (strcmp(stag, str) == 0)return  Vx;
    if (strcmp(etag, str) == 0)return EVx;
    return 0;
}
static int isVy       (char *str) {
    char stag [] = "<Vy>";
    char etag [] = "</Vy>";
    if (strcmp(stag, str) == 0)return  Vy;
    if (strcmp(etag, str) == 0)return EVy;
    return 0;
}
static int isD        (char *str) {
    char stag [] = "<Penalty>";
    char etag [] = "</Penalty>";
    if (strcmp(stag, str) == 0)return  TAGD;
    if (strcmp(etag, str) == 0)return ETAGD;
    return 0;
}
static int isDx       (char *str) {
    char stag [] = "<dx>";
    char etag [] = "</dx>";
    if (strcmp(stag, str) == 0)return  Dx;
    if (strcmp(etag, str) == 0)return EDx;
    return 0;
}
static int isDy       (char *str) {
    char stag [] = "<dy>";
    char etag [] = "</dy>";
    if (strcmp(stag, str) == 0)return  Dy;
    if (strcmp(etag, str) == 0)return EDy;
    return 0;
}
static int isDxx      (char *str) {
    char stag [] = "<dxx>";
    char etag [] = "</dxx>";
    if (strcmp(stag, str) == 0)return  Dxx;
    if (strcmp(etag, str) == 0)return EDxx;
    return 0;
}
static int isDyy      (char *str) {
    char stag [] = "<dyy>";
    char etag [] = "</dyy>";
    if (strcmp(stag, str) == 0)return  Dyy;
    if (strcmp(etag, str) == 0)return EDyy;
    return 0;
}
static int isB      (char *str) {
    char stag [] = "<LinearTerm>";
    char etag [] = "</LinearTerm>";
    if (strcmp(stag, str) == 0)return  BTAG;
    if (strcmp(etag, str) == 0)return EBTAG;
    return 0;
}

static inline int getTeg(char *str) {
    int sum = isMODEL(str)+
    isP        (str)+
    isSCORE    (str)+
    isRATIO    (str)+
    isCOMP     (str)+
    isRFILTER  (str)+
    isPFILTERs (str)+
    isPFILTER  (str)+
    isSIZEX    (str)+
    isSIZEY    (str)+
    isWEIGHTS  (str)+
    isV        (str)+
    isVx       (str)+
    isVy       (str)+
    isD        (str)+
    isDx       (str)+
    isDy       (str)+
    isDxx      (str)+
    isDyy      (str)+
    isB        (str);

    return sum;
}

static void addFilter(LsvmFilter ** &filters, int &last, int &max)
{
    last++;

    if (last >= max) {
		LsvmFilter **nmodel;
		int i;

        max += 10;
        nmodel = (LsvmFilter **)malloc(sizeof(LsvmFilter *) * max);
        for (i = 0; i < last; i++)
            nmodel[i] = filters[i];

        free(filters);
        filters = nmodel;
    }

    filters[last] = new LsvmFilter;
}

static void parserRFilter(FILE *xmlf, int p, LsvmFilter *filter, float *b)
{
    int st = 0;
    int sizeX = 0, sizeY = 0;
    int tag;
    int tagVal;
    char ch;
    int i, j, ii;
    char buf[1024];
    char tagBuf[1024];
    double *data;

    filter->V.x = 0;
    filter->V.y = 0;
    filter->V.l = 0;
    filter->fineFunction[0] = 0.0;
    filter->fineFunction[1] = 0.0;
    filter->fineFunction[2] = 0.0;
    filter->fineFunction[3] = 0.0;

    i   = 0;
    j   = 0;
    st  = 0;
    tag = 0;
    while (!feof(xmlf)) {
        ch = (char)fgetc( xmlf);
        if (ch == '<') {
            tag = 1;
            j   = 1;
            tagBuf[j - 1] = ch;
        } else {
            if (ch == '>') {
                tagBuf[j    ] = ch;
                tagBuf[j + 1] = '\0';

                tagVal = getTeg(tagBuf);

                if (tagVal == ERFILTER) {
                    return;
                } else if (tagVal == SIZEX) {
                    st = 1;
                    i = 0;
                } else if (tagVal == ESIZEX) {
                    st = 0;
                    buf[i] = '\0';
                    sizeX = atoi(buf);
                    filter->sizeX = sizeX;
                } else if (tagVal == SIZEY) {
                    st = 1;
                    i = 0;
                } else if (tagVal == ESIZEY) {
                    st = 0;
                    buf[i] = '\0';
                    sizeY = atoi(buf);
                    filter->sizeY = sizeY;
                } else if (tagVal == WEIGHTS) {
                    data = (double *)malloc(sizeof(double) * p * sizeX * sizeY);
                    size_t elements_read = fread(data, sizeof(double), p * sizeX * sizeY, xmlf);
                    CV_Assert(elements_read == (size_t)(p * sizeX * sizeY));
                    filter->H = (float *)malloc(sizeof(float)* p * sizeX * sizeY);

                    for (ii = 0; ii < p * sizeX * sizeY; ii++)
                        filter->H[ii] = (float)data[ii];

                    free(data);
                } else if (tagVal == EWEIGHTS) {
                    //printf("WEIGHTS OK\n");
                } else if (tagVal == BTAG) {
                    st = 1;
                    i = 0;
                } else if (tagVal == EBTAG) {
                    st = 0;
                    buf[i] = '\0';
                    *b =(float) atof(buf);
                }

                tag = 0;
                i   = 0;
            } else {
                if ((tag == 0)&& (st == 1)) {
                    buf[i] = ch; i++;
                } else {
                    tagBuf[j] = ch; j++;
                }
            }
        }
    }
}

static void parserV(FILE *xmlf, LsvmFilter *filter)
{
    int st = 0;
    int tag;
    int tagVal;
    char ch;
    int i,j;
    char buf[1024];
    char tagBuf[1024];

    i   = 0;
    j   = 0;
    st  = 0;
    tag = 0;
    while (!feof(xmlf)) {
        ch = (char)fgetc(xmlf);
        if (ch == '<') {
            tag = 1;
            j   = 1;
            tagBuf[j - 1] = ch;
        } else {
            if (ch == '>') {
                tagBuf[j    ] = ch;
                tagBuf[j + 1] = '\0';

                tagVal = getTeg(tagBuf);

                if (tagVal == ETAGV) {
                    return;
                } else if (tagVal == Vx) {
                    st = 1;
                    i = 0;
                } else if (tagVal == EVx) {
                    st = 0;
                    buf[i] = '\0';
                    filter->V.x = atoi(buf);
                } else if (tagVal == Vy) {
                    st = 1;
                    i = 0;
                } else if (tagVal == EVy) {
                    st = 0;
                    buf[i] = '\0';
                    filter->V.y = atoi(buf);
                }
                tag = 0;
                i   = 0;
            } else {
                if ((tag == 0)&& (st == 1)) {
                    buf[i] = ch; i++;
                } else {
                    tagBuf[j] = ch; j++;
                }
            }
        }
    }
}

static void parserD(FILE *xmlf, LsvmFilter *filter)
{
    int st = 0;
    int tag;
    int tagVal;
    char ch;
    int i,j;
    char buf[1024];
    char tagBuf[1024];

    i   = 0;
    j   = 0;
    st  = 0;
    tag = 0;
    while (!feof(xmlf)) {
        ch = (char)fgetc(xmlf);
        if (ch == '<') {
            tag = 1;
            j   = 1;
            tagBuf[j - 1] = ch;
        } else {
            if (ch == '>') {
                tagBuf[j    ] = ch;
                tagBuf[j + 1] = '\0';

                tagVal = getTeg(tagBuf);

                if (tagVal == ETAGD) {
                    return;
                } else if (tagVal == Dx) {
                    st = 1;
                    i = 0;
                } else if (tagVal == EDx) {
                    st = 0;
                    buf[i] = '\0';

                    filter->fineFunction[0] = (float)atof(buf);
                } else if (tagVal == Dy) {
                    st = 1;
                    i = 0;
                } else if (tagVal == EDy) {
                    st = 0;
                    buf[i] = '\0';

                    filter->fineFunction[1] = (float)atof(buf);
                } else if (tagVal == Dxx) {
                    st = 1;
                    i = 0;
                } else if (tagVal == EDxx) {
                    st = 0;
                    buf[i] = '\0';

                    filter->fineFunction[2] = (float)atof(buf);
                } else if (tagVal == Dyy) {
                    st = 1;
                    i = 0;
                } else if (tagVal == EDyy) {
                    st = 0;
                    buf[i] = '\0';

                    filter->fineFunction[3] = (float)atof(buf);
                }

                tag = 0;
                i   = 0;
            } else {
                if ((tag == 0)&& (st == 1)) {
                    buf[i] = ch; i++;
                } else {
                    tagBuf[j] = ch; j++;
                }
            }
        }
    }
}

static void parserPFilter(FILE * xmlf, int p, LsvmFilter *filters)
{
    int st = 0;
    int sizeX=0, sizeY=0;
    int tag;
    int tagVal;
    char ch;
    int i,j, ii;
    char buf[1024];
    char tagBuf[1024];
    double *data;

    filters->V.x = 0;
    filters->V.y = 0;
    filters->V.l = 0;
    filters->fineFunction[0] = 0.0f;
    filters->fineFunction[1] = 0.0f;
    filters->fineFunction[2] = 0.0f;
    filters->fineFunction[3] = 0.0f;

    i   = 0;
    j   = 0;
    st  = 0;
    tag = 0;
    while (!feof(xmlf)) {
        ch = (char)fgetc(xmlf);
        if (ch == '<') {
            tag = 1;
            j   = 1;
            tagBuf[j - 1] = ch;
        } else {
            if (ch == '>') {
                tagBuf[j    ] = ch;
                tagBuf[j + 1] = '\0';

                tagVal = getTeg(tagBuf);

                if (tagVal == EPFILTER) {
                    return;
                } else if (tagVal == TAGV) {
                    parserV(xmlf, filters);
                } else if (tagVal == TAGD) {
                    parserD(xmlf, filters);
                } else if (tagVal == SIZEX) {
                    st = 1;
                    i = 0;
                } else if (tagVal == ESIZEX) {
                    st = 0;
                    buf[i] = '\0';
                    sizeX = atoi(buf);
                    filters->sizeX = sizeX;
                } else if (tagVal == SIZEY) {
                    st = 1;
                    i = 0;
                } else if (tagVal == ESIZEY) {
                    st = 0;
                    buf[i] = '\0';
                    sizeY = atoi(buf);
                    filters->sizeY = sizeY;
                } else if (tagVal == WEIGHTS) {
                    data = (double *)malloc(sizeof(double) * p * sizeX * sizeY);
                    size_t elements_read = fread(data, sizeof(double), p * sizeX * sizeY, xmlf);
                    CV_Assert(elements_read == (size_t)(p * sizeX * sizeY));
                    filters->H = (float *)malloc(sizeof(float)* p * sizeX * sizeY);

					for (ii = 0; ii < p * sizeX * sizeY; ii++)
                        filters->H[ii] = (float)data[ii];

					free(data);
                } else if (tagVal == EWEIGHTS) {

                }
                tag = 0;
                i   = 0;
            } else {
                if ((tag == 0)&& (st == 1)) {
                    buf[i] = ch; i++;
                } else {
                    tagBuf[j] = ch; j++;
                }
            }
        }
    }
}

static void parserPFilterS(FILE * xmlf, int p, LsvmFilter ** &filters, int &last, int &max)
{
    int st = 0;
    int N_path = 0;
    int tag;
    int tagVal;
    char ch;
    int j;
    char tagBuf[1024];

    j   = 0;
    st  = 0;
    tag = 0;
    while (!feof(xmlf)) {
        ch = (char)fgetc( xmlf);
        if (ch == '<') {
            tag = 1;
            j   = 1;
            tagBuf[j - 1] = ch;
        } else {
            if (ch == '>') {
                tagBuf[j    ] = ch;
                tagBuf[j + 1] = '\0';

                tagVal = getTeg(tagBuf);

                if (tagVal == EPFILTERs) {
                    return;
                } else if (tagVal == PFILTER) {
                    addFilter(filters, last, max);
                    parserPFilter(xmlf, p, filters[last]);
                    N_path++;
                }
                tag = 0;
            } else {
                if ((tag == 0)&& (st == 1)) {
                    //buf[i] = ch; i++;
                } else {
                    tagBuf[j] = ch; j++;
                }
            }
        }
    }
}

static void parserComp(FILE *xmlf, int p, int *N_comp, LsvmFilter ** &filters,
						float *b, int &last, int &max)
{
    int st = 0;
    int tag;
    int tagVal;
    char ch;
    int j;
    char tagBuf[1024];

    j   = 0;
    st  = 0;
    tag = 0;
    while (!feof(xmlf)) {
        ch = (char)fgetc( xmlf);
        if (ch == '<') {
            tag = 1;
            j   = 1;
            tagBuf[j - 1] = ch;
        } else {
            if (ch == '>') {
                tagBuf[j    ] = ch;
                tagBuf[j + 1] = '\0';

                tagVal = getTeg(tagBuf);

                if (tagVal == ECOMP) {
                    (*N_comp)++;
                    return;
                } else if (tagVal == RFILTER) {
                    addFilter(filters, last, max);
                    parserRFilter(xmlf, p, filters[last], b);
                } else if (tagVal == PFILTERs) {
                    parserPFilterS(xmlf, p, filters, last, max);
                }
                tag = 0;
            } else {
                if ((tag == 0)&& (st == 1)) {
                    //buf[i] = ch; i++;
                } else {
                    tagBuf[j] = ch; j++;
                }
            }
        }
    }
}

static void parserModel(FILE *xmlf, LsvmFilter ** &filters,
						int &last, int &max, int * &comp, float * &b, int *count,
						float *score, float *ratio)
{
    int p = 0; // numFeatures
    int N_comp = 0;
    int * cmp;
    float *bb;
    int st = 0;
    int tag;
    int tagVal;
    char ch;
    int i,j, ii = 0;
    char buf[1024];
    char tagBuf[1024];

    i   = 0;
    j   = 0;
    st  = 0;
    tag = 0;
    while (!feof(xmlf)) {
        ch = (char)fgetc( xmlf);
        if (ch == '<') {
            tag = 1;
            j   = 1;
            tagBuf[j - 1] = ch;
        } else {
            if (ch == '>') {
                tagBuf[j    ] = ch;
                tagBuf[j + 1] = '\0';

                tagVal = getTeg(tagBuf);

                if (tagVal == EMODEL) {
                    for (ii = 0; ii <= last; ii++)
                        filters[ii]->numFeatures = p;
                    *count = N_comp;
                    return;
                } else if (tagVal == COMP) {
                    if (N_comp == 0) {
                        cmp = (int   *)malloc(sizeof(int));
                        bb  = (float *)malloc(sizeof(float));
                        comp = cmp;
                        b    = bb;
                        *count = N_comp + 1;
                    } else {
                        cmp = (int   *)malloc(sizeof(int)   * (N_comp + 1));
                        bb  = (float *)malloc(sizeof(float) * (N_comp + 1));
                        for (ii = 0; ii < N_comp; ii++) {
                            cmp[ii] = comp[ii];
                            bb [ii] = b[ii];
                        }
                        free(comp);
                        free(b);
                        comp = cmp;
                        b    = bb;
                        * count = N_comp + 1;
                    }
                    parserComp(xmlf, p, &N_comp, filters, &b[N_comp], last, max);
                    cmp[N_comp - 1] = last;
                } else if (tagVal == P) {
                    st = 1;
                    i = 0;
                } else if (tagVal == EP) {
                    st = 0;
                    buf[i] = '\0';
                    p = atoi(buf);
                } else if (tagVal == SCORE) {
                    st = 1;
                    i = 0;
                } else if (tagVal == ESCORE) {
                    st = 0;
                    buf[i] = '\0';
                    *score = (float)atof(buf);
                } else if (tagVal == RATIO) {
                    st = 1;
                    i = 0;
                } else if (tagVal == ERATIO) {
                    st = 0;
                    buf[i] = '\0';
                    *ratio = (float)atof(buf);
                }
                tag = 0;
                i   = 0;
            } else {
                if ((tag == 0)&& (st == 1)) {
                    buf[i] = ch;
					i++;
				} else {
					tagBuf[j] = ch;
					j++;
				}
            }
        }
    }
}

int LSVMparser(const char *filename, LsvmFilter ** &filters,
			   int &last, int &max, int * &comp, float * &b, int *count,
			   float *score, float *ratio)
{
    int tag;
    char ch;
    int j;
    FILE *xmlf;
    char tagBuf[1024];

    max   = 10;
    last  = -1;
    filters = (LsvmFilter ** )malloc(sizeof(LsvmFilter *) * max);

    xmlf = fopen(filename, "rb");
    if (xmlf == NULL) {
        free(filters);
        filters = NULL;
        return LSVM_PARSER_FILE_NOT_FOUND;
    }

    j   = 0;
    tag = 0;
    while (!feof(xmlf)) {
        ch = (char)fgetc(xmlf);
        if (ch == '<') {
            tag = 1;
            j   = 1;
            tagBuf[j - 1] = ch;
        } else {
            if (ch == '>') {
                tag = 0;
                tagBuf[j    ] = ch;
                tagBuf[j + 1] = '\0';
                if (getTeg(tagBuf) == MODEL)
                    parserModel(xmlf, filters, last, max, comp, b, count, score, ratio);
            } else if (tag != 0) {
                tagBuf[j] = ch; j++;
            }
        }
    }

    fclose(xmlf);
    return LATENT_SVM_OK;
}

int QloadModel(const char *modelPath,
              LsvmFilter ** &filters,
              int *kFilters,
              int *kComponents,
              int **kPartFilters,
              float * &b,
              float *scoreThreshold,
              float *ratioThreshold)
{
    int *comp = NULL;
    int last, max, count, i;
    float score;
    int err;

    err = LSVMparser(modelPath, filters, last, max, comp, b, &count, &score, ratioThreshold);
    if (err != LATENT_SVM_OK)
        return err;

    *kFilters       = last + 1;
    *kComponents    = count;
    *scoreThreshold = score;

    *kPartFilters = (int *)malloc(sizeof(int) * count);

    for (i = 1; i < count;i++)
        (*kPartFilters)[i] = (comp[i] - comp[i - 1]) - 1;

	(*kPartFilters)[0] = comp[0];

    free(comp);

    return 0;
}
