//
// Created by james on 4/4/19.
//

#ifndef SRC_BOBBLECONTROLLERBASE_H
#define SRC_BOBBLECONTROLLERBASE_H

#include <cstddef>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <mutex>

#include <realtime_tools/realtime_publisher.h>
#include <ros/node_handle.h>
#include <topic_tools/shape_shifter.h>
#include <hardware_interface/joint_command_interface.h>
#include <hardware_interface/imu_sensor_interface.h>
#include <controller_interface/controller.h>

namespace bobble_controllers {

    /// Generic Controller Base
    template <class publisherType> class BobbleControllerBase : public controller_interface::
    Controller<hardware_interface::EffortJointInterface> {

    protected:
        BobbleControllerBase(void) {}
        ~BobbleControllerBase(void) {
            runSubscriberThread = false;
            subscriberThread->join();
        }

        /// Node and hardware interface
        ros::NodeHandle node_;
        hardware_interface::RobotHW *robot_;

        /// Realtime status publisher
        realtime_tools::RealtimePublisher<publisherType>* pub_status;
        /// Virtual publising function that has to be implemented by child class
        virtual void publish_status_message() = 0;

        /// Hardware interfaces
        std::vector <hardware_interface::JointHandle> joints_;
        hardware_interface::ImuSensorInterface *imu_;
        hardware_interface::ImuSensorHandle imuData;

        /// Mutex for transferring commands
        std::mutex control_command_mutex;

        /// Thread for managing non-RT subscriber
        std::thread* subscriberThread;
        /// Typedef for subscriber callback functions
        typedef void (*callbackFunctionPtr_t)(const topic_tools::ShapeShifter::ConstPtr &msg);
        /// Vector containing subscriber string to function definitions
        std::map<std::string, callbackFunctionPtr_t> subscribers;
        /// Subscriber frequency
        double subscriberFrequency;

        /// Vector for housing possible control modes
        std::map<std::string, unsigned int> controlModes;
        unsigned int ActiveControlMode;

        /// Vectors for names of elements of control bools and doubles
        std::vector<std::string> controlDoubleNames;
        std::vector<std::string> controlBoolNames;
        /// Vectors for housing possible command transfer elements
        std::map<std::string, double> controlDoubles;
        std::map<std::string, bool> controlBools;
        /// Second copy of vectors for non-RT access - have to be static to be used in callbacks
        static std::map<std::string, double> controlDoublesNoRT;
        static std::map<std::string, bool> controlBoolsNoRT;
        /// Third copy of vectors for RT use
        std::map<std::string, double> controlDoublesRT;
        std::map<std::string, bool> controlBoolsRT;

        /// Bool for telling subscriber thread to finish
        bool runSubscriberThread;
        /// Subscriber thread function
        void subscriberFunction() {
            ros::NodeHandle n;
            std::vector<ros::Subscriber*> activeSubscribers;

            /// add all of the subscribers
            for (std::map<std::string, callbackFunctionPtr_t>::iterator it = subscribers.begin(); it != subscribers.end(); ++it) {
                ros::TransportHints rosTH;
                activeSubscribers.push_back(new ros::Subscriber(n.subscribe(it->first, 1, &(*it->second), rosTH)));
            }

            ros::Rate loop_rate(subscriberFrequency);

            while (ros::ok() && runSubscriberThread) {
                /// Lock control data transfer mutex
                control_command_mutex.lock();
                /// Transfer all of the data defined
                for (std::map<std::string, double>::iterator it = controlDoubles.begin(); it != controlDoubles.end(); ++it) {
                    it->second = controlDoublesNoRT.find(it->first)->second;
                }
                for (std::map<std::string, bool>::iterator it = controlBools.begin(); it != controlBools.end(); ++it) {
                    it->second = controlBoolsNoRT.find(it->first)->second;
                }
                /// Unlock control data transfer mutex
                control_command_mutex.unlock();

                /// Sleep thread until next call
                loop_rate.sleep();
            }

            /// shutdown all of the subscribers
            for (std::vector<ros::Subscriber*>::iterator it = activeSubscribers.begin(); it != activeSubscribers.end(); ++it) {
                (*it)->shutdown();
                free(*it);
            }
        }

        /// Function for transferring in commands for RT use
        void populateControlCommands()
        {
            control_command_mutex.lock();
            for (std::map<std::string, double>::iterator it = controlDoubles.begin(); it != controlDoubles.end(); ++it) {
                controlDoublesRT.find(it->first)->second = it->second;
            }
            for (std::map<std::string, bool>::iterator it = controlBools.begin(); it != controlBools.end(); ++it) {
                controlBoolsRT.find(it->first)->second = it->second;
            }
            control_command_mutex.unlock();
        }

        void populateControlCommandNames()
        {
            for (std::vector<std::string>::iterator it = controlBoolNames.begin(); it != controlBoolNames.end(); ++it){
                controlBools[*it] = false;
                controlBoolsRT[*it] = false;
                controlBoolsNoRT[*it] = false;
            }
            for (std::vector<std::string>::iterator it = controlDoubleNames.begin(); it != controlDoubleNames.end(); ++it){
                controlDoubles[*it] = 0.0;
                controlDoublesRT[*it] = 0.0;
                controlDoublesNoRT[*it] = 0.0;
            }
        }

        /// Output limiting function
        double limit(double cmd, double max) {
            if (cmd < -max) {
                return -max;
            } else if (cmd > max) {
                return max;
            }
            return cmd;
        }

        /// Functions for unpacking flags and ros params
        void unpackFlag(std::string parameterName, bool &referenceToFlag, bool defaultValue)
        {
            if (!node_.getParam(parameterName, referenceToFlag)) {
                referenceToFlag = defaultValue;
                ROS_ERROR("%s not set for (namespace: %s). Setting to false.",
                          parameterName.c_str(),
                          node_.getNamespace().c_str());
            }
        }
        template <class refType> void unpackParameter(std::string parameterName, refType &referenceToParameter, refType defaultValue)
        {
            if (!node_.getParam(parameterName, referenceToParameter)) {
                referenceToParameter = defaultValue;
                ROS_ERROR("%s not set for (namespace: %s) using %f.",
                          parameterName.c_str(),
                          node_.getNamespace().c_str(),
                          defaultValue);
            }
        }
    };

}

#endif //SRC_BOBBLECONTROLLERBASE_H