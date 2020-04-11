#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "QCommon.h"
#include "QDebug.h"

#include "QImageViewerCmn.h"
#include "qimage_cs.h"

int qimage_parse_num(const char *str, int *num)
{
	int ret = 0;
	long val;
	char *endptr;

	val = strtol(str, &endptr, 10);

	/* check for various possible errors */
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
			|| (errno != 0 && val == 0))
	{
		perror("strtol");

		ret = -EINVAL;
		goto failed_strtol;
	}

	if (endptr == str)
	{
		LOGWRN("no digits were found");

		ret = -EINVAL;
		goto failed_strtol;
	}

	*num = (int)val;

failed_strtol:

	return ret;
}

/*
 * @w: stride
 * @h: height
 *
 * if found, return 0
 * else return 1
 */
int qimage_parse_w_h(const char *str, int *w, int *h)
{
	unsigned int i;
	size_t len;
	int err;

	len = strlen(str);

	for (i = 1; i < len - 1; i++)
	{
		if ((str[i] == 'x') && (isdigit(str[i - 1]) && isdigit(str[i + 1])))
		{
			int j, pos = 0;

			/* parse h */
			for (j = i - 2; j > 0; j--)
			{
				if (!isdigit(str[j]))
				{
					pos = j + 1;
					break;
				}
			}

			err = qimage_parse_num(str + pos, w);
			if (err < 0)
				goto failed_parse;

			err = qimage_parse_num(str + i + 1, h);
			if (err < 0)
				goto failed_parse;

			return 0;
		}
	}

failed_parse:

	return 1;
}

int qimage_parse_arg(char *str, int *num, const char *key)
{
    char *ptr, *tmp;

    ptr = str;

    do
    {
        tmp = strstr(ptr, key);
        if (tmp == NULL || tmp == str || !isdigit((int)tmp[-1]))
            goto find_next;

        tmp--;

        while (tmp >= ptr && isdigit((int)*tmp))
            tmp--;

        qimage_parse_num(tmp + 1, num);

        return 0; /* found */

find_next:
        ptr = tmp + 2;
    } while (tmp);

    return 1; /* couldn't find */
}

static int qimage_strlen_compare(const void *a, const void *b)
{
	struct qcsc_info *l = (struct qcsc_info *)a;
	struct qcsc_info *r = (struct qcsc_info *)b;

	return strlen(l->name) < strlen(r->name);
}

void qimage_sort_cs(struct qcsc_info *ci)
{
	memcpy(ci, qcsc_info_table, sizeof(qcsc_info_table));

	qsort(ci, ARRAY_SIZE(qcsc_info_table), sizeof(struct qcsc_info),
		qimage_strlen_compare);
}

/*
 * find color space in name
 *
 * NOTE: 1. name should not be NULL
 *       2. should be used after qimage_sort_cs
 */
const struct qcsc_info * const qimage_find_cs(struct qcsc_info *ci,
											   const char *name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(qcsc_info_table); i++)
	{
		const char *buf = strstr(name, ci[i].name);

		if (buf)
			return ci + i;
	}

	return NULL;
}

const char *qimage_find_cs_name(struct qcsc_info *ci, int cs)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(qcsc_info_table); i++)
	{
		if (ci[i].cs == cs)
			return ci[i].name;
	}

	return NULL;
}

int qimage_resolution_idx(int w_ref, int h_ref)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(qresolution_info_table); i++)
	{
		int w = 0, h = 0;

		int error = qimage_parse_w_h(qresolution_info_table[i], &w, &h);
		if (!error && w_ref == w && h_ref == h)
			return i;
	}

	return -1;
}
