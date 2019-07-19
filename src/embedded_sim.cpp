#include <robot_kinematic_simulation/embedded_sim.hpp>

EmbeddedSimulator::EmbeddedSimulator(
    std::shared_ptr<generic_control_toolbox::ControllerBase> &controller)
    : nh_("~"), controller_(controller)
{
  if (!init())
  {
    throw std::logic_error(
        "Failed to initialize the kinematic simulation node");
  }
}

bool EmbeddedSimulator::init()
{
  std::vector<std::string> names;
  if (!sim_.reset())
  {
    return false;
  }

  if (!nh_.getParam("init/joint_names", names))
  {
    ROS_ERROR("Missing init/joint_names parameter");
    return false;
  }

  if (!nh_.getParam("compute_rate", compute_rate_))
  {
    ROS_ERROR("Missing compute_rate");
    return false;
  }

  reset_server_ =
      nh_.advertiseService("/state_reset", &EmbeddedSimulator::resetSrv, this);
  return true;
}

bool EmbeddedSimulator::resetSrv(std_srvs::Empty::Request &req,
                                 std_srvs::Empty::Response &res)
{
  bool ret = sim_.reset();
  ROS_WARN("Resetting kinematic simulation");
  return ret;
}

void EmbeddedSimulator::run()
{
  ros::Time prev_time;
  ros::Rate r(compute_rate_);
  sensor_msgs::JointState command;

  prev_time = ros::Time::now();
  while (ros::ok())
  {
    if (controller_->isActive())
    {
      std::vector<double> joint_velocities(7, 0.0);
      command = controller_->updateControl(sim_.getState(),
                                           ros::Time::now() - prev_time);
      for (unsigned int i = 0; i < command.name.size(); i++)
      {
        ptrdiff_t idx = findInVector(joint_names_, command.name[i]);
        if (idx != -1)
        {
          joint_velocities[idx] = command.velocity[i];
        }
      }

      sim_.update(joint_velocities);
      prev_time = ros::Time::now();
      ros::spinOnce();
      r.sleep();
    }
    else
    {
      std::this_thread::sleep_for(
          std::chrono::milliseconds((int)(1000 / compute_rate_)));
    }
  }
}