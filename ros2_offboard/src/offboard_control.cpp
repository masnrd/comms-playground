#include <iostream>
#include <chrono>

#include <rclcpp/rclcpp.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/msg/offboard_control_mode.hpp>
#include <px4_msgs/msg/vehicle_local_position.hpp>
#include <px4_msgs/msg/trajectory_setpoint.hpp>

using namespace std::chrono_literals;

using px4_msgs::msg::VehicleCommand;
using px4_msgs::msg::OffboardControlMode; // Heartbeat

using px4_msgs::msg::VehicleLocalPosition;
using px4_msgs::msg::TrajectorySetpoint;

class Point
{
    public:
    float x, y;
    Point() { x = 0; y = 0; }
    Point(const float initX, const float initY) {
        x = initX;
        y = initY;
    }
};


class OBCNode : public rclcpp::Node 
{
    private:
    uint64_t counter_; // current cycle

    // Position Tracking
    Point pos; // current X/Y
    float alt; // current altitude

    TrajectorySetpoint tgt; // target point
    bool operating; // if the drone is in operation
    
    // Closeness to target (in metres) to which the drone considers itself as having "reached" the target
    const float tolerance = 0.25;

    // Target Altitude, in metres
    const float targetAlt = 5;

    // Velocity Targets (m/s)
    const float fullSpeed = 5.0;
    const float scanSpeed = 2.0;

    // Publisher Interval
    const std::chrono::milliseconds pubIntv = 100ms;

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<VehicleCommand>::SharedPtr vehcom_pub;
    rclcpp::Publisher<OffboardControlMode>::SharedPtr ocm_pub;
    rclcpp::Subscription<VehicleLocalPosition>::SharedPtr localpos_sub;
    rclcpp::Publisher<TrajectorySetpoint>::SharedPtr tsp_pub;

    void set_target(const Point pt);
    void process_pos();

    void pub_heartbeat();
    void pub_target();
    void pub_vehcom(uint16_t cmd, float p1 = 0.0, float p2 = 0.0);

    public:
    OBCNode();
    void arm();
    void disarm();
    void log(const char* msg);
};

OBCNode::OBCNode() : Node("ros2_offboard")
{
    // Initial Setup
    counter_ = 0;
    pos = Point(0, 0);
    alt = 0;
    operating = false;
    tgt = TrajectorySetpoint{}; // Setpoint message to be sent
    tgt.position = {0.0, 0.0, -targetAlt};

    // Setup Subscriber
    // 1. QOS Setup (ROS2-PX4 interfacing issues)
    rmw_qos_profile_t qos_sub_prof = rmw_qos_profile_sensor_data;
    auto qos_sub = rclcpp::QoS(rclcpp::QoSInitialization(qos_sub_prof.history, 5), qos_sub_prof);

    // 2. Create subscription
    localpos_sub = this->create_subscription<VehicleLocalPosition>("/fmu/out/vehicle_local_position", qos_sub, [this](const VehicleLocalPosition::UniquePtr msg) {
        // Update local position every time PX4 publishes to this
        pos.x = msg->x;
        pos.y = msg->y;
        alt = -msg->z;

        std::cout << "Position: " << pos.x << "," << pos.y << std::endl;

        // If we've reached the tgt, then we can report.
        if (operating)
            process_pos();
    });

    // Setup Publishers
    ocm_pub = this->create_publisher<OffboardControlMode>("/fmu/in/offboard_control_mode", 10);
    tsp_pub = this->create_publisher<TrajectorySetpoint>("/fmu/in/trajectory_setpoint", 10);
    vehcom_pub = this->create_publisher<VehicleCommand>("/fmu/in/vehicle_command", 10);

    // Setup timed publishing
    auto timer_callback = [this]() -> void {
        counter_++;
        if (counter_ <= 10) {
            // Continuous sending for the first 1 second
            this->arm();
            this->pub_vehcom(
                VehicleCommand::VEHICLE_CMD_DO_SET_MODE,
                1, 6 // Set to OFFBOARD mode
            );
        }
        pub_heartbeat();

        if (operating)
            pub_target();
    };
    timer_ = this->create_wall_timer(pubIntv, timer_callback);


    // TESTER CODE
    set_target(Point(10.0, 10.0)); log("Going to (10.0, 10.0).");
}

void OBCNode::process_pos() {
    if (abs(pos.x - tgt.position[0]) > tolerance)
        return;
    if (abs(pos.y - tgt.position[1]) > tolerance)
        return;
    
    // If we've reached the target, report in.
    log("Reached target.");

    // TESTER CODE
    set_target(Point(-10.0, -10.0)); log("Going to (-10.0, -10.0).");
}

void OBCNode::set_target(const Point pt) {
    operating = false;
    tgt.position = {pt.x, pt.y, -targetAlt};
    operating = true;
}

void OBCNode::log(const char* msg) {
    RCLCPP_INFO(this->get_logger(), msg);
}

void OBCNode::arm() {
    pub_vehcom(VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 1.0);
    log("Arm command sent.");
}

void OBCNode::disarm() {
    pub_vehcom(VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 0.0);
    log("Disarm command sent.");
}

void OBCNode::pub_heartbeat() {
    OffboardControlMode hb{};
    hb.position = true;
    hb.velocity = false; hb.acceleration = false; hb.attitude = false; hb.body_rate = false;
    hb.timestamp = this->get_clock()->now().nanoseconds() / 1000;
    ocm_pub->publish(hb);
}

void OBCNode::pub_vehcom(uint16_t cmd, float p1, float p2) {
    VehicleCommand msg{};
    msg.param1 = p1; msg.param2 = p2;
    msg.command = cmd;
    msg.target_system = 1;
    msg.target_component = 1;
    msg.source_system = 1;
    msg.source_component = 1;
    msg.from_external = true;
    msg.timestamp = this->get_clock()->now().nanoseconds() / 1000;
    vehcom_pub->publish(msg);
}

void OBCNode::pub_target() {
    // Continuously publish to this to ensure drone knows where to go
    tgt.timestamp = this->get_clock()->now().nanoseconds() / 1000;
    tsp_pub->publish(tgt);
}

int main(int argc, char** argv)
{
    std::cout << "Starting OBCNode." << std::endl;
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<OBCNode>());
    rclcpp::shutdown();
    return 0;
}