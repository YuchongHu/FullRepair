#include "task/algorithm/eva_pipe.hh"

#include <algorithm>
#include <cstring>

namespace exr {

//Constructor and destructor
EvaPipe::EvaPipe(const Count &k, const Count &num,
                 const Count &rid, const Count &task_num,
                 const Path &bw_path)
    : RouteCalculator(rid, bw_path), k_(k), num_(num),
      bandwidths_(new Bandwidth[num]), raw_bandwidths_(new Bandwidth[num]),
      task_num_(task_num), max_bw_(0), base_(0),
      nodes_(nullptr), tasks_(nullptr) {}

EvaPipe::~EvaPipe() = default;

//Fill the information
Count EvaPipe::GetTaskNumber(const Count &gid) {
  return gid == 0 ? max_bw_ : 0;
}

void EvaPipe::FillTask(const Count &gid, const Count &tid,
                       const Count &node_id,
                             RepairTask &rt, Count *src_ids) {
  if (node_id > num_ || gid > 0) {
    rt.size = 0;
    return;
  }
  //Check if is chosen
  Count nid = node_id - 1;
  if (!(nodes_[nid * max_bw_ + tid])) {
    rt.size = 0;
    return;
  }

  //Fill the target and bandwidth
  auto cur_task = tasks_.get() + tid * num_;
  rt.tar_id = (cur_task[nid] % num_) + 1;
  rt.bandwidth = base_;

  //Find out the sources
  rt.src_num = 0;
  for (Count i = 0; i < num_; ++i)
    if (cur_task[i] == nid && i != nid)
      src_ids[(rt.src_num)++] = i + 1;

  //Allocate the offset and size
  auto ss = rt.offset + rt.size;
  rt.size /= max_bw_;
  rt.offset += rt.size * tid;
  if (tid == max_bw_ - 1)
    rt.size = ss - rt.offset;
}

BwType EvaPipe::get_capacity() { return max_bw_ * base_; }

//Calculate and get the repair route
Count EvaPipe::CalculateRoute(const Bandwidth *bws, const Count &rid) {
  //Copy all the bandwidth and calculate the limit
  std::memcpy(bandwidths_.get(), bws, sizeof(Bandwidth) * num_);
  AnalyzeBandwidth_(rid - 1);
  if (max_bw_ == 0) return 0;

  //Quantify the bandwidths and re-calculate the limit
  std::memcpy(raw_bandwidths_.get(),
              bandwidths_.get(), sizeof(Bandwidth) * num_);
  base_ = max_bw_ / task_num_ + 1;

  do {
    --base_;
    for (Count i = 0; i < num_; ++i) {
      bandwidths_[i].upload = raw_bandwidths_[i].upload / base_;
      bandwidths_[i].download = raw_bandwidths_[i].download / base_;
    }
    AnalyzeBandwidth_(rid - 1);
    if (max_bw_ == 0) return 0;
  } while (max_bw_ < task_num_);

  //Dis
  CalculatePath_(rid - 1);
  return max_bw_ > 0 ? 1 : 0;
}

void EvaPipe::AnalyzeBandwidth_(const Count &rid) {
  //Get limit by uploads
  auto is_biggest = std::make_unique<bool[]>(num_);
  for (Count i = 0; i < num_; ++i) {
    is_biggest[i] = false;
    if (i != rid && bandwidths_[i].upload == 0)
      bandwidths_[i].download = 0;
  }
  Count divider = k_;
  while (true) {
    BwType max_upload = 0, sum_upload = 0;
    for (Count i = 0; i < num_; ++i) {
      if (!is_biggest[i]) {
        auto &new_bw = bandwidths_[i].upload;
        sum_upload += new_bw;
        max_upload = new_bw > max_upload ? new_bw : max_upload;
      }
    }
    max_bw_ = sum_upload / divider;

    if (max_upload <= max_bw_) break;

    for (Count i = 0; i < num_; ++i) {
      if (bandwidths_[i].upload == max_upload) {
        is_biggest[i] = true;
        --divider;
      }
    }
  }

  //Adjust the limit by downloads
  BwType sum_download = 0;
  for (Count i = 0; i < num_; ++i) sum_download += bandwidths_[i].download;
  if (max_bw_ * k_ > sum_download) max_bw_ = sum_download / k_;
  if (bandwidths_[rid].download < max_bw_)
    max_bw_ = bandwidths_[rid].download;

  for (Count i = 0; i < num_; ++i)
    if (bandwidths_[i].upload > max_bw_) bandwidths_[i].upload = max_bw_;
}

void EvaPipe::CalculatePath_(const Count &rid) {
  //Inits
  std::vector<Count> unassigned; // unassigned nodes
  // all the nodes up and down tasks
  auto up_tasks = std::make_unique<std::unique_ptr<bool[]>[]>(num_);
  auto down_tasks = std::make_unique<std::unique_ptr<Count[]>[]>(num_);
  auto u_remains = std::make_unique<Count[]>(num_);
  auto d_remains = std::make_unique<Count[]>(num_);
  for (Count i = 0; i < num_; ++i) {
    unassigned.push_back(i);
    up_tasks[i] = std::make_unique<bool[]>(max_bw_);
    down_tasks[i] = std::make_unique<Count[]>(max_bw_);
    for (Count j = 0; j < max_bw_; ++j) {
      up_tasks[i][j] = i == rid ? true : false;
      down_tasks[i][j] = 0;
    }
    u_remains[i] = bandwidths_[i].upload;
    d_remains[i] = bandwidths_[i].download;
  }
  auto task_ids = std::make_unique<Count[]>(max_bw_); // all the tasks, used for sorting
  // the remain number of up and down
  Count up_remain = max_bw_ * k_, down_remain = max_bw_ * k_;
  // the used number for tasks
  auto task_ups = std::make_unique<Count[]>(max_bw_);
  auto task_downs = std::make_unique<Count[]>(max_bw_);
  for (Count i = 0; i < max_bw_; ++i) {
    task_ids[i] = i;
    task_ups[i] = 0;
    task_downs[i] = 0;
  }

  //Assign all the uploads
  std::sort(unassigned.begin(), unassigned.end(),
            [&](const Count &i, const Count &j) {
    auto mi = bandwidths_[i].download + bandwidths_[j].upload;
    auto mj = bandwidths_[j].download + bandwidths_[i].upload;
    if (mi == mj) {
      if (bandwidths_[i].download == bandwidths_[j].download)
        return i > j;
      return bandwidths_[i].download < bandwidths_[j].download;
    }
    return mi < mj;
  }); // sorting by (download - upload, download)
  while (unassigned.size() > 0) {
    if (up_remain == 0) break;
    Count nid = unassigned.back();
    unassigned.pop_back();
    std::sort(task_ids.get(), task_ids.get() + max_bw_,
              [&](const Count &i, const Count &j) {
      if (task_ups[i] == task_ups[j]) return i < j;
      return task_ups[i] < task_ups[j];
    }); // get least assigned up tasks
    for (Count i = 0; i < bandwidths_[nid].upload; ++i) {
      if (task_ups[task_ids[i]] >= k_) break;
      up_tasks[nid][task_ids[i]] = true;
      task_ups[task_ids[i]] += 1;
      u_remains[nid] -= 1;
      --up_remain;
    } // assign uploads
    for (Count i = 0; i < bandwidths_[nid].download; ++i) {
      std::sort(task_ids.get(), task_ids.get() + max_bw_,
                [&](const Count &x, const Count &y) {
        if (task_downs[x] == task_downs[y]) return x < y;
        return task_downs[x] < task_downs[y];
      }); // get the least assigned down task
      bool flag = true;
      for (Count j = 0; j < max_bw_; ++j) {
        if (task_downs[task_ids[j]] < k_ && up_tasks[nid][task_ids[j]]) {
          down_tasks[nid][task_ids[j]] += 1;
          task_downs[task_ids[j]] += 1;
          d_remains[nid] -= 1;
          --down_remain;
          flag = false;
          break;
        }
      } // assign downloads
      if (flag) break;
    }
    if (d_remains[nid] > 0)
      unassigned.insert(unassigned.begin(), nid);
  }

  //Fill the remained downloads
  std::sort(unassigned.begin(), unassigned.end(),
            [&](const Count &i, const Count &j) {
    if (d_remains[i] - u_remains[i] == d_remains[j] - u_remains[j])
      return i < j;
    return d_remains[i] - u_remains[i] == d_remains[j] - u_remains[j];
  });
  while (down_remain > 0) {
    if (unassigned.size() == 0) break;
    Count nid = unassigned.back();
    unassigned.pop_back();
    while (d_remains[nid] > 0) {
      std::sort(task_ids.get(), task_ids.get() + max_bw_,
                [&](const Count &x, const Count &y) {
        if (task_downs[x] == task_downs[y]) return x < y;
        return task_downs[x] < task_downs[y];
      }); // get the least assigned down task
      if (task_downs[task_ids[0]] == k_) break;
      bool changed = false;
      for (Count i = 0; i < num_; ++i) {
        if (i == rid) continue;
        if (up_tasks[i][task_ids[0]] && down_tasks[i][task_ids[0]] == 0) {
          if (u_remains[nid] <= 0) {
            bool flag = true;
            for (Count j = 0; j < max_bw_; ++j) {
              if (!up_tasks[i][j] && up_tasks[nid][j]) {
                up_tasks[i][j] = true;
                up_tasks[nid][j] = false;
                u_remains[nid] = 1;
                flag = false;
                break;
              }
            }
            if (flag) continue;
          }
          up_tasks[i][task_ids[0]] = false;
          up_tasks[nid][task_ids[0]] = true;
          u_remains[nid] -= 1;
          BwType added = k_ - task_downs[task_ids[0]];
          if (d_remains[nid] < added) added = d_remains[nid];
          down_tasks[nid][task_ids[0]] += added;
          task_downs[task_ids[0]] += added;
          d_remains[nid] -= added;
          down_remain -= added;
          changed = true;
          break;
        }
      }
      if (!changed) break;
    }
    if (d_remains[nid] > 0) break;
  }
  if (down_remain > 0) {
    max_bw_ = 0;
    return;
  }

  //Fill the result
  nodes_ = std::make_unique<bool[]>(num_ * max_bw_);
  tasks_ = std::make_unique<Count[]>(max_bw_ * num_);
  for (Count i = 0; i < num_ * max_bw_; ++i) {
    nodes_[i] = false;
    tasks_[i] = num_;
  }
  for (Count i = 0; i < max_bw_; ++i) {
    std::vector<Count> nids;
    for (Count j = 0; j < num_; ++j) {
      if (up_tasks[j][i]) nids.push_back(j);
    }
    std::sort(nids.begin(), nids.end(), [&](const Count &x, const Count &y){
      if (x == rid) return true;
      if (y == rid) return false;
      if (down_tasks[x][i] == down_tasks[y][i]) return x < y;
      return down_tasks[x][i] > down_tasks[y][i];
    });
    tasks_[i * num_ + rid] = rid;
    Count poped_id = 0;
    for (Count j = 0; j < nids.size(); ++j) {
      nodes_[nids[j] * max_bw_ + i] = true;
      for (Count m = 0; m < down_tasks[nids[j]][i]; ++m) {
        tasks_[i * num_ + nids[++poped_id]] = nids[j];
      }
    }
  }
}

} // namespace exr
