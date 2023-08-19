#ifndef EXR_TASK_ALGORITHM_EVAPIPE_HH_
#define EXR_TASK_ALGORITHM_EVAPIPE_HH_

#include <memory>
#include <vector>

#include "task/algorithm/route_calculator.hh"
#include "util/typedef.hh"
#include "util/types.hh"

namespace exr {

/* Using exploit repair's algorithm to arrange the repair routes */
class EvaPipe : public RouteCalculator
{
 public:
  EvaPipe(const Count &k, const Count &num, const Count &rid,
          const Count &task_num, const Path &bw_path);
  ~EvaPipe();

  Count GetTaskNumber(const Count &gid) override;
  void FillTask(const Count &gid, const Count &tid, const Count &node_id,
                RepairTask &rt, Count *src_ids) override;
  BwType get_capacity() override;

  //EvaPipe is neither copyable nor movable
  EvaPipe(const EvaPipe&) = delete;
  EvaPipe& operator=(const EvaPipe&) = delete;

 protected:
  Count CalculateRoute(const Bandwidth *bws, const Count &rid) override;

 private:
  Count k_;
  Count num_;
  std::unique_ptr<Bandwidth[]> bandwidths_;
  std::unique_ptr<Bandwidth[]> raw_bandwidths_;

  Count task_num_;
  BwType max_bw_;
  BwType base_;

  std::unique_ptr<bool[]> nodes_;
  std::unique_ptr<Count[]> tasks_;

  void AnalyzeBandwidth_(const Count &rid);
  void CalculatePath_(const Count &rid);
};

struct TaskNum {
  Count id;
  Count num;
  TaskNum() : id(0), num(0) {}
};

} // namespace exr

#endif // EXR_TASK_ALGORITHM_EVAPIPE_HH_
