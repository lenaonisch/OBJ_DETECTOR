/* 
// Author: Juergen Gall, BIWI, ETH Zurich
// Email: gall@vision.ee.ethz.ch
*/
#pragma once
#include "stdafx.h"
#include "CRForestDetector.h"

using namespace std;

// imgDetect - vector.size == num_of_classes
void CRForestDetector::detectColor(cv::Mat img, cv::Size size, cv::Mat& imgDetect, cv::Mat& ratios) {

	int timer_regression = 0, timer_leaf_process = 0;

	// extract features
	cv::Mat vImg;
	vector<cv::Mat> vImg_old;
	int time_cv_merge = clock();
	cv::Mat vCVMerge;
	cv::Mat img_2;
	img.copyTo(img_2);
	CRPatch::extractFeatureChannels(img_2, vCVMerge);
	time_cv_merge = clock() - time_cv_merge;
	vCVMerge.release();
	//vImg.release();

	using namespace concurrency;

	int time_amp_merge = clock();
	CRPatch::extractFeatureChannels(img, vImg_old);
	int rows = size.height;
	int cols = size.width;
	int sz[] = {rows,cols};
	int channels = vImg_old.size();
	
	vImg.create(rows,cols, CV_8UC(channels)); 
	int step_output = vImg.step1();
	int step_input = vImg_old[0].step1();
	concurrency::extent<1> eOut((rows*cols*channels+3)/4);
	array_view<unsigned int, 1> vImgView (eOut, reinterpret_cast<unsigned int*>(vImg.data));
	vImgView.discard_data();

	for(int c = 0; c < channels; c++)
	{
		concurrency::extent<1> eIn((rows*cols+3)/4);
		array_view<const unsigned int, 1> inputView (eIn, reinterpret_cast<unsigned int*>(vImg_old[c].data));	

		concurrency::extent<2> e(rows, cols);
		parallel_for_each(e, [=](index<2>idx) restrict (amp)
		{
			unsigned int ch = read_uchar(inputView, idx[0], idx[1], step_input);
			//write
			int index = idx[0]*step_output+idx[1]*channels+c;
			atomic_fetch_xor(&vImgView[index >> 2], vImgView[index >> 2] & (0xFF << ((index & 0x3) << 3)));
			atomic_fetch_xor(&vImgView[index >> 2], (ch & 0xFF) << ((index & 0x3) << 3));
		});
		vImgView.synchronize();
	}
	
	time_amp_merge = clock() - time_amp_merge;

	// reset output image
	imgDetect = cv::Mat::zeros(size, CV_32FC(num_of_classes)); // CV_32FC1 !!
	int numclass2 = num_of_classes*2;
	ratios = cv::Mat::zeros(size, CV_32FC(numclass2));

/////////////must be commented
	// get pointers to feature channels
	int stepImg;
	uchar* ptFCh;
	uchar* ptFCh_row;
	ptFCh = vImg.data;
	stepImg = vImg.step1();
	//uchar** ptFCh_old     = new uchar*[vImg_old.size()];
	//uchar** ptFCh_row_old = new uchar*[vImg_old.size()];
	//for(unsigned int c=0; c<vImg_old.size(); ++c) {
	//	ptFCh_old[c] = vImg_old[c].data;
	//}
	//int stepImg_old = vImg_old[0].step1();

	// get pointer to output image
	int stepDet;
	int stepRatio;
	float* ptDet;
	float* ptRatio;
	/*for(unsigned int c=0; c < num_of_classes; ++c)
	{*/
	ptDet = (float*)imgDetect.data;
	ptRatio = (float*)ratios.data;
	//}
	stepDet = imgDetect.step1();
	stepRatio = ratios.step1();
////////////////////////////must be commented
	int xoffset = width/2;
	int yoffset = height/2;
	
	int x, y, cx, cy; // x,y top left; cx,cy center of patch
	cy = yoffset; 

	//// output image vImgDetect
	//concurrency::extent<3> e_ptDet(num_of_classes, size.height, size.width);
	//array_view<const float, 3> ptDetView(e_ptDet, imgDetect.data);
	//
	//// output matrices ratio
	//concurrency::extent<3> e_ptRatio(num_of_classes*2, size.height, size.width);
	//array_view<const float, 3> ptRatioView(e_ptRatio, ratios.data);


	//// treetable
	//int leaflen = 100; 
	//concurrency::extent<2> e2(crForest->vTrees.size(),  leaflen);
	//parallel_for_each(e2, [=](index<2>idx) restrict (amp)
	//{
	//	leafs[idx]++;
	//});


	for(y=0; y<img.rows-height; ++y, ++cy) 
	{
		// Get start of row
		//for(unsigned int c=0; c<vImg_old.size(); ++c)
		//	ptFCh_row_old[c] = ptFCh_old[c];
		//for(unsigned int c=0; c<vImg.size(); ++c)
		ptFCh_row = ptFCh;
		cx = xoffset; 
		
		for(x=0; x<img.cols-width; ++x, ++cx) 
		{					
			// regression for a single patch
			int temp = clock();
			
			vector<const LeafNode*> result;
			crForest->regression(result, ptFCh_row, stepImg, channels);

			timer_regression+=(clock()-temp);
			
			// vote for all trees (leafs) 
			temp = clock();
			for(vector<const LeafNode*>::const_iterator itL = result.begin(); itL!=result.end(); ++itL)
			{

				for (int c = 0; c < num_of_classes; c++)
				{

				// To speed up the voting, one can vote only for patches 
			        // with a probability for foreground > 0.5
			        // 
				/*if ((*itL)->pfg[c] > prob_threshold)
					{*/
						// voting weight for leaf 
						float w = (*itL)->pfg[c] / float( (*itL)->vCenter[c].size() * result.size() );
						float r = (*itL)->vRatio[c];

						// vote for all points stored in the leaf
						for(vector<cv::Point>::const_iterator it = (*itL)->vCenter[c].begin(); it!=(*itL)->vCenter[c].end(); ++it)
						{
							int x = int(cx - (*it).x + 0.5);
							int y = cy-(*it).y;
							if(y >= 0 && y < imgDetect.rows && x >= 0 && x<imgDetect.cols)
							{
								*(ptDet + y*stepDet + x*num_of_classes + c) += w;
								// ptr[row*step + col*channels + channel] = 7;
								//formula for pointer: *(ptM[mat_num] + row*step + col*channels_total + channel)
								*(ptRatio + y*stepRatio + x*numclass2 + 2*c)+=r;
								*(ptRatio + y*stepRatio + x*numclass2 + 2*c+1)+=1;
							}
						}
					//} // end if
				}
			}
			timer_leaf_process+=clock()-temp;

			// increase pointer - x
			//for(unsigned int c=0; c<vImg.size(); ++c)
			ptFCh_row+=channels;
			//for(unsigned int c=0; c<vImg_old.size(); ++c)
			//	++ptFCh_row_old[c];

		} // end for x

		// increase pointer - y
		//for(unsigned int c=0; c<vImg.size(); ++c)
			ptFCh += stepImg;
		//for(unsigned int c=0; c<vImg_old.size(); ++c)
		//	ptFCh_old[c] += stepImg_old;

	} // end for y 	

	// smooth result image
	cv::GaussianBlur(imgDetect, imgDetect, cv::Size(3,3), 0);

	// release feature channels
	//for(unsigned int c=0; c<vImg.size(); ++c)
	vImg.release();
	
	//delete[] ptFCh;
	//delete[] ptFCh_row;
	//delete ptDet;
	//delete ptRatio;
}

// img - input image
// scales - scales to detect objects with different sizes
// vImgDetect [0..scales], inside vector is for different classes
double* CRForestDetector::detectPyramid(cv::Mat img, vector<float>& scales, vector<vector<cv::Mat>>& vImgDetect, Results& result)
{	
	int cl2 = num_of_classes*2;
	// [0] - summary time
	// [1] - init. maps
	// [2] - detectColor
	// [3] - localMax function
	// [4] - max. find other operations
	// [5] - convert to Results
	double timers [10] = {0,0,0,0,0,0,0,0,0,0};
	double* t_ptr = &timers[0];
	int init_maps = 0, detecCol = 0, maxFind = 0, localmax = 0;
	if(img.channels() == 1) 
	{
		throw string_exception("Gray color images are not supported.");
	} 
	else 
	{
		timers[0] = clock();

		vImgDetect.resize(scales.size());

		vector<MaxPoint> maxs;
		int max_index = 0;
		// run detector for all scales
		for(int i=0; i < scales.size(); i++) 
		{
			init_maps = clock();
			// mats for classes and [i] scale
			cv::Mat tmps;
			vImgDetect[i].resize(num_of_classes);
			cv::Size scaled_size(int(img.cols*scales[i]+0.5),int(img.rows*scales[i]+0.5));
			//tmps.create (scaled_size, CV_32FC(num_of_classes) );

			cv::Mat cLevel (tmps.rows, tmps.cols, CV_8UC3);				
			cv::resize( img, cLevel, scaled_size);//CV_INTER_LINEAR is default
			
			cv::Mat ratios;

			init_maps = clock() - init_maps;
			timers[1] += init_maps;

			// detection
			detecCol = clock();
			detectColor(cLevel, scaled_size, tmps, ratios);
			detecCol = clock() - detecCol;
			timers[2]+=detecCol;

			int treshold = 150;

			cv::split(tmps, vImgDetect[i]);
			for (int c = 0; c<num_of_classes;c++)
				vImgDetect[i][c].convertTo(vImgDetect[i][c], CV_8UC1, out_scale);
			tmps.release();

			
			localmax = clock();
			for (int c = 0; c < num_of_classes; c++)
				localMaxima(vImgDetect[i][c], cv::Size(width_aver[c]*scales[i], height_min[c]*scales[i]), maxs, c, treshold);
			localmax = clock() - localmax;
			timers[3] = localmax;

			maxFind = clock();
			int step = ratios.step1();
			for (int k = maxs.size()-1; k>=max_index;k--)
			{
				int cl = maxs[k].class_label;
				float* ptr = (float*)ratios.data + maxs[k].point.y*step + maxs[k].point.x * cl2 + 2*cl;
				float vec[] = {*ptr, *(ptr+1)};
				maxs[k].ratio = vec[0]/(float)vec[1];
				maxs[k].point.x /= scales[i];
				maxs[k].point.y /= scales[i];
			}
			max_index = maxs.size();

			maxFind = clock()-maxFind;
			timers[4]+=maxFind;

			ratios.release();
				
			cLevel.release();
		}

		// convert to Results
		timers[5] = clock();
		for (int i = maxs.size()-1; i > 0; i--)
		{
			int cl = maxs[i].class_label;
			int w = width_aver[cl];
			int h = w*maxs[i].ratio;
			cv::Rect rect(maxs[i].point.x-w/2, maxs[i].point.y-h/2, w,h);
			for (int j = 0; j < i;j++)
			{
				if (rect.contains(maxs[j].point) && maxs[j].pf > maxs[i].pf)
				{
					maxs[i].pf = 0;
					break;
				}
			}
		}
		for (int j = 0; j < maxs.size(); j++)
		{
			if (maxs[j].pf != 0)
			{
				result.classes.push_back(maxs[j].class_label); 
				int w = width_aver[maxs[j].class_label];
				int h = w * maxs[j].ratio;
				result.rects.push_back(cv::Rect(maxs[j].point.x - w/2, maxs[j].point.y - h/2, w, h));
			}
		}
		timers[5] = clock() - timers[5];

		maxs.clear();
		//timers[0] = (double)(clock() - timers[0])/CLOCKS_PER_SEC;
		timers[0] = clock() - timers[0];

		for (int i = 0; i<10; i++)
			timers[i] /= CLOCKS_PER_SEC;
	}
	return t_ptr;
}

int CRForestDetector::maxUsedValInHistogramData(cv::Mat src)
{
	HierarhicalThreshold ht(src);
	int bins = 10;
	if (ht.ProcessImage(1, bins))
		return 255/bins*ht.thresholds_for_max_sigma[0];
	return 0;
}

bool CRForestDetector::localMaxima(cv::Mat src, cv::Size size, std::vector<MaxPoint>& locations, int class_label, int threshold)
{
	cv::Mat m0;
	cv::Mat dst(src.rows, src.cols, CV_8UC1);
	cv::Point maxLoc(0,0);

	//1.Be sure to have at least 3x3 for at least looking at 1 pixel close neighbours
	//  Also the window must be <odd>x<odd>
	//SANITYCHECK(squareSize,3,1);
	cv::Point sqrCenter ((size.width-1)/2.0, (size.height-1)/2.0);
	if (size.height % 2 == 0)
		size.height--;
	if (size.width % 2 == 0)
		size.width--;

	int half_height = size.height/2;
	int half_width = size.width/2;
	//2.Create the localWindow mask to get things done faster
	//  When we find a local maxima we will multiply the subwindow with this MASK
	//  So that we will not search for those 0 values again and again
	cv::Mat localWindowMask = cv::Mat::zeros(size,CV_8U);//boolean
	localWindowMask.at<unsigned char>(sqrCenter)=1;

	//3.Find the threshold value to threshold the image
		//this function here returns the peak of histogram of picture
		//the picture is a thresholded picture it will have a lot of zero values in it
		//so that the second boolean variable says :
		//  (boolean) ? "return peak even if it is at 0" : "return peak discarding 0"
	if (threshold == 0) //black image, no max
		return false;
	cv::threshold(src, dst, threshold, 1, CV_THRESH_TOZERO); // 5th parameter 3 === THRESH_BINARY

	//put the src in the middle of the big array
	for (int row=sqrCenter.y;row<dst.size().height-sqrCenter.y;row++)
	{
		for (int col=sqrCenter.x;col<dst.size().width-sqrCenter.x;col++)
			{
			//1.if the value is zero it can not be a local maxima
			if (dst.at<unsigned char>(row,col)==0)
				continue;
			//2.the value at (row,col) is not 0 so it can be a local maxima point
			m0 =  dst.colRange(col-sqrCenter.x,col+sqrCenter.x+1).rowRange(row-sqrCenter.y,row+sqrCenter.y+1);
			minMaxLoc(m0,NULL,NULL,NULL,&maxLoc);
			//if the maximum location of this subWindow is at center
			//it means we found the local maxima
			//so we should delete the surrounding values which lies in the subWindow area
			//hence we will not try to find if a point is at localMaxima when already found a neighbour was
			if ((maxLoc.x==sqrCenter.x)&&(maxLoc.y==sqrCenter.y))
			{
				bool skip = false;
				// check probability of other classes in area around found point
				for (vector<MaxPoint>::iterator mp = locations.begin(); mp != locations.end(); mp++)
				{
					if (cv::Rect(mp->point.x - half_width, mp->point.y - half_height, size.width, size.height).contains(cv::Point(col,row)))
					{
						if (mp->pf < dst.at<uchar>(row, col)) // if probability of previously detected max is less, replace
						{
							mp->class_label = class_label;
							mp->pf = dst.at<uchar>(row, col);
							mp->point.x = col;
							mp->point.y = row;
						}
						skip = true;
						break;
					}
				}
				if (!skip)
					locations.push_back(MaxPoint(col, row, dst.at<uchar>(row, col), class_label));
				m0 = m0.mul(localWindowMask);
								//we can skip the values that we already made 0 by the above function
				col+=sqrCenter.x;
			}
		}
	}
	return true;
}

void CRForestDetector::convertToMultiChannel(array_view<unsigned int>& outputView, vector<cv::Mat> input)
{
	int sz[] = {input[0].rows, input[0].cols};
	int rows = sz[0];
	int cols = sz[1];
	int channels = input.size();
	int step = cols*channels;
	
	for(int c = 0; c < channels; c++)
	{
		using namespace concurrency;
	
		concurrency::extent<1> eIn((rows*cols+3)/4);
		array_view<const unsigned int, 1> inputView (eIn, reinterpret_cast<unsigned int*>(input[c].data));	

		concurrency::extent<2> e(rows, cols);
		parallel_for_each(e, [=](index<2>idx) restrict (amp)
		{
			unsigned int ch = read_uchar(inputView, idx, cols);
			//write
			int index = idx[0]*step+idx[1]*channels+c;
			atomic_fetch_xor(&outputView[index >> 2], outputView[index >> 2] & (0xFF << ((index & 0x3) << 3)));
			atomic_fetch_xor(&outputView[index >> 2], (ch & 0xFF) << ((index & 0x3) << 3));
		});
	}
	outputView.synchronize();
}