#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define IMAGE "../../DataSet/KakaoTalk_20140513_230128527.jpg"
#include "QImageDPM.h"

#define VERIFY

#if defined(VERIFY)
float truth[22] = {
	0.930403233f,  -0.118122101f, -0.245104790f, -0.292538166f, -1.18291330f,
	-1.20421219f,  -1.38423300f,  -1.45735073f,  -1.50255394f,  -1.54027367f,
	-1.54908323f,  -1.60269690f,  -1.63132620f,  -1.65922165f,  -1.67843056f,
	-1.70085001f,  -1.72598124f,  -1.73117352f,  -1.73303413f,  -1.76742029f,
	-1.87163448f,  -1.89260197f,
};
#endif

using namespace std;
using namespace cv;

int main(int argc, char **argv)
{
	// read an image
	cv::Mat image = cv::imread(IMAGE);

	std::string model = "../../ExtraData/latentsvmXml/person.xml";
	std::vector<std::string> models;

	models.push_back(model);
	QUniLsvmDetector detector(models);

	float overlapThreshold = 0.2f;
	double timeLimit = -1.; // in milli seconds

	detector.setup(image, overlapThreshold, timeLimit);

	vector<QRect> searchBoxes;
	vector<FeatureMapCoord> ftrMapCoords;
	vector<vector<FeatureMapCoord> *> ftcss;
	ftcss.push_back(&ftrMapCoords);
#if 1 // one region search
	// Ground Truth is level 26
	QRect rt(1, 195, 340, 630);
	searchBoxes.push_back(rt);

	detector.cvtBox2FtrMapCoord(&searchBoxes, &ftrMapCoords);
#elif 0 // full region, full level
	detector.genFullFtrMapCoord(&ftrMapCoords);
#elif 0 // center fisrt search
	detector.genCenteredFtrMapCoord(&ftrMapCoords);
#elif 0 // random search
	detector.genRandFtrMapCoord(&ftrMapCoords);
#else // full region, full level, no thread
#endif

	std::vector<QUniLsvmDetector::ObjectDetection> detections;
	detector.detect(detections, ftcss);

    for (size_t i = 0; i < detections.size(); i++) {
        const QUniLsvmDetector::ObjectDetection &od = detections[i];

		if (od.score < PERSON_THRESHOLD)
			continue;
        rectangle(image, od.rect, cv::Scalar(0, 255, 0), 2);
    }

#if defined(VERIFY)
	for (size_t i = 0; i < detections.size(); i++) {
		const QUniLsvmDetector::ObjectDetection& od = detections[i];

		if (truth[i] != od.score) {
			printf("different index %ld: orig[%f] changed[%f]\n", i, truth[i], od.score);
			abort();
		}
	}
#endif

	cv::namedWindow("My Image");
	// show the image on window
	cv::imshow("My Image", image);
	cv::waitKey();

    return 0;
}
