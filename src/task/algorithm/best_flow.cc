#include "task/algorithm/best_flow.hh"

#include <algorithm>

namespace exr {

//Constructor and destructor
BestFlow::BestFlow(const Count &k, const Count &n, const Count &rid,
                   const bool &if_even, const BwType &min_bw,
                   const Path &bw_path)
    : RouteCalculator(rid, bw_path), k_(k), n_(n), rid_(rid - 1),
      if_even_(if_even), min_bw_(min_bw), capacity_(0),
      nodes_(new Node[n]) {}

BestFlow::~BestFlow() = default;

//Fill the information
Count BestFlow::GetTaskNumber(const Count &gid) {
  return gid == 0 ? final_tasks_.size() : 0;
}

void BestFlow::FillTask(const Count &gid, const Count &tid,
                        const Count &node_id,
                        RepairTask &rt, Count *src_ids) {
  if (node_id > n_ || gid > 0) {
    rt.size = 0;
    return;
  }
  //Check if is chosen
  Count nid = node_id - 1;
  auto &ic = is_chosen_[tid];
  if (!(ic[nid])) {
    rt.size = 0;
    return;
  }
  auto &ft = final_tasks_[tid];

  //Fill the target and sources
  rt.bandwidth = ft.size();
  rt.src_num = 0;
  if (nid == ft.id) { //is the nid's task
    rt.tar_id = rid_ + 1;
    for (Count i = 0; i < n_; ++i)
      if (ic[i] && i != rid_ && i != nid) src_ids[(rt.src_num)++] = i + 1;
  } else if (nid == rid_) {
    rt.tar_id = rid_ + 1;
    src_ids[(rt.src_num)++] = ft.id + 1;
  } else {
    rt.tar_id = ft.id + 1;
  }

  //Allocate the offset and size
  DataSize start = (ft.start * rt.size) / capacity_;
  DataSize end = (ft.end * rt.size) / capacity_;
  rt.offset += start;
  rt.size = end - start;
}

BwType BestFlow::get_capacity() { return capacity_; }

//Calculate and get the repair route
Count BestFlow::CalculateRoute(const Bandwidth *bws, const Count &rid) {
  //Clear previous results
  tasks_.clear();
  task_nids_.clear();
  is_chosen_.clear();
  final_tasks_.clear();
  //Copy all the bandwidth and calculate the limit
  for (Count i = 0; i < n_; ++i) {
    nodes_[i].id = i;
    nodes_[i].upload = bws[i].upload;
    nodes_[i].download = bws[i].download;
    nodes_[i].task = Task();
    nodes_[i].up_tasks.clear();
    if (i != rid_ && (nodes_[i].upload < min_bw_ ||
                      nodes_[i].download < min_bw_)) {
      nodes_[i].upload = 0;
      nodes_[i].download = 0;
    }
  }
  AnalyzeBandwidth_();
  if (capacity_ < min_bw_) return 0;
  DistributeTasks_();
  AssignUploads_();
  CombineAndTranslate_();
  return final_tasks_.size() > 0 ? 1 : 0;
}

void BestFlow::AnalyzeBandwidth_() {
  //Get limit by uploads
  auto is_biggest = std::make_unique<bool[]>(n_);
  for (Count i = 0; i < n_; ++i) is_biggest[i] = false;
  Count divider = k_;
  BwType max_upload = 0, sum = 0;
  while (true) {
    //Check the limit
    max_upload = 0, sum = 0;
    for (Count i = 0; i < n_; ++i) {
      if (!is_biggest[i]) {
        auto &new_bw = nodes_[i].upload;
        sum += new_bw;
        max_upload = new_bw > max_upload ? new_bw : max_upload;
      }
    }
    if (max_upload * divider <= sum) {
      capacity_ = sum / divider;
      if (capacity_ > nodes_[rid_].download)
        capacity_ = nodes_[rid_].download;
      break;
    }
    //Pick up biggers
    for (Count i = 0; i < n_; ++i) {
      if (!is_biggest[i] && nodes_[i].upload * divider > sum) {
        is_biggest[i] = true;
        --divider;
      }
    }
  }
  //Adjust the limit by downloads
  do {
    max_upload = 0, sum = 0;
    for (Count i = 0; i < n_; ++i) {
      auto &new_bw = nodes_[i];
      if (i != rid_) {
        if (capacity_ < new_bw.upload)
          new_bw.upload = capacity_;
        if (new_bw.upload * (k_ - 1) < new_bw.download)
          new_bw.download = new_bw.upload / (k_ - 1);
      }
      sum += new_bw.download;
      max_upload = new_bw.upload > max_upload ? new_bw.upload : max_upload;
    }
    if (capacity_ * k_ > sum) capacity_ = sum / k_;
  } while (max_upload > capacity_);
}

void BestFlow::DistributeTasks_() {
  //Sort by descent of download
  std::sort(nodes_.get(), nodes_.get() + n_,
            [&](const Node &i, const Node &j) {
    if (i.id == rid_) return true;
    if (j.id == rid_) return false;
    if (i.download == j.download) {
      if (i.upload == j.upload)
        return i.id < j.id;
      return i.upload > j.upload;
    }
    return i.download > j.download;
  });
  //Distribute tasks among helpers
  BwType remain = capacity_, sum_download = 0;
  for (Count i = 1; i < n_; ++i) sum_download += nodes_[i].download;
  for (Count i = 1; i < n_; ++i) {
    uint64_t cap = capacity_;
    BwType size;
    if (if_even_) {
      size = (nodes_[i].download * cap) / sum_download + 1;
      if (size * (k_ - 1) > nodes_[i].download)
        size = nodes_[i].download / (k_ - 1) + 1;
    } else {
      size = nodes_[i].download / (k_ - 1) + 1;
    }
    if (size > remain ) size = remain;
    if (nodes_[i].download == 0) size = 0;
    nodes_[i].task = Task(0, size);
    remain -= size;
    if (remain == 0) break;
  }
  //The requester get the rest part
  if (remain > 0) nodes_[0].task = Task(0, remain);
  //Prepare for upload task assigning
  std::sort(nodes_.get(), nodes_.get() + n_,
            [&](const Node &i, const Node &j) {
    if (i.id == rid_) return true;
    if (j.id == rid_) return false;
    if (i.upload - i.task.size() == j.upload - j.task.size()) {
      if (i.upload == j.upload)
        return i.id < j.id;
      return i.upload > j.upload;
    }
    return i.upload - i.task.size() > j.upload - j.task.size();
  });
  remain = 0;
  for (Count i = 0, id = 0; i < n_; ++i) {
    if (nodes_[i].task.size() > 0) {
      nodes_[i].task.id = id;
      nodes_[i].task.AddOffset(remain);
      remain = nodes_[i].task.end;
      task_nids_.push_back(nodes_[i].id);
      tasks_.push_back({id++, nodes_[i].task.start, nodes_[i].task.end});
    }
  }
}

void BestFlow::AssignUploads_() {
  //Init the remain of uploads
  remains_ = std::make_unique<RemainTask[]>(tasks_.size());
  for (Count i = 0; i < tasks_.size(); ++i) {
    remains_[i].task = Task(tasks_[i]);
    remains_[i].remain = k_;
  }
  //Assigning
  Count cur_id = 0;
  for (Count i = 0; i < n_; ++i) {
    AssignNode_(nodes_[i], cur_id, i, nodes_[i].id == rid_);
    if (nodes_[i].task.size() > 0) ++cur_id;
  }
}

void BestFlow::AssignNode_(Node &node, const Count &cur_id,
                           const Count &nid, const bool &is_requester) {
  auto upload = node.upload;
  //Add own task
  if (node.task.size() > 0 && !is_requester) {
    for (Count i = 0; i < tasks_.size(); ++i) {
      if (node.task.id == remains_[i].task.id) {
        --(remains_[i].remain);
        node.up_tasks.push_back(node.task);
        upload -= node.task.size();
        break;
      }
    }
  }
  //Add other uploading tasks
  while (upload > 0) {
    std::sort(remains_.get(), remains_.get() + tasks_.size(),
              [&](const RemainTask &i, const RemainTask &j) {
      if (i.remain == j.remain) {
        auto ifinished = i.task.id < cur_id, jfinished = j.task.id < cur_id;
        if (ifinished == jfinished) {
          if (ifinished) return i.task.id < j.task.id;
          else return i.task.id > j.task.id;
        }
        return ifinished;
      }
      return i.remain > j.remain;
    });
    bool flag = true;
    for (Count i = 0; i < tasks_.size(); ++i) {
      //No more tasks
      if (remains_[i].remain == 0) break;
      //Tasks that cannot add
      if ((remains_[i].task.id == cur_id && node.task.size() > 0) ||
          (remains_[i].remain == 1 &&
           ((remains_[i].task.id > cur_id && node.task.size() > 0) ||
            (remains_[i].task.id >= cur_id && node.task.size() == 0))))
        continue;
      //Try to add the task
      auto uptask = remains_[i].task.Sub(node.up_tasks, upload);
      if (uptask.size() > 0) {
        upload -= uptask.size();
        node.up_tasks.push_back(uptask);
        if (remains_[i].task.size() == 0) {
          remains_[i].task.start = tasks_[remains_[i].task.id].start;
          --(remains_[i].remain);
        }
        flag = false;
        break;
      }
    }
    if (flag) break;
  }
  //Exchange
  Count next = cur_id + 1;
  if (upload > 0 && next < tasks_.size() && node.task.size() > 0 &&
      remains_[cur_id].remain == 1 && remains_[next].remain == 1) {
    Task tt(remains_[cur_id].task);
    BwType ss = tt.size() > upload ? upload : tt.size();
    tt.end = tt.start + ss;
    remains_[cur_id].task.start += ss;
    std::vector<Task> kks;
    for (Count i = next; i < tasks_.size(); ++i) {
      Task kk(remains_[i].task);
      BwType kss = kk.size() > ss ? ss : kk.size();
      kk.end = kk.start + kss;
      ss -= kk.size();
      kks.push_back(kk);
      if (ss == 0) break;
    }
    while (tt.size() > 0) {
      bool flag = true;
      for (Count i = 0; i < nid; ++i) {
        Node &nnn = nodes_[i];
        Task tcross = tt.GetFirstCross(nnn.up_tasks);
        Task kcross = kks[0].GetFirstCross(nnn.up_tasks);
        if (tcross.size() < tt.size() and kcross.size() > 0) {
          if (tcross.size() > 0) {
            if (tcross.start == tt.start || kcross.start != kks[0].start)
              continue;
            tcross.end = tt.end;
          }
          BwType chsize = tt.size() - tcross.size();
          if (kcross.size() < chsize) chsize = kcross.size();
          //Adding
          Task tadd(tt);
          tadd.end = tadd.start + chsize;
          tt.start = tadd.end;
          nnn.up_tasks.push_back(tadd);
          //Exchanging
          kcross.end = kcross.start + chsize;
          kks[0].start = kcross.end;
          if (kks[0].size() == 0)
            kks.erase(kks.begin());
          nnn.Erase(kcross);
          flag = false;
          break;
        }
      }
      if (flag) break;
    }
  }
}

void BestFlow::CombineAndTranslate_() {
  /*std::cout << std::endl << capacity_ << std::endl;
  for (Count i = 0; i < n_; ++i) {
    std::cout << std::endl << nodes_[i].id << ": ";
    if (nodes_[i].task.size() > 0)
      std::cout << "(" << nodes_[i].task.id << ") "
                << nodes_[i].task.start << "-"
                << nodes_[i].task.end;
    std::cout << " [" << nodes_[i].upload << ", " << nodes_[i].download <<  "]";
    for (auto &x: nodes_[i].up_tasks) {
      std::cout << std::endl << "\t(" << x.id << ") "
                << x.start << "-" << x.end << " => " << task_nids_[x.id];
    }
  }
  std::cout << std::endl;*/
  //Sort the tasks and combine the related task pieces
  for (Count i = 0; i < n_; ++i) {
    auto &up_tasks = nodes_[i].up_tasks;
    std::sort(up_tasks.begin(), up_tasks.end(),
              [&](const Task &x, const Task &y) {
      if (x.id == y.id) return x.start > y.start;
      return x.id > y.id;
    });
    std::vector<Count> deleted;
    for (Count j = 1; j < up_tasks.size(); ++j) {
      if (up_tasks[j].id == up_tasks[j-1].id &&
          up_tasks[j].end == up_tasks[j-1].start) {
        up_tasks[j].end = up_tasks[j-1].end;
        deleted.push_back(j-1);
      }
    }
    while (deleted.size() > 0) {
      up_tasks.erase(up_tasks.begin() + deleted.back());
      deleted.pop_back();
    }
  }
  /*for (Count i = 0; i < n_; ++i) {
    std::cout << std::endl << nodes_[i].id << ": ";
    if (nodes_[i].task.size() > 0)
      std::cout << "(" << nodes_[i].task.id << ") "
                << nodes_[i].task.start << "-"
                << nodes_[i].task.end;
    std::cout << " [" << nodes_[i].upload << "]";
    for (auto &x: nodes_[i].up_tasks) {
      std::cout << std::endl << "\t(" << x.id << ") "
                << x.start << "-" << x.end << " => " << task_nids_[x.id];
    }
  }
  std::cout << std::endl;*/
  //Poping tasks and spliting
  Count cur_id = 0;
  BwType start = 0, end = 0;
  do {
    start = end;
    end = capacity_;
    for (Count i = 0; i < n_; ++i) {
      auto &up_tasks = nodes_[i].up_tasks;
      // check if the node is finished
      if (up_tasks.size() > 0 && up_tasks.back().end <= start)
        up_tasks.pop_back();
      if (up_tasks.size() == 0) continue;
      // get range
      if (up_tasks.back().end <= end) {
        end = up_tasks.back().end;
        cur_id = up_tasks.back().id;
      }
    }
    final_tasks_.push_back({task_nids_[cur_id], start, end});
    is_chosen_.push_back(std::make_unique<bool[]>(n_));
    for (Count i = 0; i < n_; ++i) {
      auto &up_tasks = nodes_[i].up_tasks;
      if (nodes_[i].id == rid_ || (up_tasks.size() > 0 &&
          up_tasks.back().end >= end && up_tasks.back().start <= start)) {
        is_chosen_.back()[nodes_[i].id] = true;
      } else {
        is_chosen_.back()[nodes_[i].id] = false;
      }
    }
    /*std::cout << final_tasks_.size()-1 << ": " << start << "-" << end
              << "  =>  " << task_nids_[cur_id] << std::endl << "\t";
    for (Count i = 0; i < n_; ++i) {
      if (is_chosen_.back()[i]) {
        std::cout << i << (i > 9 ? " " : "  ");
      } else {
        std::cout << "*  ";
      }
    }
    std::cout << std::endl;
    int a = 0;
    for (Count i = 0; i < n_; ++i)
      if (!is_chosen_.back()[i]) ++a;
    if (a != n_ - k_ - 1 || !is_chosen_.back()[task_nids_[cur_id]])
      std::cout << "asdfasdklf" << std::endl;*/
  } while (end < capacity_);
}

} // namespace exr
