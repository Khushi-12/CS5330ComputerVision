/**
 * author: Harshit Kumar, Khushi Neema
 * date: Feb 19, 2024
 * purpose: Implements various object detection algorithms
 *
 */

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include "objDetect.h"

using namespace std;
using namespace cv;

int blur5x5_2(cv::Mat &src, cv::Mat &dst)
{
    dst = cv::Mat::zeros(src.size(), src.type());
    cv::Mat temp = cv::Mat::zeros(src.size(), src.type());

    // row filter
    for (int i = 2; i < src.rows - 2; i++)
    {
        cv::Vec3b *rowptr = src.ptr<cv::Vec3b>(i);
        cv::Vec3b *tempptr = temp.ptr<cv::Vec3b>(i);

        // loop over columns
        for (int j = 2; j < src.cols - 2; j++)
        {
            // loop over color channels
            for (int c = 0; c < 3; c++)
            {
                // row filter [1 , 2, 4, 2, 1]
                tempptr[j][c] = (1 * rowptr[j - 2][c] + 2 * rowptr[j - 1][c] + 4 * rowptr[j][c] + 2 * rowptr[j + 1][c] + 1 * rowptr[j + 2][c]) / 10.0;
            }
        }
    }

    // column filter
    for (int i = 2; i < src.rows - 2; i++)
    {
        cv::Vec3b *rowptrm2 = temp.ptr<cv::Vec3b>(i - 2);
        cv::Vec3b *rowptrm1 = temp.ptr<cv::Vec3b>(i - 1);
        cv::Vec3b *rowptr = temp.ptr<cv::Vec3b>(i);
        cv::Vec3b *rowptrp1 = temp.ptr<cv::Vec3b>(i + 1);
        cv::Vec3b *rowptrp2 = temp.ptr<cv::Vec3b>(i + 2);

        cv::Vec3b *dptr = dst.ptr<cv::Vec3b>(i);

        // loop over columns
        for (int j = 2; j < src.cols - 2; j++)
        {
            // loop over color channels
            for (int c = 0; c < 3; c++)
            {
                /*column filter
                    [1]  m2
                    [2]  m1
                    [4]  r
                    [2]  p1
                    [1]  p2
                */
                dptr[j][c] = (1 * rowptrm2[j][c] + 2 * rowptrm1[j][c] + 4 * rowptr[j][c] + 2 * rowptrp1[j][c] + 1 * rowptrp2[j][c]) / 10.0;
                // clip b/w 0 and 255
                dptr[j][c] = dptr[j][c] > 255 ? 255 : dptr[j][c];
            }
        }
    }

    return 0; // Success
}

double calculateDynamicThreshold(const Mat &src, int k)
{
    // Reshape the image to a 1D array of pixels
    Mat samples(src.rows * src.cols, 1, CV_32F);
    for (int y = 0; y < src.rows; y++)
        for (int x = 0; x < src.cols; x++)
            samples.at<float>(y + x * src.rows, 0) = src.at<uchar>(y, x);

    // Apply k-means clustering
    Mat labels, centers;
    kmeans(samples, k, labels, TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 10, 1.0), 3, KMEANS_PP_CENTERS, centers);

    // Assuming K=2, calculate the mean threshold
    double thresholdValue = (centers.at<float>(0, 0) + centers.at<float>(1, 0)) / 2.0;
    return thresholdValue;
}

cv::Mat customThreshold(const cv::Mat &grayImage, double thresh, double maxValue)
{
    // clone the input image
    cv::Mat outputImage = grayImage.clone();

    // loop over the input image and apply the thresholding
    for (int i = 0; i < grayImage.rows; ++i)
    {
        for (int j = 0; j < grayImage.cols; ++j)
        {
            // apply the thresholding
            if (grayImage.at<uchar>(i, j) < thresh)
            {
                // set the pixel value to the maximum value
                outputImage.at<uchar>(i, j) = static_cast<uchar>(maxValue);
            }
            else
            {
                outputImage.at<uchar>(i, j) = 0;
            }
        }
    }

    return outputImage;
}

// Function to preprocess and threshold the video frame
Mat preprocessAndThreshold(const cv::Mat &frame)
{
    // Convert to grayscale
    Mat grayFrame, blur;
    Mat input = frame;
    blur5x5_2(input, blur);
    cv::convertScaleAbs(blur, blur);
    cvtColor(blur, grayFrame, COLOR_BGR2GRAY);

    // Optional: Blur the image to make regions more uniform
    // blur5x5_2(grayFrame, grayFrame);
    // cv::convertScaleAbs(grayFrame,grayFrame);
    // GaussianBlur(grayFrame, grayFrame, Size(5, 5), 0);

    // Dynamically calculate the threshold
    double thresholdValue = calculateDynamicThreshold(grayFrame, 2);
    // std::cout << "Threshold: " << thresholdValue << std::endl;

    // Apply the threshold
    // Mat thresholded;
    // threshold(grayFrame, thresholded, thresholdValue, 255, THRESH_BINARY_INV);
    cv::Mat thresholded = customThreshold(grayFrame, thresholdValue, 255);

    return thresholded;
}

void morphologyEx(const cv::Mat &src, cv::Mat &dst, int operation, const cv::Mat &kernel)
{
    switch (operation)
    {
    case MORPH_DILATE:
        dilate(src, dst, kernel);
        break;
    case MORPH_ERODE:
        erode(src, dst, kernel);
        break;
    case MORPH_OPEN:
    {
        Mat temp;
        erode(src, temp, kernel);
        dilate(temp, dst, kernel);
        break;
    }
    case MORPH_CLOSE:
    {
        Mat temp;
        dilate(src, temp, kernel);
        erode(temp, dst, kernel);
        break;
    }
    default:
        std::cout << "Invalid morphological operation" << std::endl;
        break;
    }
}

void dfs(const Mat &binaryImage, Mat &labels, int i, int j, int label)
{
    int rows = binaryImage.rows;
    int cols = binaryImage.cols;

    // Check if current pixel is within image boundaries and is foreground
    if (i < 0 || i >= rows || j < 0 || j >= cols || binaryImage.at<uchar>(i, j) == 0 || labels.at<int>(i, j) != 0)
    {
        return; // Out of bounds, background pixel, or already labeled
    }

    labels.at<int>(i, j) = label; // Label current pixel

    // Recursively label neighboring pixels
    dfs(binaryImage, labels, i + 1, j, label);
    dfs(binaryImage, labels, i - 1, j, label);
    dfs(binaryImage, labels, i, j + 1, label);
    dfs(binaryImage, labels, i, j - 1, label);
}

// Function to perform connected components analysis
void connectedComponents(const Mat &binaryImage, Mat &labeledImage)
{
    labeledImage = Mat::zeros(binaryImage.size(), CV_32S); // Initialize labeled image

    int label = 1; // Start labeling from 1
    for (int i = 0; i < binaryImage.rows; ++i)
    {
        for (int j = 0; j < binaryImage.cols; ++j)
        {
            if (binaryImage.at<uchar>(i, j) != 0 && labeledImage.at<int>(i, j) == 0)
            {
                std::cout << "Processing pixel (" << i << ", " << j << ")" << std::endl;
                dfs(binaryImage, labeledImage, i, j, label++);
            }
        }
    }
}

class UnionFind
{
private:
    std::vector<int> parent;
    std::vector<int> rank;

public:
    UnionFind(int n)
    {
        parent.resize(n);
        rank.resize(n);
        for (int i = 0; i < n; ++i)
        {
            parent[i] = i;
            rank[i] = 0;
        }
    }

    int find(int u)
    {
        if (parent[u] != u)
        {
            parent[u] = find(parent[u]); // Path compression
        }
        return parent[u];
    }

    void unite(int u, int v)
    {
        int rootU = find(u);
        int rootV = find(v);
        if (rootU == rootV)
            return;

        if (rank[rootU] < rank[rootV])
        {
            parent[rootU] = rootV;
        }
        else if (rank[rootU] > rank[rootV])
        {
            parent[rootV] = rootU;
        }
        else
        {
            parent[rootV] = rootU;
            rank[rootU]++;
        }
    }
};

#if 0
std::map<int, int> connectedComponentsTwoPass(const Mat &binaryImage, Mat &labeledImage)
{
    labeledImage = Mat::zeros(binaryImage.size(), CV_32S); // Initialize labeled image

    int rows = binaryImage.rows;
    int cols = binaryImage.cols;
    UnionFind uf(rows * cols);

    // Traverse the image pixels
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            if (binaryImage.at<uchar>(i, j) != 0)
            {
                int current = i * cols + j;
                int up = (i > 0) ? (current - cols) : -1;
                int left = (j > 0) ? (current - 1) : -1;

                // Union with neighboring pixels
                if (up != -1 && binaryImage.at<uchar>(i - 1, j) != 0)
                    uf.unite(current, up);
                if (left != -1 && binaryImage.at<uchar>(i, j - 1) != 0)
                    uf.unite(current, left);
            }
        }
    }

    // Assign labels to connected components
    std::map<int, int> labelsMap;
    int newLabel = 0;
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            int current = i * cols + j;
            if (binaryImage.at<uchar>(i, j) != 0)
            {
                int label = uf.find(current);
                if (labelsMap.find(label) == labelsMap.end())
                {
                    labelsMap[label] = newLabel++;
                }
                labeledImage.at<int>(i, j) = labelsMap[label];
            }
        }
    }

    return labelsMap;
}
#endif

void connectedComponentsTwoPass(const Mat &binaryImage, Mat &labeledImage)
{
    labeledImage = Mat::zeros(binaryImage.size(), CV_32S); // Initialize labeled image

    int rows = binaryImage.rows;
    int cols = binaryImage.cols;
    UnionFind uf(rows * cols); // Assume UnionFind is a correctly implemented class

    // First pass: Label the components
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            if (binaryImage.at<uchar>(i, j) != 0)
            {
                int current = i * cols + j;
                int up = (i > 0) ? (current - cols) : -1;
                int left = (j > 0) ? (current - 1) : -1;

                // Union with neighboring pixels
                if (up != -1 && binaryImage.at<uchar>(i - 1, j) != 0)
                    uf.unite(current, up);
                if (left != -1 && binaryImage.at<uchar>(i, j - 1) != 0)
                    uf.unite(current, left);
            }
        }
    }

    // Second pass: Map the root of each union-find set to a label
    map<int, int> componentSizes;
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            if (binaryImage.at<uchar>(i, j) != 0)
            {
                int label = uf.find(i * cols + j);
                labeledImage.at<int>(i, j) = label;
                componentSizes[label]++;
            }
        }
    }

    // Filter out small components
    int sizeThreshold = 10000; // Minimum size of a connected component
    // Create a map to store the new labels for sufficiently large components
    // key: original label, value: new label
    map<int, int> labelsMap;
    int newLabel = 1; // Start labeling from 1
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            int originalLabel = labeledImage.at<int>(i, j);
            // cout << componentSizes[originalLabel] << endl;
            if (originalLabel > 0 && componentSizes[originalLabel] >= sizeThreshold)
            {
                if (labelsMap.find(originalLabel) == labelsMap.end())
                {
                    labelsMap[originalLabel] = newLabel++;
                }
                labeledImage.at<int>(i, j) = labelsMap[originalLabel];
            }
            else
            {
                labeledImage.at<int>(i, j) = 0; // Set to background
            }
        }
    }
}

std::map<int, ObjectFeatures> computeFeatures(const cv::Mat &labeledImage, cv::Mat &outputImage)
{
    // Create a copy of the labeled image for visualization
    outputImage = labeledImage.clone();
    // Convert outputImage to CV_8U for visualization if it's not already
    if (outputImage.type() != CV_8U)
    {
        double minVal, maxVal;
        cv::minMaxLoc(outputImage, &minVal, &maxVal); // Find min and max values to scale the image
        outputImage.convertTo(outputImage, CV_8U, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
    }

    // Create a map to store the features of each connected component
    std::map<int, ObjectFeatures> featuresMap;
    std::set<int> uniqueLabels; // To keep track of unique labels (connected components)

    // Iterate over the labeledImage to find unique labels
    for (int i = 0; i < labeledImage.rows; ++i)
    {
        for (int j = 0; j < labeledImage.cols; ++j)
        {
            int label = labeledImage.at<int>(i, j);
            if (label != 0)
            { // Exclude background
                uniqueLabels.insert(label);
            }
        }
    }

    for (int label : uniqueLabels)
    {
        // Extract the component as a binary mask
        cv::Mat mask = labeledImage == label;

        // Calculate moments for this component
        cv::Moments moments = cv::moments(mask, true);

        // Calculate centroid
        double centerX = moments.m10 / moments.m00;
        double centerY = moments.m01 / moments.m00;

        // Calculate the orientation (angle of the axis of least moment)
        double angle = 0.5 * atan2(2 * moments.mu11, moments.mu20 - moments.mu02);

        // Use non-zero locations for minAreaRect
        std::vector<cv::Point> nonZeroLocations;
        cv::findNonZero(mask, nonZeroLocations);
        cv::RotatedRect rotatedRect = cv::minAreaRect(nonZeroLocations);

        // Feature calculations
        double area = cv::contourArea(nonZeroLocations);
        double boundingBoxArea = rotatedRect.size.width * rotatedRect.size.height;
        double percentFilled = (area / boundingBoxArea) * 100.0;
        double aspectRatio = rotatedRect.size.width / rotatedRect.size.height;

        // Store the features
        featuresMap[label] = {percentFilled, aspectRatio};

        // Draw the oriented bounding box
        cv::Point2f vertices[4];
        rotatedRect.points(vertices);
        for (int i = 0; i < 4; i++)
        {
            cv::line(outputImage, vertices[i], vertices[(i + 1) % 4], static_cast<uchar>(255), 4);
        }

        // Draw the axis of least moment
        cv::Point2f pt1, pt2;
        double length = std::max(rotatedRect.size.width, rotatedRect.size.height) / 2;
        pt1.x = static_cast<float>(centerX + length * cos(angle));
        pt1.y = static_cast<float>(centerY + length * sin(angle));
        pt2.x = static_cast<float>(centerX - length * cos(angle));
        pt2.y = static_cast<float>(centerY - length * sin(angle));
        cv::line(outputImage, pt1, pt2, static_cast<uchar>(255), 2);

        // label object on image
        cv::putText(outputImage, std::to_string(label), cv::Point(centerX, centerY), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 3);
    }

    return featuresMap;
}

std::map<std::string, ObjectFeatures> loadFeatureDatabase(const std::string &filename)
{
    std::map<std::string, ObjectFeatures> database;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string label;
        ObjectFeatures features;
        char delimiter; // To consume the comma delimiter after the label

        if (std::getline(iss, label, ',') &&
            (iss >> features.percentFilled >> delimiter >> features.aspectRatio))
        {
            database[label] = features;
        }
    }

    return database;
}

ObjectFeatures calculateStdDev(const std::map<std::string, ObjectFeatures> &database)
{
    ObjectFeatures mean = {0.0, 0.0}, stdDev = {0.0, 0.0};
    int count = database.size();

    // Calculate sums for each feature
    for (const auto &entry : database)
    {
        mean.percentFilled += entry.second.percentFilled;
        mean.aspectRatio += entry.second.aspectRatio;
    }

    // Calculate mean
    mean.percentFilled /= count;
    mean.aspectRatio /= count;

    // Calculate squared sum for standard deviation
    for (const auto &entry : database)
    {
        stdDev.percentFilled += std::pow(entry.second.percentFilled - mean.percentFilled, 2);
        stdDev.aspectRatio += std::pow(entry.second.aspectRatio - mean.aspectRatio, 2);
    }

    // Finalize standard deviation calculation
    stdDev.percentFilled = std::sqrt(stdDev.percentFilled / count);
    stdDev.aspectRatio = std::sqrt(stdDev.aspectRatio / count);

    return stdDev;
}

double scaledEuclideanDistance(const ObjectFeatures &f1, const ObjectFeatures &f2, const ObjectFeatures &stdev)
{
    double distance = 0.0;
    distance += std::pow((f1.percentFilled - f2.percentFilled) / stdev.percentFilled, 2);
    distance += std::pow((f1.aspectRatio - f2.aspectRatio) / stdev.aspectRatio, 2);
    return std::sqrt(distance);
}

std::string classifyObject(const ObjectFeatures &unknownObjectFeatures, const std::map<std::string, ObjectFeatures> &database, const ObjectFeatures &stdev)
{
    std::string bestMatch = "Unknown";
    double minDistance = std::numeric_limits<double>::max();

    for (const auto &entry : database)
    {
        double distance = scaledEuclideanDistance(unknownObjectFeatures, entry.second, stdev);

        if (distance < minDistance)
        {
            minDistance = distance;
            bestMatch = entry.first;
        }
    }

    return bestMatch;
}