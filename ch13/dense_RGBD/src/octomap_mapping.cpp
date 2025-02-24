#include <iostream>
#include <fstream>

using namespace std;

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <octomap/octomap.h>

#include <Eigen/Geometry>
#include <boost/format.hpp>

int main(int argc,char** argv){

    vector<cv::Mat> colorImgs, depthImgs;
    vector<Eigen::Isometry3d, Eigen::aligned_allocator<Eigen::Isometry3d>> poses;

    // // vector<Eigen::Isometry3d> poses;

    ifstream fin("./pose.txt");

    if (!fin)
    {
        cerr << "请提供pose.txt文件!" << endl;
        return 1;
    }

    //获取每一张图片的相机与位姿
    for (int i = 0; i < 5; i++)
    {
        boost::format fmt("./%s/%d.%s");

        colorImgs.push_back(cv::imread((fmt % "color" % (i + 1) % "png").str()));
        depthImgs.push_back(cv::imread((fmt % "depth" % (i + 1) % "pgm").str(), -1)); //读取灰度值图像

        double data[7] = {0};

        for (auto &d : data)
            fin >> d;

        Eigen::Quaterniond q(data[6], data[3], data[4], data[5]);
        //构建相机到世界坐标系的欧氏变换矩阵Twc
        Eigen::Isometry3d T(q);
        T.pretranslate(Eigen::Vector3d(data[0], data[1], data[2]));
        poses.push_back(T);
    }

    //确定相机内参
    double cx = 325.5;
    double cy = 253.5;
    double fx = 518.0;
    double fy = 519.0;
    double depthScale = 1000.0;

    cout << "将图片转为Octomap ..." << endl;

    octomap::OcTree tree(0.05);

    for(int i=0;i<5;i++){
        cv::Mat color = colorImgs[i];
        cv::Mat depth =depthImgs[i];
        Eigen::Isometry3d T = poses[i];

        octomap::Pointcloud cloud;

        for (size_t v = 0; v < color.rows; v++)
        {
            for (size_t u = 0; u < color.cols; u++)
            {
                unsigned int d = depth.ptr<unsigned short> (v)[u];
                if (d==0)
                {
                    continue;
                }

                if (d>7000)
                {
                    continue;
                }
                Eigen::Vector3d point;
                point[2]=(double)d/depthScale;
                point[0]=(u-cx)*point[2]/fx;
                point[1]=(v-cy)*point[2]/fy;
                Eigen::Vector3d point2world = T*point;
                cloud.push_back(point2world[0],point2world[1],point2world[2]);

                
            }
        }

        tree.insertPointCloud(cloud,octomap::point3d(T(0,3),T(1,3),T(2,3)));
        
    }

    tree.updateInnerOccupancy();
    cout << "保存octomap..." <<endl;
    tree.writeBinary("octomap.bt");
    return 0;
}