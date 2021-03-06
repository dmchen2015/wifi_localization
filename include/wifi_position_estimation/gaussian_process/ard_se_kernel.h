#ifndef PROJECT_ARD_SE_KERNEL_H
#define PROJECT_ARD_SE_KERNEL_H

#include <Eigen/Dense>

using namespace Eigen;

/**
 * Kernel class
 * This is the radial basis function kernel. It is used in the Gaussian process.
 */
class ARD_SE_Kernel
{
public:
  /**
   * Constructor
   * @param signal_noise
   * @param signal_var
   * @param lengthscale
   */
  ARD_SE_Kernel(double signal_noise, double signal_var, Vector2d lengthscale);

  /**
   * Constructor
   * @param signal_noise
   * @param signal_var
   * @param lengthscale
   * @param lengthscale2
   */
  ARD_SE_Kernel(double signal_noise, double signal_var, double lengthscale, double lengthscale2);


  /**
   * Compute the covariance for two provided positions
   * @param pos1 first position
   * @param pos2 second position
   * @return Covariance of the two positions
   */
  double covariance(Vector2d& pos1, Vector2d& pos2);

  /**
   * Computes the gradient for two given positions.
   * @param pos1 first position
   * @param pos2 second position
   * @return The gradient as Eigen matrix
   */
  Matrix<double, 4, 1> gradient(Vector2d& pos1, Vector2d& pos2);

  /**
   * Set the hyper-parameters of the kernel.
   * @param signal_noise
   * @param signal_var
   * @param lengthscale
   */
  void set_parameters(double signal_noise, double signal_var, Vector2d lengthscale);

  /**
   * Set the hyper-parameters of the kernel
   * @param signal_noise
   * @param signal_var
   * @param lengthscale
   * @param lengthscale2
   */
  void set_parameters(double signal_noise, double signal_var, double lengthscale, double lengthscale2);

  /**
   * Get the hyper-parameters of the kernel.
   * @return
   */
  Vector4d get_parameters();

private:
  double signal_noise_;
  double signal_var_;
  Vector2d lengthscale_;
  double orig_signal_noise_;
  double orig_signal_var_;
  Vector2d orig_lengthscale_;
};

#endif //PROJECT_ARD_SE_KERNEL_H
