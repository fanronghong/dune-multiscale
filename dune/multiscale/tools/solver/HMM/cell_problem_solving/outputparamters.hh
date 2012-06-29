#ifndef DUNEMS_HMM_CELL_OUTPUTPARAMTERS_HH
#define DUNEMS_HMM_CELL_OUTPUTPARAMTERS_HH

namespace Dune {

// define output traits
struct CellProblemDataOutputParameters
  : public DataOutputParameters
{
public:
  std::string my_prefix_;
  std::string my_path_;

  void set_prefix(std::string my_prefix) {
    my_prefix_ = my_prefix;
    // std :: cout << "Set prefix. my_prefix_ = " << my_prefix_ << std :: endl;
  }

  void set_path(std::string my_path) {
    my_path_ = my_path;
  }

  // base of file name for data file
  std::string prefix() const {
    if (my_prefix_ == "")
      return "solutions";
    else
      return my_prefix_;
  }

  // path where the data is stored
  std::string path() const {
    if (my_path_ == "")
      return "data_output_hmm";
    else
      return my_path_;
  }

  // format of output:
  int outputformat() const {
    // return 0; // GRAPE (lossless format)
    return 1; // VTK
    // return 2; // VTK vertex data
    // return 3; // gnuplot
  }
};

} // namespace Dune {

#endif // DUNEMS_HMM_CELL_OUTPUTPARAMTERS_HH
