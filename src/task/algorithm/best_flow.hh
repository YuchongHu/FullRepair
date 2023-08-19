#ifndef EXR_TASK_ALGORITHM_BESTFLOW_HH_
#define EXR_TASK_ALGORITHM_BESTFLOW_HH_

#include <memory>
#include <vector>

#include "task/algorithm/route_calculator.hh"
#include "util/typedef.hh"
#include "util/types.hh"

namespace exr {

struct Task {
  Count id;
  BwType start;
  BwType end;
  Task(const Count &_id, const BwType &_start, const BwType &_end)
    : id(_id), start(_start), end(_end) {}
  Task(const Count &_id, const BwType &size) : Task(_id, 0, size) {}
  Task(const Task &task) : Task(task.id, task.start, task.end) {}
  Task() : Task(0, 0, 0) {}
  void AddOffset(const BwType &offset) {
    start += offset;
    end += offset;
  }
  BwType size() const {
    return end - start;
  }
  Task Sub(std::vector<Task> &tasks, const BwType &size) {
    for (auto &x: tasks) {
      if (x.id != id) continue;
      if (x.start == start) return Task();
      BwType ss = x.start - start;
      if (ss > size) ss = size;
      start += ss;
      return Task(id, start - ss, start);
    }
    Task tt(id, start, start + size);
    if (tt.end > end) tt.end = end;
    start = tt.end;
    return tt;
  }
  Task GetFirstCross(std::vector<Task> &tasks) {
    Task tt(id, end, end);
    for (auto &x: tasks) {
      if (x.id != id || x.end <= start || x.start >= end) continue;
      BwType s = x.start > start ? x.start : start;
      BwType e = x.end < end ? x.end : end;
      if (s < tt.start) {
        tt.start = s;
        tt.end = e;
      }
    }
    return tt;
  }
};

struct Node {
  Count id;
  BwType upload;
  BwType download;
  Task task;
  std::vector<Task> up_tasks;
  void Erase(const Task &task) {
    std::vector<Count> need_delete;
    std::vector<Task> need_add;
    for (Count i = 0; i < up_tasks.size(); ++i) {
      Task &uptask = up_tasks[i];
      if (uptask.id != task.id ||
          uptask.end <= task.start || uptask.start >= task.end)
        continue;
      need_delete.push_back(i);
      BwType temp = task.start > uptask.end ? uptask.end : task.start;
      if (temp > uptask.start) {
        Task tt(uptask);
        tt.end = temp;
        need_add.push_back(tt);
      }
      temp = task.end > uptask.start ? task.end : uptask.start;
      if (temp < uptask.end) {
        Task tt(uptask);
        tt.start = temp;
        need_add.push_back(tt);
      }
    }
    while (need_delete.size() > 0) {
      auto i = need_delete.back();
      up_tasks.erase(up_tasks.begin() + i);
      need_delete.pop_back();
    }
    for (auto &x: need_add) {
      Task tt(x);
      up_tasks.push_back(tt);
    }
  }
};

struct RemainTask {
  Task task;
  Count remain;
};

/* Using exploit repair's algorithm to arrange the repair routes */
class BestFlow : public RouteCalculator
{
 public:
  BestFlow(const Count &k, const Count &n, const Count &rid,
           const bool &if_even, const BwType &min_bw, const Path &bw_path);
  ~BestFlow();

  Count GetTaskNumber(const Count &gid) override;
  void FillTask(const Count &gid, const Count &tid, const Count &node_id,
                RepairTask &rt, Count *src_ids) override;
  BwType get_capacity() override;

  //BestFlow is neither copyable nor movable
  BestFlow(const BestFlow&) = delete;
  BestFlow& operator=(const BestFlow&) = delete;

 protected:
  Count CalculateRoute(const Bandwidth *bws, const Count &rid) override;

 private:
  Count k_;
  Count n_;
  Count rid_;
  bool if_even_;
  BwType min_bw_;

  BwType capacity_;
  std::unique_ptr<Node[]> nodes_;
  std::vector<Task> tasks_;
  std::unique_ptr<RemainTask[]> remains_;
  std::vector<Count> task_nids_;

  std::vector<std::unique_ptr<bool[]>> is_chosen_;
  std::vector<Task> final_tasks_;

  void AnalyzeBandwidth_();
  void DistributeTasks_();
  void AssignUploads_();
  void AssignNode_(Node &node, const Count &cur_id, const Count &nid,
                   const bool &is_requester);
  void CombineAndTranslate_();
};

} // namespace exr

#endif // EXR_TASK_ALGORITHM_BESTFLOW_HH_
