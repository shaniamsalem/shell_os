#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
 public:
  std::string cmd;
  char* argv[COMMAND_MAX_ARGS];
  int argc;

  Command(const char* cmd_line);
  virtual ~Command();
  virtual void execute() = 0;
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {};
};

class ExternalCommand : public Command {
 public:
  bool is_background;
  
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};

enum pipe_t { SMASH_REG_PIPE, SMASH_ERR_PIPE};

class PipeCommand : public Command {
 public:
  std::string pipe_cmd_1;
  std::string pipe_cmd_2;
  pipe_t type;

  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

enum redirection_t { SMASH_REG_REDIRECT, SMASH_APPEND_REDIRECT};

class RedirectionCommand : public Command {
 public:
  std::string file_path;
  redirection_t type;
  std::string redirect_cmd;

  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class CHPromptCommand : public BuiltInCommand {
public:
  CHPromptCommand(const char* cmd_line);
  virtual ~CHPromptCommand() {};
  void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {

public:
  ChangeDirCommand(const char* cmd_line);
  virtual ~ChangeDirCommand() {};
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {};
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {};
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
// TODO: Add your data members
public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};


class JobsList {
 public:
  class JobEntry {
   public:
    int job_id;
    std::string cmd;
    pid_t process_id;
    time_t entry_time;
    bool is_stopped;

    JobEntry(int job_id, std::string cmd, pid_t process_id, time_t entry_time, bool is_stopped);
    ~JobEntry() = default;

    void printEntry(time_t curr_time) const;
    void resetTimerAndStop();
  };

 std::vector<JobEntry> job_vector;
 int max_job_id;

 public:
  JobsList();
  ~JobsList() = default;
  void addJob(std::string cmd, pid_t pid, bool isStopped = false);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId);
  void removeJobById(int jobId);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
  // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
 public:
  JobsCommand(const char* cmd_line);
  virtual ~JobsCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  ForegroundCommand(const char* cmd_line);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 public:
  BackgroundCommand(const char* cmd_line);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class TimeoutCommand : public BuiltInCommand {
/* Bonus */
// TODO: Add your data members
 public:
  explicit TimeoutCommand(const char* cmd_line);
  virtual ~TimeoutCommand() {}
  void execute() override;
};

class ChmodCommand : public BuiltInCommand {
  // TODO: Add your data members
 public:
  ChmodCommand(const char* cmd_line);
  virtual ~ChmodCommand() {}
  void execute() override;
};

class GetFileTypeCommand : public BuiltInCommand {
  // TODO: Add your data members
 public:
  GetFileTypeCommand(const char* cmd_line);
  virtual ~GetFileTypeCommand() {}
  void execute() override;
};

class SetcoreCommand : public BuiltInCommand {
  // TODO: Add your data members
 public:
  SetcoreCommand(const char* cmd_line);
  virtual ~SetcoreCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  KillCommand(const char* cmd_line);
  virtual ~KillCommand() {}
  void execute() override;
};

class SmallShell {
  SmallShell();

 public:
  std::string last_pwd;
  std::string prompt;
  JobsList jobs;
  pid_t curr_fg_pid; // id of process currently running in foreground
  std::string curr_fg_cmd; // cmd line of process currently running in foreground
  int curr_fg_jobid; // job id of process currently running in foreground (optional)
  bool forked_from_smash; // true if forked as part of redirect/pipe
  pid_t smash_pid;

  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);

  // new added methods
  std::string getPromptMessage() const;
  void setPromptMessage(std::string new_prompt);
};

#endif //SMASH_COMMAND_H_
