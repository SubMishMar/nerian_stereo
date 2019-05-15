/*******************************************************************************
 * Copyright (c) 2019 Nerian Vision Technologies
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *******************************************************************************/

#include <ros/ros.h>
#include <visiontransfer/asynctransfer.h>
#include <visiontransfer/reconstruct3d.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/Image.h>
#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>
#include <iostream>
#include <iomanip>
#include <nerian_stereo/StereoCameraInfo.h>
#include <boost/smart_ptr.hpp>
#include <colorcoder.h>

#include <dynamic_reconfigure/server.h>
#include <nerian_stereo/NerianStereoConfig.h>
#include <visiontransfer/scenescanparameters.h>
#include <visiontransfer/standardparameters.h> // TODO

using namespace std;
using namespace visiontransfer;

/**
 * \brief A driver node that receives data from SceneScan/SP1 and forwards
 * it to ROS.
 *
 * SceneScan and SP1 by Nerian Vision Technologies are hardware systems for
 * real-time stereo vision. They transmit a computed disparity map (an
 * inverse depth map) through gigagibt ethernet, which is then received by
 * this node. The node converts the received data into ROS messages, which
 * contain the following data:
 *
 * - Point cloud of reconstructed 3D locations
 * - Disparity map with optional color coding
 * - Rectified left camera image
 *
 * In addition, camera calibration information is also published. For
 * configuration parameters, please see the provided example launch file.
 * For more information about Nerian's SceneScan system, please visit
 * http://nerian.com/products/scenescan-stereo-vision/
 */

class DynamicReconfigurationHandler {
public:
};


class StereoNode {
public:
    StereoNode(): frameNum(0), initialConfigReceived(false) {
    }

    ~StereoNode() {
    }

    /*
     * \brief Callback that receives an updated configuration from ROS
     */
    void dynamicReconfigureCallback(nerian_stereo::NerianStereoConfig &config, uint32_t level)
    {
        if (initialConfigReceived) {
            ROS_INFO("Reconfigure request: new config object");
            if (config.operation_mode != lastKnownConfig.operation_mode) {
                ROS_INFO("-> operation_mode changed to %d", config.operation_mode);
            }
        } else {
            ROS_INFO("Initial config received");
            initialConfigReceived = true;
        }
        lastKnownConfig = config;
    }

    void updateParameterServerFromDevice(std::map<std::string, ParameterInfo>& cfg)
    {
        //ROS_INFO("Device reported current operation_mode %d", cfg["operation_mode"].getValue<int>());
        // publish the current config to the parameter server
        nh.setParam("/nerian_stereo/auto_exposure_mode", cfg["auto_exposure_mode"].getValue<int>());
        nh.setParam("/nerian_stereo/auto_exposure_roi_enabled", cfg["auto_exposure_roi_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/auto_exposure_roi_height", cfg["auto_exposure_roi_height"].getValue<int>());
        nh.setParam("/nerian_stereo/auto_exposure_roi_width", cfg["auto_exposure_roi_width"].getValue<int>());
        nh.setParam("/nerian_stereo/auto_exposure_roi_x", cfg["auto_exposure_roi_x"].getValue<int>());
        nh.setParam("/nerian_stereo/auto_exposure_roi_y", cfg["auto_exposure_roi_y"].getValue<int>());
        nh.setParam("/nerian_stereo/auto_intensity_delta", cfg["auto_intensity_delta"].getValue<double>());
        nh.setParam("/nerian_stereo/auto_maximum_exposure_time", cfg["auto_maximum_exposure_time"].getValue<double>());
        nh.setParam("/nerian_stereo/auto_maximum_gain", cfg["auto_maximum_gain"].getValue<double>());
        nh.setParam("/nerian_stereo/auto_recalibration_enabled", cfg["auto_recalibration_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/auto_recalibration_permanent", cfg["auto_recalibration_permanent"].getValue<bool>());
        nh.setParam("/nerian_stereo/auto_skipped_frames", cfg["auto_skipped_frames"].getValue<int>());
        nh.setParam("/nerian_stereo/auto_target_frame", cfg["auto_target_frame"].getValue<int>());
        nh.setParam("/nerian_stereo/auto_target_intensity", cfg["auto_target_intensity"].getValue<double>());
        nh.setParam("/nerian_stereo/consistency_check_enabled", cfg["consistency_check_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/consistency_check_sensitivity", cfg["consistency_check_sensitivity"].getValue<int>());
        nh.setParam("/nerian_stereo/disparity_offset", cfg["disparity_offset"].getValue<int>());
        nh.setParam("/nerian_stereo/gap_interpolation_enabled", cfg["gap_interpolation_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/manual_exposure_time", cfg["manual_exposure_time"].getValue<double>());
        nh.setParam("/nerian_stereo/manual_gain", cfg["manual_gain"].getValue<double>());
        nh.setParam("/nerian_stereo/mask_border_pixels_enabled", cfg["mask_border_pixels_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/max_frame_time_difference_ms", cfg["max_frame_time_difference_ms"].getValue<int>());
        nh.setParam("/nerian_stereo/noise_reduction_enabled", cfg["noise_reduction_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/number_of_disparities", cfg["number_of_disparities"].getValue<int>());
        nh.setParam("/nerian_stereo/operation_mode", cfg["operation_mode"].getValue<int>());
        nh.setParam("/nerian_stereo/reboot", cfg["reboot"].getValue<bool>());
        nh.setParam("/nerian_stereo/sgm_p1", cfg["sgm_p1"].getValue<int>());
        nh.setParam("/nerian_stereo/sgm_p2", cfg["sgm_p2"].getValue<int>());
        nh.setParam("/nerian_stereo/speckle_filter_iterations", cfg["speckle_filter_iterations"].getValue<int>());
        nh.setParam("/nerian_stereo/texture_filter_enabled", cfg["texture_filter_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/texture_filter_sensitivity", cfg["texture_filter_sensitivity"].getValue<int>());
        nh.setParam("/nerian_stereo/trigger_0_enabled", cfg["trigger_0_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/trigger_0_pulse_width", cfg["trigger_0_pulse_width"].getValue<double>());
        nh.setParam("/nerian_stereo/trigger_1_enabled", cfg["trigger_1_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/trigger_1_offset", cfg["trigger_1_offset"].getValue<double>());
        nh.setParam("/nerian_stereo/trigger_1_pulse_width", cfg["trigger_1_pulse_width"].getValue<double>());
        nh.setParam("/nerian_stereo/trigger_frequency", cfg["trigger_frequency"].getValue<double>());
        nh.setParam("/nerian_stereo/uniqueness_check_enabled", cfg["uniqueness_check_enabled"].getValue<bool>());
        nh.setParam("/nerian_stereo/uniqueness_check_sensitivity", cfg["uniqueness_check_sensitivity"].getValue<int>());
    }

    void updateConfigFromDevice(std::map<std::string, ParameterInfo>& cfg)
    {

        nerian_stereo::NerianStereoConfig config_default, config_min, config_max;
        // defaults
        config_default.auto_exposure_mode = cfg["auto_exposure_mode"].getValue<int>();
        config_default.auto_exposure_roi_enabled = cfg["auto_exposure_roi_enabled"].getValue<bool>();
        config_default.auto_exposure_roi_height = cfg["auto_exposure_roi_height"].getValue<int>();
        config_default.auto_exposure_roi_width = cfg["auto_exposure_roi_width"].getValue<int>();
        config_default.auto_exposure_roi_x = cfg["auto_exposure_roi_x"].getValue<int>();
        config_default.auto_exposure_roi_y = cfg["auto_exposure_roi_y"].getValue<int>();
        config_default.auto_intensity_delta = cfg["auto_intensity_delta"].getValue<double>();
        config_default.auto_maximum_exposure_time = cfg["auto_maximum_exposure_time"].getValue<double>();
        config_default.auto_maximum_gain = cfg["auto_maximum_gain"].getValue<double>();
        config_default.auto_recalibration_enabled = cfg["auto_recalibration_enabled"].getValue<bool>();
        config_default.auto_recalibration_permanent = cfg["auto_recalibration_permanent"].getValue<bool>();
        config_default.auto_skipped_frames = cfg["auto_skipped_frames"].getValue<int>();
        config_default.auto_target_frame = cfg["auto_target_frame"].getValue<int>();
        config_default.auto_target_intensity = cfg["auto_target_intensity"].getValue<double>();
        config_default.consistency_check_enabled = cfg["consistency_check_enabled"].getValue<bool>();
        config_default.consistency_check_sensitivity = cfg["consistency_check_sensitivity"].getValue<int>();
        config_default.disparity_offset = cfg["disparity_offset"].getValue<int>();
        config_default.gap_interpolation_enabled = cfg["gap_interpolation_enabled"].getValue<bool>();
        config_default.manual_exposure_time = cfg["manual_exposure_time"].getValue<double>();
        config_default.manual_gain = cfg["manual_gain"].getValue<double>();
        config_default.mask_border_pixels_enabled = cfg["mask_border_pixels_enabled"].getValue<bool>();
        config_default.max_frame_time_difference_ms = cfg["max_frame_time_difference_ms"].getValue<int>();
        config_default.noise_reduction_enabled = cfg["noise_reduction_enabled"].getValue<bool>();
        config_default.number_of_disparities = cfg["number_of_disparities"].getValue<int>();
        config_default.operation_mode = cfg["operation_mode"].getValue<int>();
        config_default.reboot = cfg["reboot"].getValue<bool>();
        config_default.sgm_p1 = cfg["sgm_p1"].getValue<int>();
        config_default.sgm_p2 = cfg["sgm_p2"].getValue<int>();
        config_default.speckle_filter_iterations = cfg["speckle_filter_iterations"].getValue<int>();
        config_default.texture_filter_enabled = cfg["texture_filter_enabled"].getValue<bool>();
        config_default.texture_filter_sensitivity = cfg["texture_filter_sensitivity"].getValue<int>();
        config_default.trigger_0_enabled = cfg["trigger_0_enabled"].getValue<bool>();
        config_default.trigger_0_pulse_width = cfg["trigger_0_pulse_width"].getValue<double>();
        config_default.trigger_1_enabled = cfg["trigger_1_enabled"].getValue<bool>();
        config_default.trigger_1_offset = cfg["trigger_1_offset"].getValue<double>();
        config_default.trigger_1_pulse_width = cfg["trigger_1_pulse_width"].getValue<double>();
        config_default.trigger_frequency = cfg["trigger_frequency"].getValue<double>();
        config_default.uniqueness_check_enabled = cfg["uniqueness_check_enabled"].getValue<bool>();
        config_default.uniqueness_check_sensitivity = cfg["uniqueness_check_sensitivity"].getValue<int>();
        // min
        config_min.auto_exposure_mode = cfg["auto_exposure_mode"].getMin<int>();
        config_min.auto_exposure_roi_enabled = cfg["auto_exposure_roi_enabled"].getMin<bool>();
        config_min.auto_exposure_roi_height = cfg["auto_exposure_roi_height"].getMin<int>();
        config_min.auto_exposure_roi_width = cfg["auto_exposure_roi_width"].getMin<int>();
        config_min.auto_exposure_roi_x = cfg["auto_exposure_roi_x"].getMin<int>();
        config_min.auto_exposure_roi_y = cfg["auto_exposure_roi_y"].getMin<int>();
        config_min.auto_intensity_delta = cfg["auto_intensity_delta"].getMin<double>();
        config_min.auto_maximum_exposure_time = cfg["auto_maximum_exposure_time"].getMin<double>();
        config_min.auto_maximum_gain = cfg["auto_maximum_gain"].getMin<double>();
        config_min.auto_recalibration_enabled = cfg["auto_recalibration_enabled"].getMin<bool>();
        config_min.auto_recalibration_permanent = cfg["auto_recalibration_permanent"].getMin<bool>();
        config_min.auto_skipped_frames = cfg["auto_skipped_frames"].getMin<int>();
        config_min.auto_target_frame = cfg["auto_target_frame"].getMin<int>();
        config_min.auto_target_intensity = cfg["auto_target_intensity"].getMin<double>();
        config_min.consistency_check_enabled = cfg["consistency_check_enabled"].getMin<bool>();
        config_min.consistency_check_sensitivity = cfg["consistency_check_sensitivity"].getMin<int>();
        config_min.disparity_offset = cfg["disparity_offset"].getMin<int>();
        config_min.gap_interpolation_enabled = cfg["gap_interpolation_enabled"].getMin<bool>();
        config_min.manual_exposure_time = cfg["manual_exposure_time"].getMin<double>();
        config_min.manual_gain = cfg["manual_gain"].getMin<double>();
        config_min.mask_border_pixels_enabled = cfg["mask_border_pixels_enabled"].getMin<bool>();
        config_min.max_frame_time_difference_ms = cfg["max_frame_time_difference_ms"].getMin<int>();
        config_min.noise_reduction_enabled = cfg["noise_reduction_enabled"].getMin<bool>();
        config_min.number_of_disparities = cfg["number_of_disparities"].getMin<int>();
        config_min.operation_mode = cfg["operation_mode"].getMin<int>();
        config_min.reboot = cfg["reboot"].getMin<bool>();
        config_min.sgm_p1 = cfg["sgm_p1"].getMin<int>();
        config_min.sgm_p2 = cfg["sgm_p2"].getMin<int>();
        config_min.speckle_filter_iterations = cfg["speckle_filter_iterations"].getMin<int>();
        config_min.texture_filter_enabled = cfg["texture_filter_enabled"].getMin<bool>();
        config_min.texture_filter_sensitivity = cfg["texture_filter_sensitivity"].getMin<int>();
        config_min.trigger_0_enabled = cfg["trigger_0_enabled"].getMin<bool>();
        config_min.trigger_0_pulse_width = cfg["trigger_0_pulse_width"].getMin<double>();
        config_min.trigger_1_enabled = cfg["trigger_1_enabled"].getMin<bool>();
        config_min.trigger_1_offset = cfg["trigger_1_offset"].getMin<double>();
        config_min.trigger_1_pulse_width = cfg["trigger_1_pulse_width"].getMin<double>();
        config_min.trigger_frequency = cfg["trigger_frequency"].getMin<double>();
        config_min.uniqueness_check_enabled = cfg["uniqueness_check_enabled"].getMin<bool>();
        config_min.uniqueness_check_sensitivity = cfg["uniqueness_check_sensitivity"].getMin<int>();
        // max
        config_max.auto_exposure_mode = cfg["auto_exposure_mode"].getMax<int>();
        config_max.auto_exposure_roi_enabled = cfg["auto_exposure_roi_enabled"].getMax<bool>();
        config_max.auto_exposure_roi_height = cfg["auto_exposure_roi_height"].getMax<int>();
        config_max.auto_exposure_roi_width = cfg["auto_exposure_roi_width"].getMax<int>();
        config_max.auto_exposure_roi_x = cfg["auto_exposure_roi_x"].getMax<int>();
        config_max.auto_exposure_roi_y = cfg["auto_exposure_roi_y"].getMax<int>();
        config_max.auto_intensity_delta = cfg["auto_intensity_delta"].getMax<double>();
        config_max.auto_maximum_exposure_time = cfg["auto_maximum_exposure_time"].getMax<double>();
        config_max.auto_maximum_gain = cfg["auto_maximum_gain"].getMax<double>();
        config_max.auto_recalibration_enabled = cfg["auto_recalibration_enabled"].getMax<bool>();
        config_max.auto_recalibration_permanent = cfg["auto_recalibration_permanent"].getMax<bool>();
        config_max.auto_skipped_frames = cfg["auto_skipped_frames"].getMax<int>();
        config_max.auto_target_frame = cfg["auto_target_frame"].getMax<int>();
        config_max.auto_target_intensity = cfg["auto_target_intensity"].getMax<double>();
        config_max.consistency_check_enabled = cfg["consistency_check_enabled"].getMax<bool>();
        config_max.consistency_check_sensitivity = cfg["consistency_check_sensitivity"].getMax<int>();
        config_max.disparity_offset = cfg["disparity_offset"].getMax<int>();
        config_max.gap_interpolation_enabled = cfg["gap_interpolation_enabled"].getMax<bool>();
        config_max.manual_exposure_time = cfg["manual_exposure_time"].getMax<double>();
        config_max.manual_gain = cfg["manual_gain"].getMax<double>();
        config_max.mask_border_pixels_enabled = cfg["mask_border_pixels_enabled"].getMax<bool>();
        config_max.max_frame_time_difference_ms = cfg["max_frame_time_difference_ms"].getMax<int>();
        config_max.noise_reduction_enabled = cfg["noise_reduction_enabled"].getMax<bool>();
        config_max.number_of_disparities = cfg["number_of_disparities"].getMax<int>();
        config_max.operation_mode = cfg["operation_mode"].getMax<int>();
        config_max.reboot = cfg["reboot"].getMax<bool>();
        config_max.sgm_p1 = cfg["sgm_p1"].getMax<int>();
        config_max.sgm_p2 = cfg["sgm_p2"].getMax<int>();
        config_max.speckle_filter_iterations = cfg["speckle_filter_iterations"].getMax<int>();
        config_max.texture_filter_enabled = cfg["texture_filter_enabled"].getMax<bool>();
        config_max.texture_filter_sensitivity = cfg["texture_filter_sensitivity"].getMax<int>();
        config_max.trigger_0_enabled = cfg["trigger_0_enabled"].getMax<bool>();
        config_max.trigger_0_pulse_width = cfg["trigger_0_pulse_width"].getMax<double>();
        config_max.trigger_1_enabled = cfg["trigger_1_enabled"].getMax<bool>();
        config_max.trigger_1_offset = cfg["trigger_1_offset"].getMax<double>();
        config_max.trigger_1_pulse_width = cfg["trigger_1_pulse_width"].getMax<double>();
        config_max.trigger_frequency = cfg["trigger_frequency"].getMax<double>();
        config_max.uniqueness_check_enabled = cfg["uniqueness_check_enabled"].getMax<bool>();
        config_max.uniqueness_check_sensitivity = cfg["uniqueness_check_sensitivity"].getMax<int>();
        // publish them
        dynReconfServer->setConfigDefault(config_default);
        dynReconfServer->setConfigMax(config_max);
        dynReconfServer->setConfigMin(config_min);
    }
    /*
     * \brief Initialize and publish configuration with a dynamic_reconfigure server
     */
    void initDynamicReconfigure()
    {
        // Connect to parameter server on device
        sceneScanParameters.reset(new SceneScanParameters(remoteHost.c_str()));
        std::map<std::string, ParameterInfo> ssParams = sceneScanParameters->getAllParameters();
        // First make sure that the parameter server gets all *current* values
        updateParameterServerFromDevice(ssParams);
        // Initialize (and publish) initial configuration from compile-time generated header
        dynReconfServer.reset(new dynamic_reconfigure::Server<nerian_stereo::NerianStereoConfig>());
        // Obtain and publish the actual current configuration from the device
        updateConfigFromDevice(ssParams);
        // hook up callback for future ROS-side changes
        dynReconfServer->setCallback(boost::bind(&StereoNode::dynamicReconfigureCallback, this, _1, _2));
    }

    /**
     * \brief Performs general initializations
     */
    void init() {
        ros::NodeHandle privateNh("~");

        // Read all ROS parameters
        std::string intensityChannel = "mono8";
        privateNh.getParam("point_cloud_intensity_channel", intensityChannel);
        if(intensityChannel == "none") {
            pointCloudColorMode = NONE;
        } else if(intensityChannel == "rgb8") {
            pointCloudColorMode = RGB_COMBINED;
        } else if(intensityChannel == "rgb32f") {
            pointCloudColorMode = RGB_SEPARATE;
        } else {
            pointCloudColorMode = INTENSITY;
        }

        if (!privateNh.getParam("color_code_disparity_map", colorCodeDispMap)) {
            colorCodeDispMap = "";
        }

        if (!privateNh.getParam("color_code_legend", colorCodeLegend)) {
            colorCodeLegend = false;
        }

        if (!privateNh.getParam("frame", frame)) {
            frame = "world";
        }

        if (!privateNh.getParam("remote_port", remotePort)) {
            remotePort = "7681";
        }

        if (!privateNh.getParam("remote_host", remoteHost)) {
            remoteHost = "0.0.0.0";
        }

        if (!privateNh.getParam("use_tcp", useTcp)) {
            useTcp = false;
        }

        if (!privateNh.getParam("ros_coordinate_system", rosCoordinateSystem)) {
            rosCoordinateSystem = true;
        }

        if (!privateNh.getParam("ros_timestamps", rosTimestamps)) {
            rosTimestamps = true;
        }

        if (!privateNh.getParam("calibration_file", calibFile)) {
            calibFile = "";
        }

        if (!privateNh.getParam("delay_execution", execDelay)) {
            execDelay = 0;
        }

        if (!privateNh.getParam("max_depth", maxDepth)) {
            maxDepth = -1;
        }

        if (!privateNh.getParam("q_from_calib_file", useQFromCalibFile)) {
            useQFromCalibFile = false;
        }

        // Apply an initial delay if configured
        ros::Duration(execDelay).sleep();

        // Create publishers
        disparityPublisher.reset(new ros::Publisher(nh.advertise<sensor_msgs::Image>(
            "/nerian_stereo/disparity_map", 5)));
        leftImagePublisher.reset(new ros::Publisher(nh.advertise<sensor_msgs::Image>(
            "/nerian_stereo/left_image", 5)));
        rightImagePublisher.reset(new ros::Publisher(nh.advertise<sensor_msgs::Image>(
            "/nerian_stereo/right_image", 5)));

        loadCameraCalibration();

        cameraInfoPublisher.reset(new ros::Publisher(nh.advertise<nerian_stereo::StereoCameraInfo>(
            "/nerian_stereo/stereo_camera_info", 1)));
        cloudPublisher.reset(new ros::Publisher(nh.advertise<sensor_msgs::PointCloud2>(
            "/nerian_stereo/point_cloud", 5)));
    }

    /**
     * \brief The main loop of this node
     */
    int run() {
        try {
            ros::Time lastLogTime;
            int lastLogFrames = 0;

            AsyncTransfer asyncTransfer(remoteHost.c_str(), remotePort.c_str(),
                useTcp ? ImageProtocol::PROTOCOL_TCP : ImageProtocol::PROTOCOL_UDP);

            while(ros::ok()) {
                // Dispatch any queued ROS callbacks
                ros::spinOnce();

                // Receive image data
                ImagePair imagePair;
                if(!asyncTransfer.collectReceivedImagePair(imagePair, 0.5)) {
                    continue;
                }

                // Get time stamp
                ros::Time stamp;
                if(rosTimestamps) {
                    stamp = ros::Time::now();
                } else {
                    int secs = 0, microsecs = 0;
                    imagePair.getTimestamp(secs, microsecs);
                    stamp = ros::Time(secs, microsecs*1000);
                }

                // Publish image data messages
                publishImageMsg(imagePair, 0, stamp, false, leftImagePublisher.get());
                if(imagePair.isImageDisparityPair()) {
                    publishImageMsg(imagePair, 1, stamp, true, disparityPublisher.get());
                } else {
                    publishImageMsg(imagePair, 1, stamp, false, rightImagePublisher.get());
                }

                if(cloudPublisher->getNumSubscribers() > 0) {
                    if(recon3d == nullptr) {
                        // First initialize
                        initPointCloud();
                    }

                    publishPointCloudMsg(imagePair, stamp);
                }

                if(cameraInfoPublisher != NULL && cameraInfoPublisher->getNumSubscribers() > 0) {
                    publishCameraInfo(stamp, imagePair);
                }

                // Display some simple statistics
                frameNum++;
                if(stamp.sec != lastLogTime.sec) {
                    if(lastLogTime != ros::Time()) {
                        double dt = (stamp - lastLogTime).toSec();
                        double fps = (frameNum - lastLogFrames) / dt;
                        ROS_INFO("%.1f fps", fps);
                    }
                    lastLogFrames = frameNum;
                    lastLogTime = stamp;
                }
            }
        } catch(const std::exception& ex) {
            ROS_FATAL("Exception occured: %s", ex.what());
        }
    }

private:
    enum PointCloudColorMode {
        RGB_SEPARATE,
        RGB_COMBINED,
        INTENSITY,
        NONE
    };

    // ROS related objects
    ros::NodeHandle nh;
    boost::scoped_ptr<ros::Publisher> cloudPublisher;
    boost::scoped_ptr<ros::Publisher> disparityPublisher;
    boost::scoped_ptr<ros::Publisher> leftImagePublisher;
    boost::scoped_ptr<ros::Publisher> rightImagePublisher;
    boost::scoped_ptr<ros::Publisher> cameraInfoPublisher;

    // ROS dynamic_reconfigure
    boost::scoped_ptr<dynamic_reconfigure::Server<nerian_stereo::NerianStereoConfig>> dynReconfServer;
    nerian_stereo::NerianStereoConfig lastKnownConfig;
    bool initialConfigReceived;
    
    // Connection to parameter server on device
    boost::scoped_ptr<SceneScanParameters> sceneScanParameters;

    // Parameters
    bool useTcp;
    std::string colorCodeDispMap;
    bool colorCodeLegend;
    bool rosCoordinateSystem;
    bool rosTimestamps;
    std::string remotePort;
    std::string frame;
    std::string remoteHost;
    std::string calibFile;
    double execDelay;
    double maxDepth;
    bool useQFromCalibFile;
    PointCloudColorMode pointCloudColorMode;

    // Other members
    int frameNum;
    boost::scoped_ptr<Reconstruct3D> recon3d;
    boost::scoped_ptr<ColorCoder> colCoder;
    cv::Mat_<cv::Vec3b> colDispMap;
    sensor_msgs::PointCloud2Ptr pointCloudMsg;
    cv::FileStorage calibStorage;
    nerian_stereo::StereoCameraInfoPtr camInfoMsg;
    ros::Time lastCamInfoPublish;

    /**
     * \brief Loads a camera calibration file if configured
     */
    void loadCameraCalibration() {
        if(calibFile == "" ) {
            ROS_WARN("No camera calibration file configured. Cannot publish detailed camera information!");
        } else {
            bool success = false;
            try {
                if (calibStorage.open(calibFile, cv::FileStorage::READ)) {
                    success = true;
                }
            } catch(...) {
            }

            if(!success) {
                ROS_WARN("Error reading calibration file: %s\n"
                    "Cannot publish detailed camera information!", calibFile.c_str());
            }
        }
    }

    /**
     * \brief Publishes the disparity map as 16-bit grayscale image or color coded
     * RGB image
     */
    void publishImageMsg(const ImagePair& imagePair, int imageIndex, ros::Time stamp, bool allowColorCode,
            ros::Publisher* publisher) {

        if(publisher->getNumSubscribers() <= 0) {
            return; //No subscribers
        }

        cv_bridge::CvImage cvImg;
        cvImg.header.frame_id = frame;
        cvImg.header.stamp = stamp;
        cvImg.header.seq = imagePair.getSequenceNumber(); // Actually ROS will overwrite this

        bool format12Bit = (imagePair.getPixelFormat(imageIndex) == ImagePair::FORMAT_12_BIT_MONO);
        cv::Mat monoImg(imagePair.getHeight(), imagePair.getWidth(),
            format12Bit ? CV_16UC1 : CV_8UC1,
            imagePair.getPixelData(imageIndex), imagePair.getRowStride(imageIndex));
        string encoding = "";

        if(colorCodeDispMap == "" || colorCodeDispMap == "none" || !allowColorCode || !format12Bit) {
            cvImg.image = monoImg;
            encoding = (format12Bit ? "mono16": "mono8");
        } else {
            if(colCoder == NULL) {
                int dispMin = 0, dispMax = 0;
                imagePair.getDisparityRange(dispMin, dispMax);

                colCoder.reset(new ColorCoder(
                    colorCodeDispMap == "rainbow" ? ColorCoder::COLOR_RAINBOW_BGR : ColorCoder::COLOR_RED_BLUE_BGR,
                    dispMin*16, dispMax*16, true, true));
                if(colorCodeLegend) {
                    // Create the legend
                    colDispMap = colCoder->createLegendBorder(monoImg.cols, monoImg.rows, 1.0/16.0);
                } else {
                    colDispMap = cv::Mat_<cv::Vec3b>(monoImg.rows, monoImg.cols);
                }
            }

            cv::Mat_<cv::Vec3b> dispSection = colDispMap(cv::Rect(0, 0, monoImg.cols, monoImg.rows));

            colCoder->codeImage(cv::Mat_<unsigned short>(monoImg), dispSection);
            cvImg.image = colDispMap;
            encoding = "bgr8";
        }

        sensor_msgs::ImagePtr msg = cvImg.toImageMsg();
        msg->encoding = encoding;
        publisher->publish(msg);
    }

    /**
     * \brief Transform Q matrix to match the ROS coordinate system:
     * Swap y/z axis, then swap x/y axis, then invert y and z axis.
     */
    void qMatrixToRosCoords(const float* src, float* dst) {
        dst[0] = src[8];   dst[1] = src[9];
        dst[2] = src[10];  dst[3] = src[11];

        dst[4] = -src[0];  dst[5] = -src[1];
        dst[6] = -src[2];  dst[7] = -src[3];

        dst[8] = -src[4];  dst[9] = -src[5];
        dst[10] = -src[6]; dst[11] = -src[7];

        dst[12] = src[12]; dst[13] = src[13];
        dst[14] = src[14]; dst[15] = src[15];
    }

    /**
     * \brief Reconstructs the 3D locations form the disparity map and publishes them
     * as point cloud.
     */
    void publishPointCloudMsg(ImagePair& imagePair, ros::Time stamp) {
        if(imagePair.getPixelFormat(1) != ImagePair::FORMAT_12_BIT_MONO) {
            return; // This is not a disparity map
        }

        // Set static q matrix if desired
        if(useQFromCalibFile) {
            static std::vector<float> q;
            calibStorage["Q"] >> q;
            imagePair.setQMatrix(&q[0]);
        }

        // Transform Q-matrix if desired
        float qRos[16];
        if(rosCoordinateSystem) {
            qMatrixToRosCoords(imagePair.getQMatrix(), qRos);
            imagePair.setQMatrix(qRos);
        }

        // Get 3D points
        float* pointMap = nullptr;
        try {
            pointMap = recon3d->createPointMap(imagePair, 0);
        } catch(std::exception& ex) {
            cerr << "Error creating point cloud: " << ex.what() << endl;
            return;
        }

        // Create message object and set header
        pointCloudMsg->header.stamp = stamp;
        pointCloudMsg->header.frame_id = frame;
        pointCloudMsg->header.seq = imagePair.getSequenceNumber(); // Actually ROS will overwrite this

        // Copy 3D points
        if(pointCloudMsg->data.size() != imagePair.getWidth()*imagePair.getHeight()*4*sizeof(float)) {
            // Allocate buffer
            pointCloudMsg->data.resize(imagePair.getWidth()*imagePair.getHeight()*4*sizeof(float));

            // Set basic data
            pointCloudMsg->width = imagePair.getWidth();
            pointCloudMsg->height = imagePair.getHeight();
            pointCloudMsg->is_bigendian = false;
            pointCloudMsg->point_step = 4*sizeof(float);
            pointCloudMsg->row_step = imagePair.getWidth() * pointCloudMsg->point_step;
            pointCloudMsg->is_dense = false;
        }

        if(maxDepth < 0) {
            // Just copy everything
            memcpy(&pointCloudMsg->data[0], pointMap,
                imagePair.getWidth()*imagePair.getHeight()*4*sizeof(float));
        } else {
            // Only copy points up to maximum depth
            if(rosCoordinateSystem) {
                copyPointCloudClamped<0>(pointMap, reinterpret_cast<float*>(&pointCloudMsg->data[0]),
                    imagePair.getWidth()*imagePair.getHeight());
            } else {
                copyPointCloudClamped<2>(pointMap, reinterpret_cast<float*>(&pointCloudMsg->data[0]),
                    imagePair.getWidth()*imagePair.getHeight());
            }
        }

        // Copy intensity values
        switch(pointCloudColorMode) {
            case INTENSITY:
                copyPointCloudIntensity<INTENSITY>(imagePair);
                break;
            case RGB_COMBINED:
                copyPointCloudIntensity<RGB_COMBINED>(imagePair);
                break;
            case RGB_SEPARATE:
                copyPointCloudIntensity<RGB_SEPARATE>(imagePair);
                break;
        }

        cloudPublisher->publish(pointCloudMsg);
    }

    /**
     * \brief Copies the intensity or RGB data to the point cloud
     */
    template <PointCloudColorMode colorMode> void copyPointCloudIntensity(ImagePair& imagePair) {
        // Get pointers to the beginnig and end of the point cloud
        unsigned char* cloudStart = &pointCloudMsg->data[0];
        unsigned char* cloudEnd = &pointCloudMsg->data[0]
            + imagePair.getWidth()*imagePair.getHeight()*4*sizeof(float);

        if(imagePair.getPixelFormat(0) == ImagePair::FORMAT_8_BIT_MONO) {
            // Get pointer to the current pixel and end of current row
            unsigned char* imagePtr = imagePair.getPixelData(0);
            unsigned char* rowEndPtr = imagePtr + imagePair.getWidth();
            int rowIncrement = imagePair.getRowStride(0) - imagePair.getWidth();

            for(unsigned char* cloudPtr = cloudStart + 3*sizeof(float);
                    cloudPtr < cloudEnd; cloudPtr+= 4*sizeof(float)) {
                if(colorMode == RGB_SEPARATE) {// RGB as float
                    *reinterpret_cast<float*>(cloudPtr) = static_cast<float>(*imagePtr) / 255.0F;
                } else if(colorMode == RGB_COMBINED) {// RGB as integer
                    const unsigned char intensity = *imagePtr;
                    *reinterpret_cast<unsigned int*>(cloudPtr) = (intensity << 16) | (intensity << 8) | intensity;
                } else {
                    *cloudPtr = *imagePtr;
                }

                imagePtr++;
                if(imagePtr == rowEndPtr) {
                    // Progress to next row
                    imagePtr += rowIncrement;
                    rowEndPtr = imagePtr + imagePair.getWidth();
                }
            }
        } else if(imagePair.getPixelFormat(0) == ImagePair::FORMAT_12_BIT_MONO) {
            // Get pointer to the current pixel and end of current row
            unsigned short* imagePtr = reinterpret_cast<unsigned short*>(imagePair.getPixelData(0));
            unsigned short* rowEndPtr = imagePtr + imagePair.getWidth();
            int rowIncrement = imagePair.getRowStride(0) - 2*imagePair.getWidth();

            for(unsigned char* cloudPtr = cloudStart + 3*sizeof(float);
                    cloudPtr < cloudEnd; cloudPtr+= 4*sizeof(float)) {

                if(colorMode == RGB_SEPARATE) {// RGB as float
                    *reinterpret_cast<float*>(cloudPtr) = static_cast<float>(*imagePtr) / 4095.0F;
                } else if(colorMode == RGB_COMBINED) {// RGB as integer
                    const unsigned char intensity = *imagePtr/16;
                    *reinterpret_cast<unsigned int*>(cloudPtr) = (intensity << 16) | (intensity << 8) | intensity;
                } else {
                    *cloudPtr = *imagePtr/16;
                }

                imagePtr++;
                if(imagePtr == rowEndPtr) {
                    // Progress to next row
                    imagePtr += rowIncrement;
                    rowEndPtr = imagePtr + imagePair.getWidth();
                }
            }
        } else if(imagePair.getPixelFormat(0) == ImagePair::FORMAT_8_BIT_RGB) {
            // Get pointer to the current pixel and end of current row
            unsigned char* imagePtr = imagePair.getPixelData(0);
            unsigned char* rowEndPtr = imagePtr + imagePair.getWidth();
            int rowIncrement = imagePair.getRowStride(0) - imagePair.getWidth();

            static bool warned = false;
            if(colorMode == RGB_SEPARATE && !warned) {
                warned = true;
                ROS_WARN("RGBF32 is not supported for color images. Please use RGB8!");
            }

            for(unsigned char* cloudPtr = cloudStart + 3*sizeof(float);
                    cloudPtr < cloudEnd; cloudPtr+= 4*sizeof(float)) {
                if(colorMode == RGB_SEPARATE) {// RGB as float
                    *reinterpret_cast<float*>(cloudPtr) = static_cast<float>(imagePtr[2]) / 255.0F;
                } else if(colorMode == RGB_COMBINED) {// RGB as integer
                    *reinterpret_cast<unsigned int*>(cloudPtr) = (imagePtr[0] << 16) | (imagePtr[1] << 8) | imagePtr[2];
                } else {
                    *cloudPtr = (imagePtr[0] + imagePtr[1]*2 + imagePtr[2])/4;
                }

                imagePtr+=3;
                if(imagePtr == rowEndPtr) {
                    // Progress to next row
                    imagePtr += rowIncrement;
                    rowEndPtr = imagePtr + imagePair.getWidth();
                }
            }
        } else {
            throw std::runtime_error("Invalid pixel format!");
        }
    }

    /**
     * \brief Copies all points in a point cloud that have a depth smaller
     * than maxDepth. Other points are set to NaN.
     */
    template <int coord> void copyPointCloudClamped(float* src, float* dst, int size) {
        // Only copy points that are below the minimum depth
        float* endPtr = src + 4*size;
        for(float *srcPtr = src, *dstPtr = dst; srcPtr < endPtr; srcPtr+=4, dstPtr+=4) {
            if(srcPtr[coord] > maxDepth) {
                dstPtr[0] = std::numeric_limits<float>::quiet_NaN();
                dstPtr[1] = std::numeric_limits<float>::quiet_NaN();
                dstPtr[2] = std::numeric_limits<float>::quiet_NaN();
            } else {
                dstPtr[0] = srcPtr[0];
                dstPtr[1] = srcPtr[1];
                dstPtr[2] = srcPtr[2];
            }
        }
    }

    /**
     * \brief Performs all neccessary initializations for point cloud+
     * publishing
     */
    void initPointCloud() {
        ros::NodeHandle privateNh("~");

        // Initialize 3D reconstruction class
        recon3d.reset(new Reconstruct3D);

        // Initialize message
        pointCloudMsg.reset(new sensor_msgs::PointCloud2);

        // Set channel information.
        sensor_msgs::PointField fieldX;
        fieldX.name ="x";
        fieldX.offset = 0;
        fieldX.datatype = sensor_msgs::PointField::FLOAT32;
        fieldX.count = 1;
        pointCloudMsg->fields.push_back(fieldX);

        sensor_msgs::PointField fieldY;
        fieldY.name ="y";
        fieldY.offset = sizeof(float);
        fieldY.datatype = sensor_msgs::PointField::FLOAT32;
        fieldY.count = 1;
        pointCloudMsg->fields.push_back(fieldY);

        sensor_msgs::PointField fieldZ;
        fieldZ.name ="z";
        fieldZ.offset = 2*sizeof(float);
        fieldZ.datatype = sensor_msgs::PointField::FLOAT32;
        fieldZ.count = 1;
        pointCloudMsg->fields.push_back(fieldZ);

        if(pointCloudColorMode == INTENSITY) {
            sensor_msgs::PointField fieldI;
            fieldI.name ="intensity";
            fieldI.offset = 3*sizeof(float);
            fieldI.datatype = sensor_msgs::PointField::UINT8;
            fieldI.count = 1;
            pointCloudMsg->fields.push_back(fieldI);
        }
        else if(pointCloudColorMode == RGB_SEPARATE) {
            sensor_msgs::PointField fieldRed;
            fieldRed.name ="r";
            fieldRed.offset = 3*sizeof(float);
            fieldRed.datatype = sensor_msgs::PointField::FLOAT32;
            fieldRed.count = 1;
            pointCloudMsg->fields.push_back(fieldRed);

            sensor_msgs::PointField fieldGreen;
            fieldGreen.name ="g";
            fieldGreen.offset = 3*sizeof(float);
            fieldGreen.datatype = sensor_msgs::PointField::FLOAT32;
            fieldGreen.count = 1;
            pointCloudMsg->fields.push_back(fieldGreen);

            sensor_msgs::PointField fieldBlue;
            fieldBlue.name ="b";
            fieldBlue.offset = 3*sizeof(float);
            fieldBlue.datatype = sensor_msgs::PointField::FLOAT32;
            fieldBlue.count = 1;
            pointCloudMsg->fields.push_back(fieldBlue);
        } else if(pointCloudColorMode == RGB_COMBINED) {
            sensor_msgs::PointField fieldRGB;
            fieldRGB.name ="rgb";
            fieldRGB.offset = 3*sizeof(float);
            fieldRGB.datatype = sensor_msgs::PointField::UINT32;
            fieldRGB.count = 1;
            pointCloudMsg->fields.push_back(fieldRGB);
        }
    }

    /**
     * \brief Publishes the camera info once per second
     */
    void publishCameraInfo(ros::Time stamp, const ImagePair& imagePair) {
        if(camInfoMsg == NULL) {
            // Initialize the camera info structure
            camInfoMsg.reset(new nerian_stereo::StereoCameraInfo);

            camInfoMsg->header.frame_id = frame;
            camInfoMsg->header.seq = imagePair.getSequenceNumber(); // Actually ROS will overwrite this

            if(calibFile != "") {
                std::vector<int> sizeVec;
                calibStorage["size"] >> sizeVec;
                if(sizeVec.size() != 2) {
                    std::runtime_error("Calibration file format error!");
                }

                camInfoMsg->left_info.header = camInfoMsg->header;
                camInfoMsg->left_info.width = sizeVec[0];
                camInfoMsg->left_info.height = sizeVec[1];
                camInfoMsg->left_info.distortion_model = "plumb_bob";
                calibStorage["D1"] >> camInfoMsg->left_info.D;
                readCalibrationArray("M1", camInfoMsg->left_info.K);
                readCalibrationArray("R1", camInfoMsg->left_info.R);
                readCalibrationArray("P1", camInfoMsg->left_info.P);
                camInfoMsg->left_info.binning_x = 1;
                camInfoMsg->left_info.binning_y = 1;
                camInfoMsg->left_info.roi.do_rectify = false;
                camInfoMsg->left_info.roi.height = 0;
                camInfoMsg->left_info.roi.width = 0;
                camInfoMsg->left_info.roi.x_offset = 0;
                camInfoMsg->left_info.roi.y_offset = 0;

                camInfoMsg->right_info.header = camInfoMsg->header;
                camInfoMsg->right_info.width = sizeVec[0];
                camInfoMsg->right_info.height = sizeVec[1];
                camInfoMsg->right_info.distortion_model = "plumb_bob";
                calibStorage["D2"] >> camInfoMsg->right_info.D;
                readCalibrationArray("M2", camInfoMsg->right_info.K);
                readCalibrationArray("R2", camInfoMsg->right_info.R);
                readCalibrationArray("P2", camInfoMsg->right_info.P);
                camInfoMsg->right_info.binning_x = 1;
                camInfoMsg->right_info.binning_y = 1;
                camInfoMsg->right_info.roi.do_rectify = false;
                camInfoMsg->right_info.roi.height = 0;
                camInfoMsg->right_info.roi.width = 0;
                camInfoMsg->right_info.roi.x_offset = 0;
                camInfoMsg->right_info.roi.y_offset = 0;

                readCalibrationArray("Q", camInfoMsg->Q);
                readCalibrationArray("T", camInfoMsg->T_left_right);
                readCalibrationArray("R", camInfoMsg->R_left_right);
            }
        }

        double dt = (stamp - lastCamInfoPublish).toSec();
        if(dt > 1.0) {
            // Rather use the Q-matrix that we received over the network if it is valid
            const float* qMatrix = imagePair.getQMatrix();
            if(qMatrix[0] != 0.0) {
                for(int i=0; i<16; i++) {
                    camInfoMsg->Q[i] = static_cast<double>(qMatrix[i]);
                }
            }

            // Publish once per second
            camInfoMsg->header.stamp = stamp;
            camInfoMsg->left_info.header.stamp = stamp;
            camInfoMsg->right_info.header.stamp = stamp;
            cameraInfoPublisher->publish(camInfoMsg);

            lastCamInfoPublish = stamp;
        }
    }

    /**
     * \brief Reads a vector from the calibration file to a boost:array
     */
    template<class T>
    void readCalibrationArray(const char* key, T& dest) {
        std::vector<double> doubleVec;
        calibStorage[key] >> doubleVec;

        if(doubleVec.size() != dest.size()) {
            std::runtime_error("Calibration file format error!");
        }

        std::copy(doubleVec.begin(), doubleVec.end(), dest.begin());
    }
};

int main(int argc, char** argv) {
    try {
        ros::init(argc, argv, "nerian_stereo");
        StereoNode node;
        node.init();
        node.initDynamicReconfigure();
        return node.run();
    } catch(const std::exception& ex) {
        ROS_FATAL("Exception occured: %s", ex.what());
        return 1;
    }
}
