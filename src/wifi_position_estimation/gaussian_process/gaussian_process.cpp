#include "wifi_position_estimation/gaussian_process/gaussian_process.h"
#include "wifi_position_estimation/gaussian_process/optimizer.h"
#include <iostream>
#include <grid_map_ros/grid_map_ros.hpp>
#include <boost/progress.hpp>

Process::Process(Matrix<double, Dynamic, 2> &training_coords, Matrix<double, Dynamic, 1> &training_observs,
                 double signal_noise, double signal_var, Vector2d lengthscale) : ard_se_kernel_(signal_noise, signal_var, lengthscale)
{
  set_training_values(training_coords, training_observs);
  update_covariance_matrix();
}

void Process::train_params(Matrix<double, Dynamic, 1> starting_point)
{
  Optimizer opt(*this);
  //opt.rprop(starting_point);
  opt.conjugate_gradient(4);
}

void Process::update_covariance_matrix()
{
  for(int i = 0; i < n; i++)
  {
    for(int j = 0; j < n; j++)
    {
      Matrix<double, 2, 1> pos1;
      Matrix<double, 2, 1> pos2;
      pos1 << training_coords_(i,0), training_coords_(i,1);
      pos2 << training_coords_(j,0), training_coords_(j,1);
      K_(i,j) = ard_se_kernel_.covariance(pos1,pos2);
    }
  }

  K_inv_ = K_.fullPivLu().solve(MatrixXd::Identity(n,n));
  //K_inv_ = K_.colPivHouseholderQr().solve(MatrixXd::Identity(n,n));
}

void Process::compute_cov_vector(double x, double y)
{
  Matrix<double, 2 ,1> pos1;
  pos1 << x, y;
  for(int i = 0; i < n; i++)
  {
    Matrix<double, 2, 1> pos2;
    pos2 << training_coords_(i,0), training_coords_(i,1);
    cov_vector_(i,0) = ard_se_kernel_.covariance(pos1, pos2);
  }
}

double Process::probability(double x, double y, double z)
{
  x = (x - x_mean_)/x_std_;
  y = (y- y_mean_)/y_std_;
  z = (z+100.0)/(100.0);
  Matrix<double, 2, 1> pos;
  pos << x, y;

  double mean;
  double variance;

  compute_cov_vector(x, y);
  mean = cov_vector_.transpose() * K_inv_ * training_observs_;
  variance = ard_se_kernel_.covariance(pos, pos) - cov_vector_.transpose() * K_inv_ * cov_vector_;

  return ((1.0 / sqrt(2.0 * M_PI * fabs(variance))) * exp(-(pow(z-mean,2.0)/(2.0*fabs(variance)))));
}

double Process::probability_precomputed(double mean, double variance, double z)
{
  z = (z+100.0)/(100.0);
  return ((1.0 / sqrt(2.0 * M_PI * fabs(variance))) * exp(-(pow(z-mean,2.0)/(2.0*fabs(variance)))));
}

void Process::precompute_data(PrecomputedDataPoint& data, Eigen::Vector2d position)
{
  position(0) = (position(0) - x_mean_)/x_std_;
  position(1) = (position(1) - y_mean_)/y_std_;
  Matrix<double, 2, 1> pos;
  pos << position(0), position(1);
  compute_cov_vector(position(0), position(1));
  data.mean_ = cov_vector_.transpose() * K_inv_ * training_observs_;
  data.variance_ = ard_se_kernel_.covariance(position, position) - cov_vector_.transpose() * K_inv_ * cov_vector_;
}

void Process::set_training_values(Matrix<double, Dynamic, 2> &training_coords, Matrix<double, Dynamic, 1> &training_observs)
{
  n = training_coords.rows();
  training_coords_.resize(n, 2);
  training_observs_.resize(n, 1);

  Eigen::RowVectorXd mean = training_coords.colwise().mean();
  x_mean_ = mean(0);
  y_mean_ = mean(1);
  Eigen::RowVectorXd std = ((training_coords.rowwise() - mean).array().square().colwise().sum() / (training_coords.rows() - 1)).sqrt();
  x_std_ = std(0);
  y_std_ = std(1);
  training_coords_ = (training_coords.rowwise() - mean).array().rowwise() / std.array();

  training_observs_ = (training_observs.array()+100.0)/(100.0);

  K_.resize(n,n);
  cov_vector_.resize(n,1);
}

void Process::set_params(const Matrix<double, Dynamic, 1> &params)
{
  ard_se_kernel_ = ARD_SE_Kernel(params(0,0), params(1,0), params(2,0), params(3,0));
  update_covariance_matrix();
}

void Process::set_params(double signal_noise, double signal_var, double lengthscale1, double lengthscale2)
{
  ard_se_kernel_ = ARD_SE_Kernel(signal_noise, signal_var, lengthscale1, lengthscale2);
  update_covariance_matrix();
}

double Process::log_likelihood()
{
  Vector2d pos1;
  Vector2d pos2;

  MatrixXd L = K_.llt().matrixL();

  Matrix<double, Dynamic, Dynamic> diagL = L.diagonal();
  double log_det_K = 2.0 *(L.diagonal().unaryExpr<double(*)(double)>(&std::log)).sum();

  double ret = (-0.5 * double(training_observs_.transpose() * K_inv_ * training_observs_)) - (0.5 * log_det_K) - ((n/2.0)*log(2.0*M_PI));

  if(std::isnan(ret))
    return std::numeric_limits<double>::infinity();
  return -ret;
}

Matrix<double, Dynamic, 1> Process::log_likelihood_gradient()
{
  Matrix<double, Dynamic, Dynamic> K1(n,n);
  Matrix<double, Dynamic, Dynamic> K2(n,n);
  Matrix<double, Dynamic, Dynamic> K3(n,n);
  Matrix<double, Dynamic, Dynamic> K4(n,n);

  Matrix<double,2,1> pos1;
  Matrix<double,2,1> pos2;
  for(int i = 0; i < n; i++)
  {
    for(int j = 0; j < n; j++)
    {
      pos1 << training_coords_(i,0), training_coords_(i,1);
      pos2 << training_coords_(j,0), training_coords_(j,1);
      Vector4d gradient = ard_se_kernel_.gradient(pos1, pos2);
      K1(i,j) = gradient(0);
      K2(i,j) = gradient(1);
      K3(i,j) = gradient(2);
      K4(i,j) = gradient(3);
    }
  }

  Matrix<double, Dynamic, 1> alpha(n,1);
  Matrix<double, Dynamic, Dynamic> alpha2(n,n);
  alpha = K_inv_ * training_observs_;
  alpha2 = alpha*alpha.transpose() - K_inv_;

  Matrix<double, Dynamic, 1> res(4,1);

  res(0) = 0.5 * ((alpha2*K1).trace());
  res(1) = 0.5 * ((alpha2*K2).trace());
  res(2) = 0.5 * ((alpha2*K3).trace());
  res(3) = 0.5 * ((alpha2*K4).trace());

  return -res;
}

Vector4d Process::get_params()
{
  Vector4d params;
  params = ard_se_kernel_.get_parameters();

  return params;
}

void Process::create_gp_mean_map(grid_map::GridMap &map)
{
  ROS_INFO("Plotting Mean of Gaussian Process");
  unsigned long map_size = map.getSize()[0] * map.getSize()[1];
  boost::progress_display show_progress(map_size);
  for (grid_map::GridMapIterator it(map); !it.isPastEnd(); ++it) {
    grid_map::Position position;
    map.getPosition(*it, position);
    double x = (position.x() - x_mean_)/x_std_;
    double y = (position.y() - y_mean_)/y_std_;
    compute_cov_vector(x,y);

    double mean = cov_vector_.transpose() * K_inv_ * training_observs_;
    //if(!(mean < 0.00001 && mean > -0.00001))
    //{
    //std::cout << mean << std::endl;
    map.at("gp_mean", *it) = mean;
    ++show_progress;
  }

  //ros::Time time = ros::Time::now();

  // Publish grid map.
  //map.setTimestamp(time.toNSec());
  //grid_map_msgs::GridMap message;
  //grid_map::GridMapRosConverter::toMessage(map, message);
  //return message;
}

void Process::create_gp_variance_map(grid_map::GridMap &map)
{
  ROS_INFO("Plotting Variance of Gaussian Process");
  unsigned long map_size = map.getSize()[0] * map.getSize()[1];
  boost::progress_display show_progress(map_size);
  for (grid_map::GridMapIterator it(map); !it.isPastEnd(); ++it) {
    grid_map::Position position;
    map.getPosition(*it, position);
    double x = (position.x() - x_mean_)/x_std_;
    double y = (position.y() - y_mean_)/y_std_;
    compute_cov_vector(x,y);
    Matrix<double, 2, 1> pos;
    pos << x, y;
    double variance = 0.0;
    variance = sqrt(ard_se_kernel_.covariance(pos, pos) - cov_vector_.transpose() * K_inv_ * cov_vector_);

    map.at("gp_variance", *it) = variance;
    ++show_progress;
  }

  //ros::Time time = ros::Time::now();

  // Publish grid map.
  //map.setTimestamp(time.toNSec());
  //grid_map_msgs::GridMap message;
  //grid_map::GridMapRosConverter::toMessage(map, message);
  //return message;
}
