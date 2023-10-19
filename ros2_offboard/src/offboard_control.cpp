#include <iostream>
#include <chrono>

#include <rclcpp/rclcpp.hpp>
#include <px4_msgs/msg/vehicle_command.hpp>
#include <px4_msgs/msg/vehicle_control_mode.hpp>
#include <px4_msgs/msg/trajectory_setpoint.hpp>
#include <px4_msgs/msg/offboard_control_mode.hpp>

using namespace std::chrono_literals;
using namespace px4_msgs::msg;

class OffboardControl : public rclcpp::Node {
public:
  OffboardControl() : Node("ros2_offboard")
  {
    target_ = TrajectorySetpoint{};
    target_.position = {0.0, 0.0, -5.0};
    target_.yaw = -3.14;
    heartbeat_pub_ = this->create_publisher<OffboardControlMode>("/fmu/in/offboard_control_mode", 10);
    traj_setpoint_pub_ = this->create_publisher<TrajectorySetpoint>("/fmu/in/trajectory_setpoint", 10);
    veh_cmd_pub_ = this->create_publisher<VehicleCommand>("/fmu/in/vehicle_command", 10);

    // The original code for offboard control runs this at least 10 times before arming, this however seems to work? might need to change for the real thing
    pub_heartbeat();

    this->arm();
    this->pub_vehcom(VehicleCommand::VEHICLE_CMD_DO_SET_MODE, 1, 6); // Set to Offboard Mode
    
    auto timer_callback = [this]() -> void {
      pub_heartbeat();
      pub_target();
    };
    timer_ = this->create_wall_timer(100ms, timer_callback);
  }
  void arm();
  void disarm();
private:
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<OffboardControlMode>::SharedPtr heartbeat_pub_;
  rclcpp::Publisher<TrajectorySetpoint>::SharedPtr traj_setpoint_pub_;
  rclcpp::Publisher<VehicleCommand>::SharedPtr veh_cmd_pub_;
  
  TrajectorySetpoint target_;
  
  void pub_heartbeat();
  void pub_target();
  void pub_vehcom(uint16_t command, float param1 = 0.0, float param2 = 0.0);
};

void OffboardControl::arm()
{
  pub_vehcom(VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 1.0);
  RCLCPP_INFO(this->get_logger(), "Arm Command Sent.");
}

void OffboardControl::disarm()
{
  pub_vehcom(VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 0.0);
  RCLCPP_INFO(this->get_logger(), "Disarm Command Sent.");
}

void OffboardControl::pub_heartbeat()
{
  OffboardControlMode heartbeat{};
  heartbeat.position = true;
  heartbeat.velocity = false;
  heartbeat.acceleration = false;
  heartbeat.attitude = false;
  heartbeat.body_rate = false;
  heartbeat.timestamp = this->get_clock()->now().nanoseconds() / 1000;
  heartbeat_pub_->publish(heartbeat);
}

void OffboardControl::pub_vehcom(uint16_t command, float param1, float param2)
{
  VehicleCommand msg{};
  msg.param1 = param1;
  msg.param2 = param2;
  msg.command = command;
  msg.target_system = 1;
  msg.target_component = 1;
  msg.source_system = 1;
  msg.source_component = 1;
  msg.from_external = true;
  msg.timestamp = this->get_clock()->now().nanoseconds() / 1000;
  veh_cmd_pub_->publish(msg);
}

void OffboardControl::pub_target()
{
  target_.timestamp = this->get_clock()->now().nanoseconds() / 1000;
  traj_setpoint_pub_->publish(target_);
}

int main(int argc, char ** argv)
{
  std::cout << "Starting offboard control node." << std::endl;
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<OffboardControl>());
  rclcpp::shutdown();
  return 0;
}
