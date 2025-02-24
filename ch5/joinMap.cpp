//
// Created by bob on 19-3-16.
//

#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <Eigen/Geometry>
#include <boost/format.hpp>
#include <pcl-1.7/pcl/point_types.h>
#include <pcl-1.7/pcl/io/pcd_io.h>

#include <pcl-1.7/pcl/visualization/pcl_visualizer.h>


int main(int argc, char* argv[]){

    //vector容器，装载彩色图和深度图
    vector<cv::Mat> colorImgs,depthImgs;

    //T矩阵容器，aligned_allocator可能是用来对齐内存提高效率？
    vector<Eigen::Isometry3d,Eigen::aligned_allocator<Eigen::Isometry3d>> poses;
    ifstream fin("./pose.txt");

    if (!fin){
        cerr << "请在有pose.txt的目录下运行此程序" << endl;
        return 1;
    }

    //获取每一张图片的相机与
    for (int i = 0; i< 5;i++){
        boost::format fmt("./%s/%d.%s");

        colorImgs.push_back( cv::imread( (fmt %"color"%(i+1)%"png").str() ));
        depthImgs.push_back( cv::imread( (fmt %"depth"%(i+1)%"pgm").str(), -1));//读取灰度值图像

        double data[7] = {0};

        for (auto& d : data)
            fin >> d;

        Eigen::Quaterniond q(data[6],data[3],data[4],data[5]);
        //构建相机到世界坐标系的欧氏变换矩阵Twc
        Eigen::Isometry3d T(q);
        T.pretranslate(Eigen::Vector3d(data[0],data[1],data[2]));
        poses.push_back( T );
    }

    //确定相机内参
    double cx = 325.5;
    double cy = 253.5;
    double fx = 518.0;
    double fy = 519.0;
    double depthScale = 1000.0;


    cout << "正在将图像转换为点云..." << endl;

    typedef pcl::PointXYZRGB PointT;
    typedef pcl::PointCloud<PointT> PointCloud;

    PointCloud::Ptr pointCloud(new PointCloud);

    for (int i = 0; i < 5; ++i) {
        cout << "转换图像中：" << i+1 << endl;
        //这里直接复赋值不会拷贝
        cv::Mat color = colorImgs[i];
        cv::Mat depth = depthImgs[i];
        
        Eigen::Isometry3d T = poses[i];

        for (int v = 0; v < color.rows; ++v) {
            for (int u = 0; u < color.cols; ++u) {
                unsigned int d = depth.ptr<unsigned short> (v)[u];
                if (d == 0)//d == 0表示深度值为0，没有探测到
                    continue;

                //该像素点坐标(u,v)对应的像极坐标系里的点P'
                Eigen::Vector3d point;

                //d此时就指的是z
                point[2] = double(d) / depthScale;
                point[0] = (u-cx) * point[2] / fx;
                point[1] = (v-cy) * point[2] / fy;

                //求出point对应的现实世界坐标P
                Eigen::Vector3d pointW = T * point;

                PointT p;
                p.x = pointW[0];
                p.y = pointW[1];
                p.z = pointW[2];
                p.b = color.data[v*color.step+u*color.channels()];
                p.g = color.data[v*color.step+u*color.channels()+1];
                p.r = color.data[v*color.step+u*color.channels()+2];

                pointCloud->points.push_back(p);
            }
        }
    }
    //这里把点云的设置成无限的
    pointCloud->is_dense = false;
    cout << "点云共有" << pointCloud->size() << "个点." << endl;

    //解决terminate called after throwing an instance of ‘pcl::IOException’ what(): [pcl::PCDWriter::writeASCII] Number of points different than width * height!
    pointCloud->width = 1;
    pointCloud->height = pointCloud->size();

    pcl::io::savePCDFile("map.pcd",*pointCloud);

    return 0;
}

