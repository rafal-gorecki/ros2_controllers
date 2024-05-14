// Copyright 2021 PAL Robotics SL.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Authors: Subhas Das, Denis Stogl, Victor Lopez
 */

#include "test_imu_sensor_broadcaster.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "hardware_interface/loaned_state_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "imu_sensor_broadcaster/imu_sensor_broadcaster.hpp"
#include "lifecycle_msgs/msg/state.hpp"
#include "rclcpp/utilities.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "sensor_msgs/msg/imu.hpp"

using hardware_interface::LoanedStateInterface;
using testing::IsEmpty;
using testing::SizeIs;

namespace
{
constexpr auto NODE_SUCCESS = controller_interface::CallbackReturn::SUCCESS;
constexpr auto NODE_ERROR = controller_interface::CallbackReturn::ERROR;
}  // namespace

void IMUSensorBroadcasterTest::SetUpTestCase() {}

void IMUSensorBroadcasterTest::TearDownTestCase() {}

void IMUSensorBroadcasterTest::SetUp()
{
  // initialize controller
  imu_broadcaster_ = std::make_unique<FriendIMUSensorBroadcaster>();
}

void IMUSensorBroadcasterTest::TearDown() { imu_broadcaster_.reset(nullptr); }

void IMUSensorBroadcasterTest::SetUpIMUBroadcaster(const std::string & ns)
{
  const auto result = imu_broadcaster_->init(
    "test_imu_sensor_broadcaster", "", 0, ns, imu_broadcaster_->define_custom_node_options());
  ASSERT_EQ(result, controller_interface::return_type::OK);

  std::vector<LoanedStateInterface> state_ifs;
  state_ifs.emplace_back(imu_orientation_x_);
  state_ifs.emplace_back(imu_orientation_y_);
  state_ifs.emplace_back(imu_orientation_z_);
  state_ifs.emplace_back(imu_orientation_w_);
  state_ifs.emplace_back(imu_angular_velocity_x_);
  state_ifs.emplace_back(imu_angular_velocity_y_);
  state_ifs.emplace_back(imu_angular_velocity_z_);
  state_ifs.emplace_back(imu_linear_acceleration_x_);
  state_ifs.emplace_back(imu_linear_acceleration_y_);
  state_ifs.emplace_back(imu_linear_acceleration_z_);

  imu_broadcaster_->assign_interfaces({}, std::move(state_ifs));
}

void IMUSensorBroadcasterTest::subscribe_and_get_message(
  sensor_msgs::msg::Imu & imu_msg, const std::string & ns)
{
  // create a new subscriber
  rclcpp::Node test_subscription_node("test_subscription_node", ns);
  auto subs_callback = [&](const sensor_msgs::msg::Imu::SharedPtr) {};
  auto subscription = test_subscription_node.create_subscription<sensor_msgs::msg::Imu>(
    "test_imu_sensor_broadcaster/imu", 10, subs_callback);

  // call update to publish the test value
  // since update doesn't guarantee a published message, republish until received
  int max_sub_check_loop_count = 5;  // max number of tries for pub/sub loop
  rclcpp::WaitSet wait_set;          // block used to wait on message
  wait_set.add_subscription(subscription);
  while (max_sub_check_loop_count--)
  {
    imu_broadcaster_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01));
    // check if message has been received
    if (wait_set.wait(std::chrono::milliseconds(2)).kind() == rclcpp::WaitResultKind::Ready)
    {
      break;
    }
  }
  ASSERT_GE(max_sub_check_loop_count, 0) << "Test was unable to publish a message through "
                                            "controller/broadcaster update loop";

  // take message from subscription
  rclcpp::MessageInfo msg_info;
  ASSERT_TRUE(subscription->take(imu_msg, msg_info));
}

TEST_F(IMUSensorBroadcasterTest, SensorName_Configure_Success)
{
  SetUpIMUBroadcaster();

  // set the 'sensor_name'
  imu_broadcaster_->get_node()->set_parameter({"sensor_name", sensor_name_});

  // set the 'frame_id'
  imu_broadcaster_->get_node()->set_parameter({"frame_id", frame_id_});

  // configure passed
  ASSERT_EQ(imu_broadcaster_->on_configure(rclcpp_lifecycle::State()), NODE_SUCCESS);

  // check interface configuration
  auto cmd_if_conf = imu_broadcaster_->command_interface_configuration();
  ASSERT_THAT(cmd_if_conf.names, IsEmpty());
  EXPECT_EQ(cmd_if_conf.type, controller_interface::interface_configuration_type::NONE);
  auto state_if_conf = imu_broadcaster_->state_interface_configuration();
  ASSERT_THAT(state_if_conf.names, SizeIs(10lu));
  EXPECT_EQ(state_if_conf.type, controller_interface::interface_configuration_type::INDIVIDUAL);
}

TEST_F(IMUSensorBroadcasterTest, SensorName_Activate_Success)
{
  SetUpIMUBroadcaster();

  // set the params 'sensor_name' and 'frame_id'
  imu_broadcaster_->get_node()->set_parameter({"sensor_name", sensor_name_});
  imu_broadcaster_->get_node()->set_parameter({"frame_id", frame_id_});

  // configure and activate success
  ASSERT_EQ(imu_broadcaster_->on_configure(rclcpp_lifecycle::State()), NODE_SUCCESS);
  ASSERT_EQ(imu_broadcaster_->on_activate(rclcpp_lifecycle::State()), NODE_SUCCESS);

  // check interface configuration
  auto cmd_if_conf = imu_broadcaster_->command_interface_configuration();
  ASSERT_THAT(cmd_if_conf.names, IsEmpty());
  EXPECT_EQ(cmd_if_conf.type, controller_interface::interface_configuration_type::NONE);
  auto state_if_conf = imu_broadcaster_->state_interface_configuration();
  ASSERT_THAT(state_if_conf.names, SizeIs(10lu));
  EXPECT_EQ(state_if_conf.type, controller_interface::interface_configuration_type::INDIVIDUAL);

  // deactivate passed
  ASSERT_EQ(imu_broadcaster_->on_deactivate(rclcpp_lifecycle::State()), NODE_SUCCESS);

  // check interface configuration
  cmd_if_conf = imu_broadcaster_->command_interface_configuration();
  ASSERT_THAT(cmd_if_conf.names, IsEmpty());
  EXPECT_EQ(cmd_if_conf.type, controller_interface::interface_configuration_type::NONE);
  state_if_conf = imu_broadcaster_->state_interface_configuration();
  ASSERT_THAT(state_if_conf.names, SizeIs(10lu));  // did not change
  EXPECT_EQ(state_if_conf.type, controller_interface::interface_configuration_type::INDIVIDUAL);
}

TEST_F(IMUSensorBroadcasterTest, SensorName_Update_Success)
{
  SetUpIMUBroadcaster();

  // set the params 'sensor_name' and 'frame_id'
  imu_broadcaster_->get_node()->set_parameter({"sensor_name", sensor_name_});
  imu_broadcaster_->get_node()->set_parameter({"frame_id", frame_id_});

  ASSERT_EQ(imu_broadcaster_->on_configure(rclcpp_lifecycle::State()), NODE_SUCCESS);
  ASSERT_EQ(imu_broadcaster_->on_activate(rclcpp_lifecycle::State()), NODE_SUCCESS);

  ASSERT_EQ(
    imu_broadcaster_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::OK);
}

TEST_F(IMUSensorBroadcasterTest, SensorName_Publish_Success)
{
  SetUpIMUBroadcaster();

  // set the params 'sensor_name' and 'frame_id'
  imu_broadcaster_->get_node()->set_parameter({"sensor_name", sensor_name_});
  imu_broadcaster_->get_node()->set_parameter({"frame_id", frame_id_});

  ASSERT_EQ(imu_broadcaster_->on_configure(rclcpp_lifecycle::State()), NODE_SUCCESS);
  ASSERT_EQ(imu_broadcaster_->on_activate(rclcpp_lifecycle::State()), NODE_SUCCESS);

  sensor_msgs::msg::Imu imu_msg;
  subscribe_and_get_message(imu_msg);

  EXPECT_EQ(imu_msg.header.frame_id, frame_id_);
  EXPECT_EQ(imu_msg.orientation.x, sensor_values_[0]);
  EXPECT_EQ(imu_msg.orientation.y, sensor_values_[1]);
  EXPECT_EQ(imu_msg.orientation.z, sensor_values_[2]);
  EXPECT_EQ(imu_msg.orientation.w, sensor_values_[3]);
  EXPECT_EQ(imu_msg.angular_velocity.x, sensor_values_[4]);
  EXPECT_EQ(imu_msg.angular_velocity.y, sensor_values_[5]);
  EXPECT_EQ(imu_msg.angular_velocity.z, sensor_values_[6]);
  EXPECT_EQ(imu_msg.linear_acceleration.x, sensor_values_[7]);
  EXPECT_EQ(imu_msg.linear_acceleration.y, sensor_values_[8]);
  EXPECT_EQ(imu_msg.linear_acceleration.z, sensor_values_[9]);

  for (size_t i = 0; i < 9; ++i)
  {
    EXPECT_EQ(imu_msg.orientation_covariance[i], 0.0);
    EXPECT_EQ(imu_msg.angular_velocity_covariance[i], 0.0);
    EXPECT_EQ(imu_msg.linear_acceleration_covariance[i], 0.0);
  }
}

TEST_F(IMUSensorBroadcasterTest, TfPrefixNamespaceParams)
{
  struct TestCase
  {
    std::string tf_prefix;
    std::string ns;
    std::string result_prefix;
  };

  const std::vector<TestCase> test_cases = {
    {"", "", ""},
    {"/", "", ""},
    {"", "/", ""},
    {"test_prefix", "", "test_prefix"},
    {"/test_prefix", "", "test_prefix"},
    {"", "test_namespace", "test_namespace/"},
    {"", "/test_namespace", "test_namespace/"},
    {"test_prefix", "test_namespace", "test_prefix"},
  };

  for (const auto & test_case : test_cases)
  {
    const std::string & test_namespace = test_case.ns;

    SetUpIMUBroadcaster(test_namespace);

    imu_broadcaster_->get_node()->set_parameter({"sensor_name", sensor_name_});
    imu_broadcaster_->get_node()->set_parameter({"frame_id", frame_id_});
    imu_broadcaster_->get_node()->set_parameter({"tf_frame_prefix", test_case.tf_prefix});

    ASSERT_EQ(imu_broadcaster_->on_configure(rclcpp_lifecycle::State()), NODE_SUCCESS);
    ASSERT_EQ(imu_broadcaster_->on_activate(rclcpp_lifecycle::State()), NODE_SUCCESS);

    sensor_msgs::msg::Imu imu_msg;
    subscribe_and_get_message(imu_msg, test_namespace);

    EXPECT_EQ(imu_msg.header.frame_id, test_case.result_prefix + frame_id_);
  }
}

TEST_F(IMUSensorBroadcasterTest, SensorNameNamespaced)
{
  struct TestCase
  {
    bool use_namespace_as_sensor_name_prefix;
    std::string result;
  };

  const std::string & test_namespace = "test_namespace";
  const std::vector<TestCase> test_cases = {
    {false, ""},
    {true, test_namespace + "/"},
  };

  for (const auto & test_case : test_cases)
  {
    SetUpIMUBroadcaster(test_namespace);

    imu_broadcaster_->get_node()->set_parameter({"sensor_name", sensor_name_});
    imu_broadcaster_->get_node()->set_parameter({"frame_id", frame_id_});
    imu_broadcaster_->get_node()->set_parameter({"use_namespace_as_sensor_name_prefix", test_case.use_namespace_as_sensor_name_prefix});

    ASSERT_EQ(imu_broadcaster_->on_configure(rclcpp_lifecycle::State()), NODE_SUCCESS);

    auto interface_names = imu_broadcaster_->imu_sensor_->get_state_interface_names();

    EXPECT_EQ(interface_names[0], test_case.result + imu_orientation_x_.get_name());
    EXPECT_EQ(interface_names[1], test_case.result + imu_orientation_y_.get_name());
    EXPECT_EQ(interface_names[2], test_case.result + imu_orientation_z_.get_name());
    EXPECT_EQ(interface_names[3], test_case.result + imu_orientation_w_.get_name());
    EXPECT_EQ(interface_names[4], test_case.result + imu_angular_velocity_x_.get_name());
    EXPECT_EQ(interface_names[5], test_case.result + imu_angular_velocity_y_.get_name());
    EXPECT_EQ(interface_names[6], test_case.result + imu_angular_velocity_z_.get_name());
    EXPECT_EQ(interface_names[7], test_case.result + imu_linear_acceleration_x_.get_name());
    EXPECT_EQ(interface_names[8], test_case.result + imu_linear_acceleration_y_.get_name());
    EXPECT_EQ(interface_names[9], test_case.result + imu_linear_acceleration_z_.get_name());
  }
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleMock(&argc, argv);
  rclcpp::init(argc, argv);
  int result = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return result;
}
