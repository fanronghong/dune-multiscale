#ifndef DUNE_MODEL_PROBLEM_ALL_HH
#define DUNE_MODEL_PROBLEM_ALL_HH

#include <dune/stuff/common/parameter/configcontainer.hh>
#include <utility>

namespace Problem {
struct Constants
{
  const double epsilon;
  const double epsilon_est;
  const double delta;
  Constants(double def_epsilon, double def_epsilon_est, double def_delta)
    : epsilon( DSC_CONFIG_GET("problem.epsilon", def_epsilon) )
      , epsilon_est( DSC_CONFIG_GET("problem.epsilon_guess", def_epsilon_est) )
      , delta( DSC_CONFIG_GET("problem.delta", def_delta) )
  {}

  template< typename T, class Validator = Dune::Stuff::Common::ValidateAny< T > >
  T get( const std::string& key, const T& def,
         Dune::Stuff::Common::ValidatorInterface< T, Validator > validator = Validator() ) const {
    return DSC_CONFIG_GETV(std::string("problem.") + key, def, validator);
  }

  /** the following method generates arbitrary numbers, with a log-normal distribution
   * the expected value (the value with the highest probability) is:
   * E = exp( m + (s²/2))
   * \todo There's prolly a stl distribution for this
   ***/
  static double rand_log_normal(float m, float s)  {
    // float m = 0.0;
    // float s = 0.1;

    // m is a real number
    // s is a postiv real number. s is a meassure for the variance. The smaller s, the smaller the variance from the
    // expected value

    // we use the box-muller method to generate the number:

    float x1, x2, w;

    float random_number_1, random_number_2, random_number_3;

    do {
      do {
        random_number_1 = std::rand();
        random_number_2 = std::rand();
      } while ( (random_number_1 == 0.0) || (random_number_2 == 0.0) );

      if (random_number_1 > random_number_2)
      { x1 = ( 2.0 * (random_number_2 / random_number_1) ) - 1.0; } else
      { x1 = ( 2.0 * (random_number_1 / random_number_2) ) - 1.0; }

      random_number_3 = std::rand();

      if (random_number_3 > random_number_2)
      { x2 = ( 2.0 * (random_number_2 / random_number_3) ) - 1.0; } else
      { x2 = ( 2.0 * (random_number_3 / random_number_2) ) - 1.0; }

      w = x1 * x1 + x2 * x2;
    } while (w >= 1.0);

    w = sqrt( ( -2.0 * log(w) ) / w );

    // the log-normal arbitrary number:
    return exp( m + (x1 * w * s) );
  }   // end method

  void perturb(double& coefficient_0, double& coefficient_1) const {
    if ( get("stochastic_pertubation", false) ) {
      const float m = 0.0;
      const float s = get("stochastic_variance", 0.01);
      // the expected value in case of a log-normal distribution:
      const float expected_value = exp( m + (pow(s, 2.0) / 2.0) );
      const double arb_num = rand_log_normal(m, s);
      // std :: cout << "arb_num = " << arb_num << std :: endl;
      const float perturbation = arb_num - expected_value;

      coefficient_0 += perturbation;
      coefficient_1 += perturbation;

      if (coefficient_0 < 0.0001)
      { coefficient_0 = 0.0001; }

      if (coefficient_1 < 0.0001)
      { coefficient_1 = 0.0001; }
    }
  }

  template <class DomainType>
  std::pair<const double, const double> coefficients(const DomainType& x) const {
    double coefficient_0 = 1.01 + cos( 2.0 * M_PI * (x[0] / epsilon) );
    double coefficient_1 = 1.01 + cos( 2.0 * M_PI * (x[0] / epsilon) );
    perturb(coefficient_0, coefficient_1);
    return std::make_pair(coefficient_0, coefficient_1);
  }

  template <class DomainType>
  std::pair<const double, const double> coefficients_variant_A(const DomainType& x) const {
    double coefficient_0 = ( 0.1 + ( 1.0 * pow(cos( 2.0 * M_PI * (x[0] / epsilon) ), 2.0) ) );
    double coefficient_1 = ( 0.1 + 1e-3 + ( 0.1 * sin( 2.0 * M_PI * (x[1] / epsilon) ) ) );
    perturb(coefficient_0, coefficient_1);
    return std::make_pair(coefficient_0, coefficient_1);
  }

};

#define CONSTANTSFUNCTION(d, e, f) \
  static const Constants &constants() \
  { \
    static Constants c(d, e, f); \
    return c; \
  }
} // namespace Problem

#endif // DUNE_MODEL_PROBLEM_ALL_HH
