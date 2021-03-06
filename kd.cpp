#include "ioVis.h"
#include <vector>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <math.h>

#define gridLength 0.5

struct PairPoint{
	pcl::PointXYZINormal p1;
	pcl::PointXYZINormal p2;
};

typedef pcl::PointCloud<pcl::PointXYZI>::Ptr XYZICloudPtr;
typedef pcl::PointCloud<pcl::PointXYZI> XYZICloud;
typedef pcl::PointCloud<pcl::PointXYZINormal>::Ptr SurfelCloudPtr;
typedef pcl::PointCloud<pcl::PointXYZINormal> SurfelCloud;
typedef std::vector<pcl::PointXYZI> Grid;

// 从 vector 中制作点云
XYZICloudPtr genCloud(Grid);
// 将栅格中的所有点转换为一个 Surfel 特征
pcl::PointXYZINormal genSurfel(Grid);
// 将实对称矩阵分解为特征向量和特征值
int eejcb(float a[], int n, float v[], float eps, int jt);
// 将一个普通点云变成一个 Surfel 特征点云
SurfelCloudPtr cloud2Surfel(XYZICloudPtr);

int main(int argc, char* argv[]){
	XYZICloudPtr cloud1 = getFile(argv[1]);
	XYZICloudPtr cloud2 = getFile(argv[2]);
    SurfelCloudPtr surfel1 = cloud2Surfel(cloud1);
    SurfelCloudPtr surfel2 = cloud2Surfel(cloud2);
	std::vector<PairPoint> points;
    pcl::KdTreeFLANN<pcl::PointXYZINormal> kdtree;
	srand((unsigned)time(NULL));

	kdtree.setInputCloud(surfel1);

	for(int i = 0; i < surfel2->size(); i++){
		std::vector<int> idx(1);
		std::vector<float> distance(1);
		if(kdtree.nearestKSearch(surfel2->points[i],1,idx,distance) > 0){
			// cout << "Nearest neighbor search at (" << surfel2->points[i].x
			// 	 << " " << surfel2->points[i].y
			// 	 << " " << surfel2->points[i].z
			// 	 << ") ";

			// cout << "    " << surfel1->points[idx[0]].x
			// 	 << " " << surfel1->points[idx[0]].y
			// 	 << " " << surfel1->points[idx[0]].z
			// 	 << " (distance: " << distance[0] << ")" << endl;

			PairPoint p = {surfel1->points[idx[0]],surfel2->points[i]};
			points.push_back(p);
		}
	}

	pcl::visualization::PCLVisualizer viewer("kdtree");
	pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZI> handler1 (cloud1, 255, 255, 255);
	pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZI> handler2 (cloud2, 255, 255, 255);
	int v1(1);
	int v2(2);

	viewer.createViewPort(0.0,0.0,0.5,1.0,v1);
	viewer.createViewPort(0.5,0.0,1.0,1.0,v2);
	viewer.setBackgroundColor(0,0,0);
	viewer.addPointCloud<pcl::PointXYZI>(cloud1,handler1,"before",v1);
	viewer.addPointCloud<pcl::PointXYZI>(cloud2,handler2,"after",v2);
	viewer.addCoordinateSystem(1.0);
	viewer.initCameraParameters();

	for(int i = 0; i < points.size(); i++){
		if(i % 40 != 0) continue;
		auto pair = points[i];
		double rgb[3] = {((double)(rand()%100))/100.0,((double)(rand()%100))/100.0,((double)(rand()%100))/100.0};
		viewer.addCube(pair.p1.x-0.5,pair.p1.x+0.5,
					   pair.p1.y-0.5,pair.p1.y+0.5,
					   pair.p1.z-0.5,pair.p1.z+0.5,
					   rgb[0],rgb[1],rgb[2],"cube"+std::to_string(i),v1);
		viewer.addCube(pair.p2.x-0.5,pair.p2.x+0.5,
					   pair.p2.y-0.5,pair.p2.y+0.5,
					   pair.p2.z-0.5,pair.p2.z+0.5,
					   rgb[0],rgb[1],rgb[2],"cube"+std::to_string(i)+"d",v2);
		viewer.addText3D<pcl::PointXYZINormal>("A "+std::to_string(i),pair.p1,0.4,rgb[0],rgb[1],rgb[2],"string"+std::to_string(i),2);	  // 如果想在单独的坐标系中添加文字，需要用 viewport+1 才行
		viewer.addText3D<pcl::PointXYZINormal>("B "+std::to_string(i),pair.p2,0.4,rgb[0],rgb[1],rgb[2],"string"+std::to_string(i)+"d",3); // 如果用的是 0,那会把所有坐标系都添加上一样的文字
		viewer.setShapeRenderingProperties(pcl::visualization::PCL_VISUALIZER_REPRESENTATION, pcl::visualization::PCL_VISUALIZER_REPRESENTATION_WIREFRAME, "cube"+std::to_string(i));
		viewer.setShapeRenderingProperties(pcl::visualization::PCL_VISUALIZER_REPRESENTATION, pcl::visualization::PCL_VISUALIZER_REPRESENTATION_WIREFRAME, "cube"+std::to_string(i)+"d");
	}

	while (!viewer.wasStopped())
    {
        viewer.spinOnce(100);
        boost::this_thread::sleep(boost::posix_time::microseconds (100000));
    }


    return 0;
}

XYZICloudPtr genCloud(Grid points){
    XYZICloudPtr cloud(new pcl::PointCloud<pcl::PointXYZI>);

    for(Grid::iterator point = points.begin(); point != points.end(); point ++){
        cloud->push_back(*point);
    }
    
    return cloud;
}

pcl::PointXYZINormal genSurfel(Grid points){
    if(points.empty()){
        pcl::PointXYZINormal point;
        point.normal_x = 0;
        point.normal_y = 0;
        point.normal_z = 0;
        return point;
    }
    
    pcl::PointXYZINormal centerPoint;
    float cov[9] = {0.0};
    float v[9] = {0.0};


    for(Grid::iterator point = points.begin(); point != points.end(); point ++){
        centerPoint.x += point->x;
        centerPoint.y += point->y;
        centerPoint.z += point->z;   
    }
    centerPoint.x = centerPoint.x / points.size();
    centerPoint.y = centerPoint.y / points.size();
    centerPoint.z = centerPoint.z / points.size();

    //2) 协方差矩阵cov
	for(Grid::iterator point = points.begin(); point != points.end(); point ++)
	{
		float buf[3];
		buf[0] = point->x - centerPoint.x;
		buf[1] = point->y - centerPoint.y;
		buf[2] = point->z - centerPoint.z;
		for(int pp=0; pp<3; pp++)
		{
			for(int l=0; l<3; l++)
			{
				cov[pp*3+l] += buf[pp]*buf[l];
			}
		}
	}
    for(int i = 0; i < 9; i++){
        cov[i] = cov[i] / points.size();
    }

    eejcb(cov, 3, v, 0.001, 1000);
    
    float eigenValue[3] = {cov[0], cov[4], cov[8]};
    float* eigenVector[3];
    eigenVector[0] = new float[3]{v[0],v[3],v[6]};
    eigenVector[1] = new float[3]{v[1],v[4],v[7]};
    eigenVector[2] = new float[3]{v[2],v[5],v[8]};

    for (int i=2; i>0; i--)  //冒泡排序从小到大
	{
		for (int j=0; j<i; j++)
		{
			if (eigenValue[j] > eigenValue[j+1])
			{
				float tmpVa = eigenValue[j];
				float* tmpVe = eigenVector[j];
				eigenValue[j] = eigenValue[j+1];
				eigenVector[j] = eigenVector[j+1];
				eigenValue[j+1] = tmpVa;
				eigenVector[j+1] = tmpVe;
			}
		}
	}

    centerPoint.normal_x = eigenVector[0][0];
    centerPoint.normal_y = eigenVector[0][1];
    centerPoint.normal_z = eigenVector[0][2];
    centerPoint.intensity = eigenValue[2] * eigenValue[1];

    return centerPoint;
}

int eejcb(float a[], int n, float v[], float eps, int jt) { 
	int i,j,p,q,u,w,t,s,l; 
	float fm,cn,sn,omega,x,y,d; 
	l=1; 
	for (i=0; i<=n-1; i++) 
	{ 
		v[i*n+i]=1.0; 
		for (j=0; j<=n-1; j++) 
		{ 
			if (i!=j) 
			{ 
				v[i*n+j]=0.0; 
			} 
		} 
	} 
	while (1==1) 
	{ 
		fm=0.0; 
		for (i=0; i<=n-1; i++) 
		{ 
			for (j=0; j<=n-1; j++) 
			{ 
				d=fabs(a[i*n+j]); 
				if ((i!=j)&&(d>fm)) 
				{ 
					fm=d; 
					p=i; 
					q=j; 
				} 
			} 
		} 
		if (fm<eps) 
		{ 
			return(1); 
		} 
		if (l>jt) 
		{ 
			return(-1); 
		} 
		l=l+1; 
		u=p*n+q; 
		w=p*n+p; 
		t=q*n+p; 
		s=q*n+q; 
		x=-a[u]; 
		y=(a[s]-a[w])/2.0; 
		omega=x/sqrt(x*x+y*y); 
		if (y<0.0) 
		{ 
			omega=-omega; 
		} 
		sn=1.0+sqrt(1.0-omega*omega); 
		sn=omega/sqrt(2.0*sn); 
		cn=sqrt(1.0-sn*sn); 
		fm=a[w]; 
		a[w]=fm*cn*cn+a[s]*sn*sn+a[u]*omega; 
		a[s]=fm*sn*sn+a[s]*cn*cn-a[u]*omega; 
		a[u]=0.0; 
		a[t]=0.0; 
		for (j=0; j<=n-1; j++) 
		{ 
			if ((j!=p)&&(j!=q)) 
			{ 
				u=p*n+j; 
				w=q*n+j; 
				fm=a[u]; 
				a[u]=fm*cn+a[w]*sn; 
				a[w]=-fm*sn+a[w]*cn; 
			} 
		} 
		for (i=0; i<=n-1; i++) 
		{ 
			if ((i!=p)&&(i!=q)) 
			{ 
				u=i*n+p; 
				w=i*n+q; 
				fm=a[u]; 
				a[u]=fm*cn+a[w]*sn; 
				a[w]=-fm*sn+a[w]*cn; 
			} 
		} 
		for (i=0; i<=n-1; i++) 
		{ 
			u=i*n+p; 
			w=i*n+q; 
			fm=v[u]; 
			v[u]=fm*cn+v[w]*sn; 
			v[w]=-fm*sn+v[w]*cn; 
		} 
	} 
	return(1); 
} 

SurfelCloudPtr cloud2Surfel(XYZICloudPtr cloud){
	SurfelCloudPtr surfelCloud(new SurfelCloud);
	std::map<std::string,Grid> subClouds;
    float min_x = 0;
    float min_y = 0;
    float min_z = 0;

	// 找到点云数据的边界
    for (auto point : cloud->points) {
        min_x = point.x < min_x ? point.x : min_x;
        min_y = point.y < min_y ? point.y : min_y;
        min_z = point.z < min_z ? point.z : min_z;
    }

	// 将不同的点根据其坐标分配到不同的栅格里
    for(auto point : cloud->points){
        const int xPos = (point.x - min_x) / gridLength;
        const int yPos = (point.y - min_y) / gridLength;
        const int zPos = (point.z - min_z) / gridLength;
        subClouds["("+std::to_string(xPos)+","
					 +std::to_string(yPos)+","
					 +std::to_string(zPos)+")"].push_back(point);
    }

	// 将栅格中的所有点用一个带有 Surfel 特征的中心点代表
    for(auto subCloud : subClouds){
		if(subCloud.second.empty()) continue;
		surfelCloud->push_back(genSurfel(subCloud.second));
    }

    cout << "\nSurfel extract complete!\n" << "Surfel map size: " << surfelCloud->size() << endl;

    return surfelCloud;
}