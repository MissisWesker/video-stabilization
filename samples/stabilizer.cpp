#include "stabilizer.hpp"
#include <cmath>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/video/video.hpp"


bool Stabilizer::init( const cv::Mat& frame)
{
    prevFrame = frame.clone();
    cv::Mat gray4cor;
    cv::cvtColor(frame, gray4cor, cv::COLOR_BGR2GRAY);
   
    cv::goodFeaturesToTrack(gray4cor, previousFeatures, 100, 0.1, 5);

	return true;
}

namespace
{
    template<typename T>
    T median(const std::vector<T>& x)
    {
        CV_Assert(!x.empty());
        std::vector<T> y(x);
        std::sort(y.begin(), y.end());
        return y[y.size() / 2];
    }
}

bool Stabilizer::track( const cv::Mat& frame)
{
    size_t n = previousFeatures.size();
    CV_Assert(n);
    
    // Compute optical flow in selected points.
    std::vector<cv::Point2f> currentFeatures;
    std::vector<cv::Point2f> backwardPoints; //
    std::vector<uchar> state;
    std::vector<uchar> backwardPointsState; //
    std::vector<float> error;
    std::vector<float> backwardPointsError; //
    cv::calcOpticalFlowPyrLK(prevFrame, frame, previousFeatures, currentFeatures, state, error);

    float median_error = median<float>(error);
    /*std::vector<float>sorted_err(error); 
    std::sort(sorted_err.begin(), sorted_err.end());
    float median_error = sorted_err[sorted_err.size()/2];*/

    std::vector<cv::Point2f> good_points;
    std::vector<cv::Point2f> curr_points;
    for (size_t i = 0; i < n; ++i)
    {
        if (state[i] && (error[i] <= median_error))
        {
            good_points.push_back(previousFeatures[i]);
            curr_points.push_back(currentFeatures[i]);
        }
    }

    size_t s = good_points.size();
    CV_Assert(s == curr_points.size());

    cv::calcOpticalFlowPyrLK(frame, prevFrame, curr_points, backwardPoints, backwardPointsState, backwardPointsError);//
    size_t k = backwardPoints.size();//

    std::vector<float> diff_x(k);
    std::vector<float> diff_y(k);

    for (size_t i = 0; i < k; ++i)
    {
       diff_x[i] = backwardPoints[i].x - curr_points[i].x;
       diff_y[i] = backwardPoints[i].y - curr_points[i].y;
    }

    cv::Point2f average_shift(diff_x[k/2], diff_y[k/2]);

    for (size_t i = 0; i < k; ++i)
    {
        if ((diff_x[i] <= average_shift.x)&&(diff_y[i] <= average_shift.y))
            curr_points.erase(curr_points.begin()+ i);
    }

    // Find points shift.
    std::vector<float> shifts_x(s);//size = k
    std::vector<float> shifts_y(s);//size = k

    for (size_t i = 0; i < curr_points.size(); ++i)
    {
        shifts_x[i] = curr_points[i].x - good_points[i].x;
        shifts_y[i] = curr_points[i].y - good_points[i].y;
    }

    std::sort(shifts_x.begin(), shifts_x.end());
    std::sort(shifts_y.begin(), shifts_y.end());
    
    // Find median shift.
    cv::Point2f median_shift(shifts_x[s / 2], shifts_y[s / 2]);

    printf("%.4f %.4f\n", median_shift.x, median_shift.y);

    xshift.push_back(median_shift.x);
    yshift.push_back(median_shift.y);

    prevFrame = frame.clone();
    std::copy(currentFeatures.begin(), currentFeatures.end(), previousFeatures.begin());

    return true;
}

void Stabilizer :: generateFinalShift()
{
    double xsum = 0;
    double ysum = 0;
    double avg_x = 0;
    double avg_y = 0;
    int count = 0;
    int radius = 3;
    for(int j=-radius; j <= radius; j++)
    {
        for(int i=0; i < xshift.size(); i++)
        {
            if(i+j >= 0 && i+j < xshift.size())
            {
               xsum += xshift[i];
               ysum += yshift[i];
               count++;
            }
            
            avg_x = xsum / count;
            avg_y = ysum / count;

            xsmoothed.push_back(avg_x);    
            ysmoothed.push_back(avg_y);
        }
    }
}

void Stabilizer :: drawPlots()
{
    int plotWidth = 200; 
    int plotHeight = 200; 

    std::vector<cv::Point> plot; 

    //for(int i=0; i < xshift.size(); i++)
    //{
    //    plot.push_back(cv::Point(i,xsmoothed[i]));
    //}
    cv::Mat img;

    for(unsigned int i=1; i<xshift.size(); ++i)
    {

        cv::Point2f p1; p1.x = i-1; p1.y = plot[i-1].x;
        cv::Point2f p2; p2.x = i;   p2.y = plot[i].x;
        cv::line(img, p1, p2, 'r', 5, 8, 0);
    }

    //the image to be plotted 
   // cv::Mat img = cv::Mat::zeros(plotHeight,plotWidth, CV_8UC3); 

    cv::namedWindow("Plot", CV_WINDOW_AUTOSIZE); 
    cv::imshow("Plot", img); //display the image which is stored in the 'img' in the "MyWindow" window
    cv::waitKey(0);

}

void Stabilizer::resizeVideo(const cv::Mat& frame, int number, cv::Mat& outputFrame){
    cv::Rect rect(xsmoothed[number],ysmoothed[number],frame.size().width - maxX,frame.size().height - maxY);
    outputFrame = frame(rect);
}


void Stabilizer::caclMaxShifts(){
    int x = 0,y = 0;
    for (int i = 0 ; i < xsmoothed.size(); i++){
        if (abs(xsmoothed[i] - xshift[i]) > x){
            x = abs(xsmoothed[i] - xshift[i]);
        }
        if (abs(ysmoothed[i] - yshift[i]) > y){
            y = abs(ysmoothed[i] - yshift[i]);
        }
    }
    maxX = x; maxY = y;
}
