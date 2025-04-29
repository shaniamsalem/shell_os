#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash: got ctrl-Z\n";
  // non-negative curr_fg_pid means a process is currently running in foreground
  if(smash.curr_fg_pid > 0) {
    if(kill(smash.curr_fg_pid, sig_num) < 0){
      perror("smash error: kill failed");
      return;
    }

    smash.jobs.removeFinishedJobs(); // need to remove finished jobs before adding new job
    if(smash.curr_fg_jobid > 0){
      JobsList::JobEntry *job = smash.jobs.getJobById(smash.curr_fg_jobid);
      job->resetTimerAndStop();
    } else {
      smash.jobs.addJob(smash.curr_fg_cmd, smash.curr_fg_pid, true);
    }

    cout << "smash: process " << smash.curr_fg_pid << " was stopped\n";
    smash.curr_fg_cmd = "";
    smash.curr_fg_pid = -1;
    smash.curr_fg_jobid = -1;
  }
}

void ctrlCHandler(int sig_num) {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash: got ctrl-C\n";

  if(smash.curr_fg_pid > 0) {
    if(kill(smash.curr_fg_pid, sig_num) < 0){
      perror("smash error: kill failed");
      return;
    }

    smash.jobs.removeFinishedJobs(); // need to remove finished jobs before adding new job
    if(smash.curr_fg_jobid > 0) {
      smash.jobs.removeJobById(smash.curr_fg_jobid);
    }

    cout << "smash: process " << smash.curr_fg_pid << " was killed\n";
    smash.curr_fg_cmd = "";
    smash.curr_fg_pid = -1;
    smash.curr_fg_jobid = -1;
  }
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

