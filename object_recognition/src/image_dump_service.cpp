#include <queue>
#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <image_transport/image_transport.h>
#include <object_recognition/ImageDump.h>

namespace object_recognition {

    class ImageDumpService {

        struct ImageDumpTask {
            std::string filename;
            sensor_msgs::ImageConstPtr image;
        };

        private:
        ros::ServiceServer service_;

        std::string dir_name_;

        ImageDump::Request* next_request_ = NULL;
        std::queue<ImageDumpTask> image_dump_tasks_;

        bool onRequestImageDump(ImageDump::Request& req, ImageDump::Response& res)
        {
            next_request_ = &req;
        }

        void imageCallback(const sensor_msgs::ImageConstPtr& msg)
        {
            if(next_request_!=NULL) {
                ImageDumpTask t;
                t.filename = next_request_->filename;
                t.image = msg;
                image_dump_tasks_.push(t);
            }
            next_request_ = NULL;
        }

        void dumpImage(const ImageDumpTask& task) {
            cv_bridge::CvImageConstPtr cv_ptr;
            try
            {
                cv_ptr = cv_bridge::toCvShare(task.image, sensor_msgs::image_encodings::BGR8);
                std::vector<int> compression_params;
                compression_params.push_back(CV_IMWRITE_PNG_COMPRESSION);
                compression_params.push_back(9);
                cv::imwrite(dir_name_ + "/" + task.filename + ".png", cv_ptr->image);
            }
            catch (cv_bridge::Exception& e)
            {
                ROS_ERROR("cv_bridge exception: %s", e.what());
                return;
            }
            catch (std::runtime_error& e)
            {
                ROS_ERROR("Exception converting image to PNG format: %s", e.what());
                return;
            }
        }

        public:
        ImageDumpService(const std::string& image_topic, const std::string& dir_name) : dir_name_(dir_name)
        {
            ros::NodeHandle nh;
            service_ = nh.advertiseService("/image_dump_service", &ImageDumpService::onRequestImageDump, this);

            image_transport::ImageTransport it(nh);
            image_transport::Subscriber sub = it.subscribe(image_topic, 1, boost::bind(&ImageDumpService::imageCallback, this, _1));

            ros::Rate r(2);
            while(ros::ok()) {
                if(!image_dump_tasks_.empty()) {
                    dumpImage(image_dump_tasks_.front());
                    image_dump_tasks_.pop();
                }
                if(image_dump_tasks_.size() > 5) {
                    ROS_WARN_STREAM_THROTTLE(10, "Image dump service is unable to handle request volume. Number of pending requests: " << image_dump_tasks_.size());
                }
                r.sleep();
            }
        }
    };
}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "push_execution_node");
    ros::AsyncSpinner spinner(4);
    spinner.start();
    ros::NodeHandle nh;
    std::string push_results_directory;
    if(nh.getParam("push_result_directory", push_results_directory)) {
	    object_recognition::ImageDumpService ids("/kinect2/qhd/image_color", push_results_directory + "/images");
    } else {
	    ROS_ERROR("Failed to retrieve push results directory!");
    }

    return 0;
}

