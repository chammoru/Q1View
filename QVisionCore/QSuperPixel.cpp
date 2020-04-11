#include <map>
#include <stack>
#include <QCommon.h>
#include <QSuperPixel.h>
#include <pff_segment/segment-graph.h>

using namespace std;
using namespace cv;

void qSpSetLabel(Mat &seg, Point startPt, int nl /*new label*/)
{
	int ol = seg.at<int>(startPt); // old label
	stack<Point2i> st;
	st.push(startPt);
	while (st.size()) {
		Point2i pt = st.top(); st.pop();
		seg.at<int>(pt) = nl; // new label
		for (size_t k = 0; k < ARRAY_SIZE(qDx8); k++) {
			Point2i nPt = pt + Point2i(qDx8[k], qDy8[k]);
			if (nPt.x < 0 || nPt.x >= seg.cols ||
				nPt.y < 0 || nPt.y >= seg.rows)
				continue;
			if (seg.at<int>(nPt) == ol)
				st.push(nPt);
		}
	}
}

void qSpGet4NgbsBi(const Mat &labelMap, vector<set<int> > &neighbors)
{
	int h = labelMap.rows;
	int w = labelMap.cols;
	for (int i = 1; i < h; i++) {
		for (int j = 1; j < w; j++) {
			int cur = labelMap.at<int>(i, j);
			for (size_t k = 0; k < ARRAY_SIZE(qDx4) / 2; k++) {
				Point2i pt(j + qDx4[k], i + qDy4[k]);
				int nbr = labelMap.at<int>(pt);
				if (cur == nbr)
					continue;
				neighbors[cur].insert(nbr);
				neighbors[nbr].insert(cur);
			}
		}
	}
}

void qSpGet4NgbsUni(const Mat &labelMap, vector<set<int> > &neighbors)
{
	int h = labelMap.rows;
	int w = labelMap.cols;
	for (int i = 1; i < h - 1; i++) {
		for (int j = 1; j < w - 1; j++) {
			int cur = labelMap.at<int>(i, j);
			for (size_t k = 0; k < ARRAY_SIZE(qDx4); k++) {
				Point2i pt(j + qDx4[k], i + qDy4[k]);
				int nbr = labelMap.at<int>(pt);
				if (cur > nbr)
					neighbors[cur].insert(nbr);
			}
		}
	}
}

void qSpGetExtNgbsUni(const vector<set<int> > &neighbors, vector<set<int> > &extNeighbors)
{
	extNeighbors.resize(neighbors.size());
	for (int cur = 0; cur < int(neighbors.size()); cur++) {
		set<int>::iterator it1 = neighbors[cur].begin();
		for (; it1 != neighbors[cur].end(); it1++) {
			int nbr = *it1;
			extNeighbors[cur].insert(nbr);
			set<int>::iterator it2 = neighbors[nbr].begin();
			for (; it2 != neighbors[nbr].end(); it2++) {
				int nbrnbr = *it2;
				if (cur > nbrnbr)
					extNeighbors[cur].insert(nbrnbr);
			}
		}
	}
}

void qSpGet8NgbsBi(const Mat &labelMap, vector<set<int> > &neighbors)
{
	int h = labelMap.rows;
	int w = labelMap.cols;
	for (int i = 1; i < h; i++) {
		for (int j = 1; j < w; j++) {
			int cur = labelMap.at<int>(i, j);
			for (size_t k = 0; k < ARRAY_SIZE(qDx8) / 2; k++) {
				Point2i pt(j + qDx8[k], i + qDy8[k]);
				if (pt.x == w)
					continue;
				int nbr = labelMap.at<int>(pt);
				if (cur == nbr)
					continue;
				neighbors[cur].insert(nbr);
				neighbors[nbr].insert(cur);
			}
		}
	}
}

void qSpGet8NgbsUni(const Mat &labelMap, vector<set<int> > &neighbors)
{
	int h = labelMap.rows;
	int w = labelMap.cols;
	for (int i = 1; i < h - 1; i++) {
		for (int j = 1; j < w - 1; j++) {
			int cur = labelMap.at<int>(i, j);
			for (size_t k = 0; k < ARRAY_SIZE(qDx8); k++) {
				Point2i pt(j + qDx8[k], i + qDy8[k]);
				int nbr = labelMap.at<int>(pt);
				if (cur > nbr)
					neighbors[cur].insert(nbr);
			}
		}
	}
}

int qSpNormLabel(Mat &seg, size_t nMaxLabel)
{
	int count = 0;
	int total = int(seg.total());
	vector<int> indices(nMaxLabel, 0);
	for (int i = 0; i < total; i++)
		indices[seg.at<int>(i)]++;
	for (size_t i = 0; i < nMaxLabel; i++) {
		if (indices[i])
			indices[i] = count++;
	}
	for (int i = 0; i < total; i++)
		seg.at<int>(i) = indices[seg.at<int>(i)];
	return count;
}

int qSpSegment(const Mat &lab, Mat &labelMap, int nLabels, float c, int min_size)
{
	// get adjacent domains, four neighbors, uni-directional
	vector<set<int> > adjacent(nLabels);
	qSpGet4NgbsUni(labelMap, adjacent);

	// compute the average pixel values of every domain
	vector<Vec3f> avgImg(nLabels, Vec3f(0, 0, 0));
	vector<int> pixNum(nLabels, 0);
	for (int i = 0; i < (int)labelMap.total(); i++) {
		int label = labelMap.at<int>(i);
		avgImg[label] += lab.at<Vec3b>(i);
		pixNum[label]++;
	}
	size_t num = 0;
	for (int i = 0; i < nLabels; i++) {
		avgImg[i] /= pixNum[i];
		num += adjacent[i].size();
	}

	// compute the edges of adjacent domains
	Ptr<edge> edges = new edge[num];
	num = 0;
	for (int i = 0; i < nLabels; i++) {
		set<int>::iterator it = adjacent[i].begin();
		for (; it != adjacent[i].end(); it++) {
			int j = *it;
			edges[num].a = i;
			edges[num].b = j;
			edges[num].w = (float)norm(avgImg[i], avgImg[j], NORM_L2SQR);
			// The OpenCV 3.0 has a problem in the next code (3.1 is OK)
			// norm(avgImg[i] - avgImg[adjacent[i][j]], NORM_L2SQR);
			num++;
		}
	}

	// segment
	universe *u = segment_graph(nLabels, (int)num, edges, c);
	for (size_t i = 0; i < num; i++) { // post process small components
		int a = u->find(edges[i].a);
		int b = u->find(edges[i].b);
		if ((a != b) && ((u->size(a) < min_size) || (u->size(b) < min_size)))
			u->join(a, b);
	}
	int num_css = u->num_sets();

	// compute the indexes of superpixels in idx_img
	vector<int> indexes(nLabels, 0);
	num = 1;
	for (int i = 0; i < nLabels; i++) {
		int comp = u->find(i);
		if (!indexes[comp])
			indexes[comp] = (int)num++;
		pixNum[i] = indexes[comp] - 1; // pixNum now contains new index
	}

	// re-label
	for (int i = 0; i < (int)labelMap.total(); i++)
		labelMap.at<int>(i) = pixNum[labelMap.at<int>(i)];

	return num_css;
}

void qSp2Box(const Mat &labelMap, size_t nLabels, int w, int h, vector<Vec4i> &boxes)
{
	boxes.resize(nLabels, Vec4i(w - 1, h - 1, 0, 0));
	int idx = 0;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int label = labelMap.at<int>(idx++);
			boxes[label][0] = QMIN(boxes[label][0], x);
			boxes[label][1] = QMIN(boxes[label][1], y);
			boxes[label][2] = QMAX(boxes[label][2], x);
			boxes[label][3] = QMAX(boxes[label][3], y);
		}
	}
}

void qSpGetCentroid(const Mat &labelMap, size_t nLabels, vector<Point2d> &centroid)
{
	centroid.resize(nLabels, Point2d(0, 0));
	vector<int> pixelCount(nLabels, 0);

	for (int i = 0; i < labelMap.rows; i++) {
		for (int j = 0; j < labelMap.cols; j++) {
			int l = labelMap.at<int>(i, j);
			pixelCount[l]++;
			centroid[l] += Point2d(j, i);
		}
	}

	for (int l = 0; l < nLabels; l++) {
		int pixelCntInCell = pixelCount[l];
		centroid[l].x /= pixelCntInCell;
		centroid[l].y /= pixelCntInCell;
	}
}
