#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "Features.h"
#include "Estimation.h"
#include "Compositor.h"
#include <time.h>

using namespace cv;
using namespace std;
using namespace Eigen;

/* Function Implementations */
int main(int argc, char** argv)
{
	/* Panorama stitching
	
	The aim of this exercise is to learn all the different parts to image stitching. 
	These are:
	- feature detection
	- feature description
	- feature matching with NN
	- optimisation via direct linear transform
		- This may require Levenberg-Marquardt to tighten things up
	- applying transformations to images  - actual stitching

	Also:
	- bundle adjustment - yesss
	- alpha blending - poisson blending
	- using more than two views - yesss
	- parallelise it!

	The overarching aim is to learn first hand more computer vision techniques. This gives
	me a broad knowledge of a lot of skills, whilst also implementing a fun project. 
	An advantage is that we don't need camera calibration here. 

	Best if we pick two images with a relatively small baseline compared to the distance
	from the main object. 

	At the end I should comment the crap out of this for anyone reading in future

	For testing I'm using Adobe's dataset: https://sourceforge.net/projects/adobedatasets.adobe/files/adobe_panoramas.tgz/download
	Reference Implementation of FAST: https://github.com/edrosten/fast-C-src
	Panorama stitching: https://courses.engr.illinois.edu/cs543/sp2011/lectures/Lecture%2021%20-%20Photo%20Stitching%20-%20Vision_Spring2011.pdf
    http://ppwwyyxx.com/2016/How-to-Write-a-Panorama-Stitcher/#Blending

	TODO:
	- Adaptive threshold for FAST features?
	- Need to tweak threshold for Shi Tomasi
	- Tweak RANSAC threshold
	- error handling? Eh
	- Cleaning and commentary
	- Update levenberg marquardt parameter


	Issues:
	- Levenberg marquardt gets stuck?


	Log:
	- Starting with the theory of FAST features
	- implementing fast features soon i hope, iteration 1
	- FAST features basic version implemented with a threshold of 10. Do we use a dynamic
	  threshold? I'll experiment with different numbers. 15 is working for now.
	- Now score each feature with Shi-Tomasi score (then make descriptors)
	- Iteration one of scoring done, testing now. Need more tweaking
	- Got it at least working. Need a better threshold?
	- Now for feature description. HOG won't work - not orientationally invariant. SURF or SIFT ... 
	  Won't use ORB, want the more difficult route. 
	  Gonna use SIFT - http://aishack.in/tutorials/sift-scale-invariant-feature-transform-features/
	- Non-maximal suppression on features added.
	- Feature orientation added
	- SIFT descriptors added. 
	- SIFT descriptors possibly working?
	- Feature matching working? We get some matches at least
	- Going to test on other images
	- So, given that these images are probably poor for FAST corners, I'm going to
	  implement multi-scale feature detection, to see if that improves anything
	- Don't do multi-scale yet, just debug FAST features
	- Write some tests for FAST features, like check for sequential12
	- Made images grayscale, and that bloody fixed it. UGH. Was using different colour channels
	- THINGS WORK SO FAR UP TO MATCHING
	- Need to tweak Shi Tomasi threshold
	- Now to compute a Direct Linear Transform between the two images. I think
	  I can do the Szeliski algorithm, or RANSAC, or a combination? Then bundle adjust it all
	  Read Szeliski, figure out the transform
	- Homography estimation and RANSAC
	- Trying to get SVD for the homography estimation
	- Imported eigen
	- Implemented RANSAC to get best homography. Untested
	- Homography returns something, at least. RANSAC epsilon might need tuning
	- Starting compositing
	- Without scaling of homography coordinates, it all actually works!!
	- changed RANSAC criterion
	- Next on the list is Levenberg-Marquardt, and then blending,
	  although I should get normalisation working first
    - Bundle adjustment isn't working, though I think I have the wrong Jacobian ... ?
	- LM is minimising something, but I think the initial error is so wrong that we are stuck elsewhere
	  Try to get a better initial homography. So try noramlisation
	- normalisation works, bundle adjsutment is still bad
	- Try a robust cost function like Huber or Tukey. Ethan Eade is a good source here
	- unit tests for LM
	- LM works. Now add robust cost function into the mix. 
	- These seem to work fine too. So why does RANSAC produce a good homography, and
	  LM optimise well, but they can't function sequentially?
	  Plan: only RANSAC on matches that are good. Goodness of a match is rated by the distance between
	  the descriptors
	  That didn't work. How about bundle adjusting only the inliers?
	*/

	// Unit tests for optimisation
	std::vector<std::pair<Feature, Feature> > points;
	Matrix3f homography;
	/*
	This homography is for data with a zero mean and std dev of 1
	          1 5.16191e-08 2.06477e-07
1.29048e-08           1           0
1.54857e-07 2.58096e-08           1
	*/

	// Get 8 data points, perfectly done
	float num = 720;
	for (int i = 0; i < int(num); ++i)
	{
		Feature a;
		a.p = Point2f(cos(((float)i/num)*2.f*PI), sin(((float)i / num)*2.f*PI));
		Feature b;
		b.p = Point2f(cos(((float)i / num)*2.f*PI), sin(((float)i / num)*2.f*PI));
		//if (i % 2 == 0)
		//{
			float r = float(rand()) / (float(RAND_MAX) + 1.0);
			b.p.x += r * 0.01 - 0.005;
		//}

		// Some really harsh outliers
		if (i % 5 == 0)
		{
			float r = float(rand()) / (float(RAND_MAX) + 1.0);
			b.p.x += r * 0.4 - 0.2;
		}
		points.push_back(make_pair(a,b));
	}
	
	// Generate the homography between these pairs
	/*if (!FindHomography(homography, points))
	{
		cout << "Failed to find sufficiently accurate homography for matches" << endl;
		return 0;
	}*/
	cout << "Error: " << ErrorInHomography(points, homography) << endl;

	// Ok, so these points are good. Now, perturb each part of the homography by some small delta
	Matrix3f hPerturbation;
	hPerturbation << 0.1,  -0.11, 0.1,
		             -0.1, 0.5, -0.12,
		             0.11,   -0.2, 0.3;
	homography += hPerturbation;
	homography /= homography(2,2);

	cout << "Error after perturbation: " << ErrorInHomography(points, homography) << endl;

	//BundleAdjustment(points, homography);

	cout << "Error after BA: " << ErrorInHomography(points, homography) << endl;

	// Now try data with some stronger outliers?
	// some more points that are actually off?

	// Generate some perfect points
	// Assume the same 
	
	// perturb them by some small delta
	// perturb H, assuming the points correspond perfectly

	// refine them back to normal 

	// Check

	// Repeat, but create greater outliers
	// Throw in some bogus data too (these are outliers)


	//return 0;

	// pull in both images. The first is the left and the second is the right
	// In theory it doesn't actually matter though
	if (argc < 3)
	{
		cout << "Missing command line arguments!" << endl;
		cout << "Format: panorama.exe <image1.jpg> <image2.jpg>" << endl;
		exit(1);
	}
	Mat leftImage = imread(argv[1], IMREAD_GRAYSCALE);
	Mat rightImage = imread(argv[2], IMREAD_GRAYSCALE);
	
	// For debuggging
	std::string debugWindowName = "debug image";
	namedWindow(debugWindowName);

	// Find features in each image
	vector<Feature> leftFeatures;
	if (!FindFASTFeatures(leftImage, leftFeatures))
	{
		cout << "Failed to find features in left image" << endl;
		return 0;
	}
	vector<Feature> rightFeatures;
	if (!FindFASTFeatures(rightImage, rightFeatures))
	{
		cout << "Failed to find features in right image" << endl;
		return 0;
	}

#ifdef DEBUG
	// Draw the features on the image
	Mat temp = leftImage.clone();
	// Debug display
	
	Mat matchImage;
	hconcat(leftImage, rightImage, matchImage);
	int offset = leftImage.cols;
	// Draw the features on the image
	for (unsigned int i = 0; i < leftFeatures.size(); ++i)
	{
		circle(matchImage, leftFeatures[i].p, 2, (255, 255, 0), -1);
	}
	for (unsigned int i = 0; i < rightFeatures.size(); ++i)
	{
		Point p(rightFeatures[i].p.x+offset, rightFeatures[i].p.y);
		circle(matchImage, p, 2, (0, 255, 0), -1);
	}
	// Debug display
	imshow(debugWindowName, matchImage);
	waitKey(0);
#endif

	// Score features with Shi-Tomasi score, or Harris score
	std::vector<Feature> goodLeftFeatures = ScoreAndClusterFeatures(leftImage, leftFeatures);
	if (goodLeftFeatures.empty())
	{
		cout << "Failed to score and cluster features in left image" << endl;
		return 0;
	}
	std::vector<Feature> goodRightFeatures = ScoreAndClusterFeatures(rightImage, rightFeatures);
	if (goodRightFeatures.empty())
	{
		cout << "Failed to score and cluster features in right image" << endl;
		return 0;
	}

	// Sort features and cull each list to top MAC_NUM_FEATURES features
	if (goodLeftFeatures.size() > MAX_NUM_FEATURES)
	{
		sort(goodLeftFeatures.begin(), goodLeftFeatures.end(), FeatureCompare);
		goodLeftFeatures.resize(MAX_NUM_FEATURES);
	}
	if (goodRightFeatures.size() > MAX_NUM_FEATURES)
	{
		sort(goodRightFeatures.begin(), goodRightFeatures.end(), FeatureCompare);
		goodRightFeatures.resize(MAX_NUM_FEATURES);
	}

#ifdef DEBUG
	Mat matchImageScored;
	hconcat(leftImage, rightImage, matchImageScored);
	// Draw the features on the image
	for (unsigned int i = 0; i < goodLeftFeatures.size(); ++i)
	{
		circle(matchImageScored, goodLeftFeatures[i].p, 2, (255, 255, 0), -1);
	}
	for (unsigned int i = 0; i < goodRightFeatures.size(); ++i)
	{
		Point p(goodRightFeatures[i].p.x + offset, goodRightFeatures[i].p.y);
		circle(matchImageScored, p, 2, (0, 255, 0), -1);
	}
	// Debug display
	imshow(debugWindowName, matchImageScored);
	waitKey(0);
#endif

	// Create descriptors for each feature in each image
	std::vector<FeatureDescriptor> descLeft;
	if (!CreateSIFTDescriptors(leftImage, goodLeftFeatures, descLeft))
	{
		cout << "Failed to create feature descriptors for left image" << endl;
		return 0;
	}
	std::vector<FeatureDescriptor> descRight;
	if (!CreateSIFTDescriptors(rightImage, goodRightFeatures, descRight))
	{
		cout << "Failed to create feature descriptors for right image" << endl;
		return 0;
	}

	// Nearest neighbour matching with Lowe ratio test
	// The first in each pair in matches is from the left; the second, from the right. 
	std::vector<std::pair<Feature, Feature> > matches = MatchDescriptors(goodLeftFeatures, goodRightFeatures);
	cout << "Number of matches: " << matches.size() << std::endl;

	// Debug display
#ifdef DEBUG
	Mat matchImageFinal;
	hconcat(leftImage, rightImage, matchImageFinal);
	int offset = leftImage.cols;
	for (unsigned int i = 0; i < matches.size(); ++i)
	{
		Feature f1 = matches[i].first;
		Feature f2 = matches[i].second;
		f2.p.x += offset;

		circle(matchImageFinal, f1.p, 2, (255, 255, 0), -1);
		circle(matchImageFinal, f2.p, 2, (255, 255, 0), -1);
		line(matchImageFinal, f1.p, f2.p, (0,255,255), 2, 8, 0);
	}
	imshow(debugWindowName, matchImageFinal);
	waitKey(0);
#endif

	clock_t start = clock();
	Matrix3f H;
	if (!FindHomography(H, matches))
	{
		cout << "Failed to find sufficiently accurate homography for matches" << endl;
		return 0;
	}
	cout << "Homography: \n" << H << std::endl;
	start = clock() - start;
	cout << "RANSAC took " << ((float)(start)) / CLOCKS_PER_SEC << " seconds" << endl;

	// Refine the homography with bundle adjustment
	//BundleAdjustment(matches, H);

	cout << "New homography: \n" << H << std::endl;

	// Stitch the images together
	pair<int, int> size = GetFinalImageSize(leftImage, rightImage, H);
	Mat composite(size.second, size.first, CV_8U, Scalar(0));
	Stitch(leftImage, rightImage, H, composite);

//#ifdef DEBUG
	imshow(debugWindowName, composite);
	waitKey(0);
//#endif
	// Alpha blending. Poisson blending looks good here
	H;
	// TODO: multiple images
	return 0;
}

