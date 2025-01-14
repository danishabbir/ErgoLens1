// ------------------------- OpenPose C++ API Tutorial - Example 13 - Custom Input -------------------------
// Synchronous mode: ideal for production integration. It provides the fastest results with respect to runtime
// performance.
// Author: Danish,  July

// Command-line user intraface
#define OPENPOSE_FLAGS_DISABLE_PRODUCER
#include <openpose/flags.hpp>
// OpenPose dependencies
#include <openpose/headers.hpp>

// Custom OpenPose flags
// Producer
DEFINE_string(image_dir,                "examples/media/",
    "Process a directory of images. Read all standard formats (jpg, png, bmp, etc.).");

// This work will reader frames synchronously from camera & run 3d 
class WUserInput : public op::WorkerProducer<std::shared_ptr<std::vector<std::shared_ptr<op::Datum>>>>
{
    public:
        WUserInput(std::vector<uint32_t> cam_ids, bool use3d) :
                     mParamReader(std::make_shared<op::CameraParameterReader>()),
                     mUse3d(use3d)
        {
            for (const auto& cam_id : cam_ids) {
                mCams.emplace_back(std::make_shared<op::WebcamReader> (cam_id  ));                
            }
            if (use3d) {
                //calibration directory
                mParamReader->readParameters("/home/whil/Developer/sub/openpose/models/cameraParameters/flir/"); //FLAGS_calibration_dir
                //can just insert matrices here....
                mIntrinsics = mParamReader->getCameraIntrinsics();
                mExtrinsics = mParamReader->getCameraExtrinsics();
                mMatrices = mParamReader->getCameraMatrices();
            }
        }

        void initializationOnThread() {}

        std::shared_ptr<std::vector<std::shared_ptr<op::Datum>>> workProducer()
        {
            try
            {
                /* code */
                std::lock_guard<std::mutex> g(lock);
                if (mBlocked.empty()) {
                    for (size_t i = 0; i < mCams.size(); i++) {
                        auto datumsPtr = std::make_shared<std::vector<std::shared_ptr<op::Datum>>>();
                        //create new datum
                        datumsPtr->emplace_back();
                        auto& datum = datumsPtr->back();
                        datum = std::make_shared<op::Datum>();

                        //Fill datum

                        datum->cvInputData = mCams[i]->getFrame();


                        datum->cvOutputData = datum->cvInputData;
                        datum->subId = i;
                        datum->subIdMax = mCams.size() - 1;
                        if(mUse3d) {
                            datum->cameraIntrinsics = mIntrinsics[i];
                            datum->cameraExtrinsics = mExtrinsics[i];
                            datum->cameraMatrix = mMatrices[i];
                        }
                        //if empty frame-> return null
                        if (datum->cvInputData.empty()) 
                        {
                            this->stop();
                            return nullptr;
                        }
                        mBlocked.push(datumsPtr);
                    }        
                }

                auto ret = mBlocked.front();
                mBlocked.pop();
                return ret;
            }
            catch(const std::exception& e)
            {
                this->stop();
                op::error(e.what(), __LINE__, __FUNCTION__, __FILE__);
                return nullptr;
            }
            
        }
        private :
            bool mUse3d;
            std::shared_ptr<op::CameraParameterReader> mParamReader;
            std::vector<cv::Mat> mIntrinsics;
            std::vector<cv::Mat> mExtrinsics;
            std::vector<cv::Mat> mMatrices;
            std::vector<std::shared_ptr<op::WebcamReader>> mCams;
            std::queue<std::shared_ptr<std::vector<std::shared_ptr<op::Datum>>>> mBlocked;
            std::mutex lock;
};






void configureWrapper(op::Wrapper& opWrapper)
{
    try
    {
        // Configuring OpenPose

        // logging_level
        op::check(0 <= FLAGS_logging_level && FLAGS_logging_level <= 255, "Wrong logging_level value.",
                  __LINE__, __FUNCTION__, __FILE__);
        op::ConfigureLog::setPriorityThreshold((op::Priority)FLAGS_logging_level);
        op::Profiler::setDefaultX(FLAGS_profile_speed);

        // Applying user defined configuration - GFlags to program variables
        // outputSize
        const auto outputSize = op::flagsToPoint(FLAGS_output_resolution, "-1x-1");
        // netInputSize
        const auto netInputSize = op::flagsToPoint(FLAGS_net_resolution, "-1x368");
        // faceNetInputSize
        const auto faceNetInputSize = op::flagsToPoint(FLAGS_face_net_resolution, "368x368 (multiples of 16)");
        // handNetInputSize
        const auto handNetInputSize = op::flagsToPoint(FLAGS_hand_net_resolution, "368x368 (multiples of 16)");
        // poseMode
        const auto poseMode = op::flagsToPoseMode(FLAGS_body);
        // poseModel
        const auto poseModel = op::flagsToPoseModel(FLAGS_model_pose);
        // JSON saving
        if (!FLAGS_write_keypoint.empty())
            op::log("Flag `write_keypoint` is deprecated and will eventually be removed."
                    " Please, use `write_json` instead.", op::Priority::Max);
        // keypointScaleMode
        const auto keypointScaleMode = op::flagsToScaleMode(FLAGS_keypoint_scale);
        // heatmaps to add
        const auto heatMapTypes = op::flagsToHeatMaps(FLAGS_heatmaps_add_parts, FLAGS_heatmaps_add_bkg,
                                                      FLAGS_heatmaps_add_PAFs);
        const auto heatMapScaleMode = op::flagsToHeatMapScaleMode(FLAGS_heatmaps_scale);
        // >1 camera view?
        // const auto multipleView = (FLAGS_3d || FLAGS_3d_views > 1 || FLAGS_flir_camera);
        const auto multipleView = false;
        // Face and hand detectors
        const auto faceDetector = op::flagsToDetector(FLAGS_face_detector);
        const auto handDetector = op::flagsToDetector(FLAGS_hand_detector);
        // Enabling Google Logging
        const bool enableGoogleLogging = true;

        // Initializing the user custom classes
        // Frames producer (e.g., video, webcam, ...)
        auto wUserInput = std::make_shared<WUserInput>(std::vector<uint32_t>{0,2, 4, 6}, FLAGS_3d);
        // Add custom processing
        const auto workerInputOnNewThread = true;
        opWrapper.setWorker(op::WorkerType::Input, wUserInput, workerInputOnNewThread);

        // Pose configuration (use WrapperStructPose{} for default and recommended configuration)
        const op::WrapperStructPose wrapperStructPose{
            poseMode, netInputSize, outputSize, keypointScaleMode, FLAGS_num_gpu, FLAGS_num_gpu_start,
            FLAGS_scale_number, (float)FLAGS_scale_gap, op::flagsToRenderMode(FLAGS_render_pose, multipleView),
            poseModel, !FLAGS_disable_blending, (float)FLAGS_alpha_pose, (float)FLAGS_alpha_heatmap,
            FLAGS_part_to_show, FLAGS_model_folder, heatMapTypes, heatMapScaleMode, FLAGS_part_candidates,
            (float)FLAGS_render_threshold, FLAGS_number_people_max, FLAGS_maximize_positives, FLAGS_fps_max,
            FLAGS_prototxt_path, FLAGS_caffemodel_path, (float)FLAGS_upsampling_ratio, enableGoogleLogging};
        opWrapper.configure(wrapperStructPose);
        // Face configuration (use op::WrapperStructFace{} to disable it)
        const op::WrapperStructFace wrapperStructFace{
            FLAGS_face, faceDetector, faceNetInputSize,
            op::flagsToRenderMode(FLAGS_face_render, multipleView, FLAGS_render_pose),
            (float)FLAGS_face_alpha_pose, (float)FLAGS_face_alpha_heatmap, (float)FLAGS_face_render_threshold};
        opWrapper.configure(wrapperStructFace);
        // Hand configuration (use op::WrapperStructHand{} to disable it)
        const op::WrapperStructHand wrapperStructHand{
            FLAGS_hand, handDetector, handNetInputSize, FLAGS_hand_scale_number, (float)FLAGS_hand_scale_range,
            op::flagsToRenderMode(FLAGS_hand_render, multipleView, FLAGS_render_pose), (float)FLAGS_hand_alpha_pose,
            (float)FLAGS_hand_alpha_heatmap, (float)FLAGS_hand_render_threshold};
        opWrapper.configure(wrapperStructHand);
        // Extra functionality configuration (use op::WrapperStructExtra{} to disable it)
        const op::WrapperStructExtra wrapperStructExtra{
            FLAGS_3d, FLAGS_3d_min_views, FLAGS_identification, FLAGS_tracking, FLAGS_ik_threads};
        opWrapper.configure(wrapperStructExtra);
        // Output (comment or use default argument to disable any output)
        const op::WrapperStructOutput wrapperStructOutput{
            FLAGS_cli_verbose, FLAGS_write_keypoint, op::stringToDataFormat(FLAGS_write_keypoint_format),
            FLAGS_write_json, FLAGS_write_coco_json, FLAGS_write_coco_json_variants, FLAGS_write_coco_json_variant,
            FLAGS_write_images, FLAGS_write_images_format, FLAGS_write_video, FLAGS_write_video_fps,
            FLAGS_write_video_with_audio, FLAGS_write_heatmaps, FLAGS_write_heatmaps_format, FLAGS_write_video_3d,
            FLAGS_write_video_adam, FLAGS_write_bvh, FLAGS_udp_host, FLAGS_udp_port};
        opWrapper.configure(wrapperStructOutput);
        // GUI (comment or use default argument to disable any visual output)
        const op::WrapperStructGui wrapperStructGui{
            op::flagsToDisplayMode(FLAGS_display, FLAGS_3d), !FLAGS_no_gui_verbose, FLAGS_fullscreen};
        opWrapper.configure(wrapperStructGui);
        // Set to single-thread (for sequential processing and/or debugging and/or reducing latency)
        if (FLAGS_disable_multi_thread)
            opWrapper.disableMultiThreading();
    }
    catch (const std::exception& e)
    {
        op::error(e.what(), __LINE__, __FUNCTION__, __FILE__);
    }
}

int tutorialApiCpp()
{
    try
    {
        op::log("Starting OpenPose demo...", op::Priority::High);
        const auto opTimer = op::getTimerInit();

        // OpenPose wrapper
        op::log("Configuring OpenPose...", op::Priority::High);
        op::Wrapper opWrapper;
        configureWrapper(opWrapper);

        // Start, run, and stop processing - exec() blocks this thread until OpenPose wrapper has finished
        op::log("Starting thread(s)...", op::Priority::High);
        opWrapper.exec();

        // Measuring total time
        op::printTime(opTimer, "OpenPose demo successfully finished. Total time: ", " seconds.", op::Priority::High);

        // Return
        return 0;
    }
    catch (const std::exception& e)
    {
        return -1;
    }
}

int main(int argc, char *argv[])
{
    // Parsing command line flags
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // Running tutorialApiCpp
    return tutorialApiCpp();
}