#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iomanip>
#include <errno.h>
#include <sched.h>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

bool _isComplexExternalCommand(const string cmd_line) {
  const string specials = "?*";
  return cmd_line.find_first_of(specials) != string::npos;
}

size_t _getFirstPipePos(const char* cmd_line) {
  const string str(cmd_line);
  const string reg_pipe = "|";
  const string err_pipe = "|&";

  return std::min(str.find(reg_pipe),str.find(err_pipe));
}


size_t _getFirstRedirectionPos(const char* cmd_line) {
  const string str(cmd_line);
  const string reg_redirect = ">";
  const string append_redirect = ">>";

  return std::min(str.find(reg_redirect),str.find(append_redirect));
}

enum smash_cmd_type { SMASH_REGULAR_CMD, SMASH_PIPE_CMD, SMASH_REDIRECT_CMD };

smash_cmd_type _getCommandType(const char* cmd_line) {
  size_t first_pipe_pos = _getFirstPipePos(cmd_line);
  size_t first_redirect_pos = _getFirstRedirectionPos(cmd_line);

  if(first_pipe_pos != string::npos && first_redirect_pos != string::npos) {
    if(first_pipe_pos < first_redirect_pos) return SMASH_PIPE_CMD;
    return SMASH_REDIRECT_CMD;
  }

  // reaching here means - either pipe or redirect or none
  if(first_pipe_pos != string::npos) {
    return SMASH_PIPE_CMD;
  }

  if(first_redirect_pos != string::npos) {
    return SMASH_REDIRECT_CMD;
  }

  return SMASH_REGULAR_CMD;
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() :
  last_pwd(""),
  prompt("smash"),
  curr_fg_pid(-1),
  curr_fg_cmd(""),
  curr_fg_jobid(-1),
  forked_from_smash(false),
  smash_pid(-1) {
    
    if((this->smash_pid = getpid()) < 0) {
      perror("smash error: getpid failed");
      return;
    }
}

SmallShell::~SmallShell() {}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
  string cmd_s = _trim(string(cmd_line));
  string first_word = cmd_s.substr(0, cmd_s.find_first_of("& \n"));
  
  smash_cmd_type cmd_type = _getCommandType(cmd_line);
  if (cmd_type == SMASH_PIPE_CMD){
    return new PipeCommand(cmd_line);
  }

  if (cmd_type == SMASH_REDIRECT_CMD){
    return new RedirectionCommand(cmd_line);
  }

  // regular command
  if (first_word == "pwd") {
    return new GetCurrDirCommand(cmd_line);
  } else if (first_word == "showpid") {
    return new ShowPidCommand(cmd_line);
  } else if (first_word == "cd") {
    return new ChangeDirCommand(cmd_line);
  } else if(first_word == "quit") {
    return new QuitCommand(cmd_line, nullptr); // TODO: add support for kill flag
  } else if(first_word == "chprompt") {
    return new CHPromptCommand(cmd_line);
  } else if(first_word == "jobs") {
    return new JobsCommand(cmd_line);
  } else if(first_word == "fg") {
    return new ForegroundCommand(cmd_line);
  } else if(first_word == "bg") {
    return new BackgroundCommand(cmd_line);
  } else if(first_word == "kill") {
    return new KillCommand(cmd_line);
  } else if(first_word == "setcore") {
    return new SetcoreCommand(cmd_line);
  } else if(first_word == "getfiletype") {
    return new GetFileTypeCommand(cmd_line);
  } else if(first_word == "chmod") {
    return new ChmodCommand(cmd_line);
  }
  
  return new ExternalCommand(cmd_line);
}

void SmallShell::executeCommand(const char *cmd_line) {
  string str(cmd_line);
  if(_trim(str).empty()) return;
  Command* cmd = CreateCommand(cmd_line);

  jobs.removeFinishedJobs();
  cmd->execute();
  delete cmd;
  if(forked_from_smash) exit(0);
}

string SmallShell::getPromptMessage() const {
  return this->prompt;
}

void SmallShell::setPromptMessage(string new_prompt) {
  this->prompt = new_prompt;
}

// ========================== Commands =========================== //

// ============= Command ================ //
Command::Command(const char* cmd_line) : cmd(cmd_line) {
  this->argc = _parseCommandLine(cmd_line, this->argv);
}

Command::~Command() {
  for(int i = 0 ;i < argc; ++i){
    if(argv[i]) free(argv[i]);
  }
}

// ============================ Redirection Command ============================= //
RedirectionCommand::RedirectionCommand(const char* cmd_line) : Command(cmd_line) {
  string str(cmd_line);
  string delimiter;
  size_t reg_redirect_pos = str.find(">");
  size_t append_redirect_pos = str.find(">>");
  size_t split_pos;

  if(reg_redirect_pos != string::npos && append_redirect_pos != string::npos){
    // both substrings are in str
    if(reg_redirect_pos < append_redirect_pos) {
      // ">" is first
      this->type = SMASH_REG_REDIRECT;
      split_pos = reg_redirect_pos;
      delimiter = ">";
    } else {
      // ">>" is first
      this->type = SMASH_APPEND_REDIRECT;
      split_pos = append_redirect_pos;
      delimiter = ">>";
    }
  } else { // at least one of them is missing
    if(reg_redirect_pos != string::npos) {
      // ">" is in string and ">>" is not
      this->type = SMASH_REG_REDIRECT;
      split_pos = reg_redirect_pos;
      delimiter = ">";

    } else if(append_redirect_pos != string::npos) {
      // ">>" is in string and ">" is not
      this->type = SMASH_APPEND_REDIRECT;
      split_pos = append_redirect_pos;
      delimiter = ">>";
    }
  }

  this->redirect_cmd = str.substr(0, split_pos);
  this->file_path = _trim(str.substr(split_pos + delimiter.length()));
}

void RedirectionCommand::execute() {
  pid_t f_pid = fork();
  if(f_pid < 0) {
    perror("smash error: fork failed");
    return;
  }

  if(f_pid == 0){ // child
    SmallShell& smash = SmallShell::getInstance();
    smash.forked_from_smash = true;
    if(close(1) < 0) {
      perror("smash error: close failed");
      return;
    }

    int r_file_flags;
    mode_t r_file_mode = 0655;

    if(this->type == SMASH_REG_REDIRECT)
      r_file_flags = O_WRONLY | O_CREAT | O_TRUNC;
    else
      r_file_flags = O_WRONLY | O_CREAT | O_APPEND;
    

    if(open(file_path.c_str(), r_file_flags, r_file_mode) < 0) {
      perror("smash error: open failed");
      return;
    }
    smash.executeCommand(this->redirect_cmd.c_str());
  } else {
    if(waitpid(f_pid, nullptr, 0) < 0){
      perror("smash error: waitpid failed");
      return;
    }
  }
}

// ============================ Pipe Command ============================= //
PipeCommand::PipeCommand(const char* cmd_line) : Command(cmd_line) {
  string str(cmd_line);
  string delimiter;
  size_t reg_pipe_pos = str.find("|");
  size_t err_pipe_pos = str.find("|&");
  size_t split_pos;

  if(reg_pipe_pos != string::npos && err_pipe_pos != string::npos){
    // both substrings are in str
    if(reg_pipe_pos < err_pipe_pos) {
      // "|" is first
      this->type = SMASH_REG_PIPE;
      split_pos = reg_pipe_pos;
      delimiter = "|";
    } else {
      // "|&" is first
      this->type = SMASH_ERR_PIPE;
      split_pos = err_pipe_pos;
      delimiter = "|&";
    }
  } else { // at least one of them is missing
    if(reg_pipe_pos != string::npos) {
      // "|" is in string and "|&" is not
      this->type = SMASH_REG_PIPE;
      split_pos = reg_pipe_pos;
      delimiter = "|";

    } else if(err_pipe_pos != string::npos) {
      // "|&" is in string and "|" is not
      this->type = SMASH_ERR_PIPE;
      split_pos = err_pipe_pos;
      delimiter = "|&";
    }
  }

  this->pipe_cmd_1 = str.substr(0, split_pos);
  this->pipe_cmd_2 = str.substr(split_pos + delimiter.length());
}

void PipeCommand::execute() {
  int pipe_fd[2];
  SmallShell& smash = SmallShell::getInstance();
  if(pipe(pipe_fd) < 0) {
    perror("smash error: pipe failed");
    return;
  }
  pid_t f_pid;
  f_pid = fork();

  if(f_pid < 0) {
    perror("smash error: fork failed");
    return;
  }

  if(f_pid == 0){ // child
    smash.forked_from_smash = true;

    int redirect_to = this->type == SMASH_REG_PIPE ? 1 : 2;
    if(dup2(pipe_fd[1], redirect_to) < 0) {
      perror("smash error: dup2 failed");
      
      if(close(pipe_fd[0]) < 0) {
        perror("smash error: close failed");
        delete this;
        exit(0);
      }
      if(close(pipe_fd[1]) < 0) {
        perror("smash error: close failed");
        delete this;
        exit(0);
      }
      
      return;
    }
    if(close(pipe_fd[0]) < 0) {
      perror("smash error: close failed");
      delete this;
      exit(0);
    }
    if(close(pipe_fd[1]) < 0) {
      perror("smash error: close failed");
      delete this;
      exit(0);
    }
    smash.executeCommand(this->pipe_cmd_1.c_str());
  } else { // parent
    if(waitpid(f_pid, nullptr, 0) < 0) {
      perror("smash error: waitpid failed");
      return;
    }
  }

  f_pid = fork();

  if(f_pid < 0) {
    perror("smash error: fork failed");
    return;
  }
  
  if(f_pid == 0){ // child
    smash.forked_from_smash = true;

    if(dup2(pipe_fd[0], 0) < 0) {
      perror("smash error: dup2 failed");
      
      if(close(pipe_fd[0]) < 0) {
        perror("smash error: close failed");
        delete this;
        exit(0);
      }
      if(close(pipe_fd[1]) < 0) {
        perror("smash error: close failed");
        delete this;
        exit(0);
      }
      return;
    }
    if(close(pipe_fd[0]) < 0) {
      perror("smash error: close failed");
      delete this;
      exit(0);
    }
    if(close(pipe_fd[1]) < 0) {
      perror("smash error: close failed");
      delete this;
      exit(0);
    }
    smash.executeCommand(this->pipe_cmd_2.c_str());
  } else { // parent
    if(close(pipe_fd[0]) < 0) {
      perror("smash error: close failed");
      return;
    }
    if(close(pipe_fd[1]) < 0) {
      perror("smash error: close failed");
      return;
    }
    if(waitpid(f_pid, nullptr, 0) < 0) {
      perror("smash error: waitpid failed");
      return;
    }
  }
}


// ============= Built in Command ============ //
BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {
  for(int i = 0 ;i < argc; ++i){
    if(argv[i]) free(argv[i]);
  }
  char no_bg_cmd[COMMAND_ARGS_MAX_LENGTH];
  strcpy(no_bg_cmd, cmd_line);
  _removeBackgroundSign(no_bg_cmd);
  this->argc = _parseCommandLine(no_bg_cmd, this->argv);
}

// ============= GCWD Command ============== //
GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
}

void GetCurrDirCommand::execute() {
  const int MAX_PATH = 200;

  char path[MAX_PATH];

  if(!getcwd(path, MAX_PATH)) {
    perror("smash error: getcwd failed");
    return;
  }

  cout << path << "\n";
}


// ============= chprompt Comamand ============ //
CHPromptCommand::CHPromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
}

void CHPromptCommand::execute() {
  SmallShell& shell = SmallShell::getInstance();
  if(argc == 1) { // no args
    shell.setPromptMessage("smash");
  } else { // 2 or more args
    shell.setPromptMessage(argv[1]);
  }
}

// ============= showPID Command ============== //
ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
}

void ShowPidCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash pid is " << smash.smash_pid << "\n";
}

// ============= CD Command ============== //
ChangeDirCommand::ChangeDirCommand(const char* cmd_line) :
  BuiltInCommand(cmd_line) {}

void ChangeDirCommand::execute() {
  if(argc > 2){
    cerr << "smash error: cd: too many arguments\n";
    return;
  }

  // if no arguments, do nothing
  if(argc == 1) {
    return;
  }

  const int MAX_PATH = 200;
  char curr_path[MAX_PATH];

  if(!getcwd(curr_path, MAX_PATH)){
    perror("smash error: getcwd failed");
    return;
  }
  
  SmallShell& smash = SmallShell::getInstance();
  char* new_path = this->argv[1];

  if(strcmp(new_path, "-") == 0){ // "-" arg
    if(smash.last_pwd == "") { // check if no previous pwd set in shell
      cerr << "smash error: cd: OLDPWD not set\n";
      return;
    }
    if(chdir(smash.last_pwd.c_str()) < 0) { // check chdir fail
      perror("smash error: chdir failed");
      return;
    }
  } else { // regular arg (path)
    if(chdir(new_path) < 0) { // check chdir fail
      perror("smash error: chdir failed");
      return;
    }
  }
  // replace last pwd in shell
  smash.last_pwd = curr_path;
}

// ============= Quit Command ============== //
QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) :
  BuiltInCommand(cmd_line) {}

void QuitCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  if(argc > 1){
    if(strcmp(argv[1], "kill") == 0) {
      cout << "smash: sending SIGKILL signal to " << smash.jobs.job_vector.size() << " jobs:\n";
      for(auto it = smash.jobs.job_vector.begin(); it != smash.jobs.job_vector.end(); ++it) {
        cout << it->process_id << ": " << it->cmd << "\n";
      }
      smash.jobs.killAllJobs();
    }
  }
  delete this;
  exit(0);
}

// ====================== External Command ======================== //

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line), is_background(false) {
  if(_isBackgroundComamnd(cmd_line)){ // handle background command construction
    is_background = true;
    
    // fixing last argument, removing & TODO: (Aviv) find a better way
    char effective_cmd[COMMAND_ARGS_MAX_LENGTH];
    strcpy(effective_cmd, cmd_line);
    _removeBackgroundSign(effective_cmd);
    
    // free old argv
    for(int i = 0 ;i < argc; ++i){
      if(argv[i]) free(argv[i]);
    }

    // make new argv without &
    argc = _parseCommandLine(effective_cmd,argv);
  }
}

void ExternalCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  pid_t pid = fork();

  if(pid < 0){
    perror("smash error: fork failed");
    return;
  }

  if(pid == 0){ // child process
    SmallShell& smash = SmallShell::getInstance();
    smash.forked_from_smash = true;

    if(setpgrp() < 0){
      perror("smash error: setpgrp failed");
      delete this;
      exit(0);
    }
    // check if complex external command or not
    if(_isComplexExternalCommand(cmd)){ // complex command
      char complex_cmd[COMMAND_ARGS_MAX_LENGTH];
      strcpy(complex_cmd, cmd.c_str());
      char bash[] = "/bin/bash";
      char cflag[] = "-c";
      char* complex_args[] = {bash,cflag, complex_cmd, nullptr};
      execvp(bash, complex_args);
    } else { // not complex command
      execvp(argv[0],argv);
    }

    // if reached here, execvp failed, exit.
    perror("smash error: execvp failed");
    delete this;
    exit(0);
  } else { // parent process (shell)
    if(is_background){ // background command
      smash.jobs.removeFinishedJobs(); // need to remove finished jobs before adding new job
      smash.jobs.addJob(cmd, pid);
      if(waitpid(pid, nullptr, WNOHANG) < 0){
        perror("smash error: waitpid failed");
        return;
      }
    }
    else { // foreground command
      smash.curr_fg_pid = pid;
      smash.curr_fg_cmd = cmd;
      if(waitpid(pid, nullptr, WUNTRACED) < 0){
        perror("smash error: waitpid failed");
        // clean shell state
        smash.curr_fg_pid = -1;
        smash.curr_fg_cmd = "";
        return;
      }
      // at this point, child process is finished
      smash.curr_fg_pid = -1;
      smash.curr_fg_cmd = "";
    }
  }
}

// ======================== JobsCommand =========================== //
JobsCommand::JobsCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void JobsCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  smash.jobs.removeFinishedJobs(); // need to remove finished jobs before adding new job
  smash.jobs.printJobsList();
}

// ========================= Foreground Command ==================== //
ForegroundCommand::ForegroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ForegroundCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  JobsList::JobEntry* job_to_fg;
  int job_id_to_fg;
  
  // check input and find job_to_fg
  if(argc > 2){
    cerr << "smash error: fg: invalid arguments\n";
    return;
  }
  if(argc == 1){
    if(smash.jobs.job_vector.empty()){
      cerr << "smash error: fg: jobs list is empty\n";
      return;
    }
    job_to_fg = smash.jobs.getLastJob(&job_id_to_fg);
  } else if(argc == 2){
    try {
      job_id_to_fg = stoi(argv[1]);
    } catch(const exception& e){
      cerr << "smash error: fg: invalid arguments\n";
      return;
    }
    job_to_fg = smash.jobs.getJobById(job_id_to_fg);
    if(job_to_fg == nullptr) {
      cerr << "smash error: fg: job-id " << job_id_to_fg << " does not exist\n";
      return;
    }
  }
  // end of input check, reaching here means job_to_fg has been found
  if(kill(job_to_fg->process_id, SIGCONT) < 0){
    perror("smash error: kill failed");
    return;
  }
  int job_process_id = job_to_fg->process_id;
  string job_cmd = job_to_fg->cmd;

  smash.curr_fg_pid = job_process_id;
  smash.curr_fg_cmd = job_cmd;
  smash.curr_fg_jobid = job_id_to_fg;

  cout << job_cmd << " : " << job_process_id << "\n";

  if(waitpid(job_process_id, nullptr, WUNTRACED) < 0){
    perror("smash error: waitpid failed");
    return;
  }
  // at this point, process is finished
  smash.curr_fg_pid = -1;
  smash.curr_fg_cmd = "";
  smash.curr_fg_jobid = -1;
}

// ========================= Background Command ===================== //
BackgroundCommand::BackgroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void BackgroundCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    JobsList::JobEntry* job_to_bg;
    int job_id_to_bg;
    
    // check input and find job_to_bg
    if(argc > 2){
      cerr << "smash error: bg: invalid arguments\n";
      return;
    }
    if(argc == 1){
      if(smash.jobs.job_vector.empty()){
        cerr << "smash error: bg: there is no stopped jobs to resume\n";
        return;
      }
      job_to_bg = smash.jobs.getLastStoppedJob(&job_id_to_bg);
    } else if(argc == 2){
      try {
        job_id_to_bg = stoi(argv[1]);
      } catch(const exception& e){
        cerr << "smash error: bg: invalid arguments\n";
        return;
      }
      job_to_bg = smash.jobs.getJobById(job_id_to_bg);
      if(job_to_bg == nullptr) {
        cerr << "smash error: bg: job-id " << job_id_to_bg << " does not exist\n";
        return;
      }
    }
    // end of input check, reaching here means job_to_bg has been found
    if(job_to_bg->is_stopped == false) {
      cerr << "smash error: bg: job-id " << job_id_to_bg << " is already running in the background\n";
      return;
    }

    if(kill(job_to_bg->process_id, SIGCONT) < 0){
      perror("smash error: kill failed");
      return;
    }
    cout << job_to_bg->cmd << " : " << job_to_bg->process_id << "\n";

    job_to_bg->is_stopped = false;
}

// ========================= Kill Command =========================== //
KillCommand::KillCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void KillCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();

  if(argc != 3){
    cerr << "smash error: kill: invalid arguments\n";
    return;
  }

  int sig_flag;
  int job_id;

  // validate arguments, convert to integers.
  string sig_flag_str = argv[1];
  string job_id_str = argv[2];

  if(sig_flag_str[0] != '-') {
    cerr << "smash error: kill: invalid arguments\n";
    return;
  }
  sig_flag_str.erase(0,1); // remove "-" from sig flag arg

  try{
    sig_flag = stoi(sig_flag_str);
    job_id = stoi(job_id_str);
  } catch (const exception& e) {
    cerr << "smash error: kill: invalid arguments\n";
    return;
  }

  JobsList::JobEntry* job = smash.jobs.getJobById(job_id);
  if(job == nullptr){
    cerr << "smash error: kill: job-id " << job_id << " does not exist\n";
    return;
  }

  int pid = job->process_id;

  // reaching here means args are valid
  if(kill(pid, sig_flag) < 0){
    perror("smash error: kill failed");
    return;
  }

  // updating joblist
  if(sig_flag == SIGSTOP || sig_flag == SIGTSTP) {
    job->is_stopped = true;
  }

  if(sig_flag == SIGCONT) {
    job->is_stopped = false;
  }

  cout << "signal number " << sig_flag << " was sent to pid " << pid << "\n";
}

// ========================= Setcore Command ======================== //
SetcoreCommand::SetcoreCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void SetcoreCommand::execute() {
  if(argc != 3) {
    cerr << "smash error: setcore: invalid arguments\n";
    return;
  }

  int job_id;
  int core_num;

  try{
    job_id = stoi(argv[1]);
    core_num = stoi(argv[2]);
  } catch (const exception& e) {
    cerr << "smash error: setcore: invalid arguments\n";
    return;  
  }
  // reaching here means args are valid
  SmallShell& smash = SmallShell::getInstance();
  JobsList::JobEntry* job = smash.jobs.getJobById(job_id);
  if(job == nullptr) {
    cerr << "smash error: setcore: job-id " << job_id << " does not exist\n";
    return;
  }

  // try to set core
  pid_t process_id = job->process_id;
  cpu_set_t new_core_set;
  CPU_ZERO(&new_core_set);
  CPU_SET(core_num, &new_core_set);
  if(sched_setaffinity(process_id, sizeof(cpu_set_t), &new_core_set) < 0) {
    if(errno == EINVAL){
      cerr << "smash error: setcore: invalid core number\n";
      return;
    }
    perror("smash error: sched_setafiinity failed");
    return;
  }
}

// ========================= Get File Type Command ================================== //

string _getFileTypeStr(mode_t st_mode) {
  switch(st_mode & S_IFMT) {
    case S_IFREG:
      return "regular file";
    case S_IFDIR:
      return "directory";
    case S_IFCHR:
      return "character device";
    case S_IFBLK:
      return "block device";
    case S_IFIFO:
      return "FIFO";
    case S_IFLNK:
      return "symbolic link";
    case S_IFSOCK:
      return "socket";
    default:
      return "unknown";
  }
}

GetFileTypeCommand::GetFileTypeCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void GetFileTypeCommand::execute() {
  if(argc != 2) {
    cerr << "smash error: getfiletype: invalid arguments\n";
    return;
  }

  int fd = open(argv[1], O_RDONLY);
  if(fd < 0) {
    perror("smash error: open failed");
    return;
  }

  struct stat file_stat;
  if(fstat(fd, &file_stat) < 0){
    perror("smash error: fstat failed");
    if(close(fd) < 0){
      perror("smash error: close failed");
      return;
    }
    return;
  }

  string file_type = _getFileTypeStr(file_stat.st_mode);
  off_t file_size = file_stat.st_size;

  cout << argv[1] << "'s type is \"" << file_type << "\" and takes up " << file_size << " bytes\n";

  if(close(fd) < 0){
    perror("smash error: close failed");
    return;
  }
}

// ========================= Chmod Command ========================== //

bool _isFileModeValid(int file_mode){
  if(file_mode < 0 || file_mode > 9999) {
    return false;
  }
  int usr_mode;
  int grp_mode;
  int oth_mode;
  int id_mode;

  oth_mode = file_mode%10;
  file_mode /= 10;
  grp_mode = file_mode%10;
  file_mode /= 10;;
  usr_mode = file_mode%10;
  file_mode /= 10;
  id_mode = file_mode%10;

  if(oth_mode > 7) return false;
  if(grp_mode > 7) return false;
  if(usr_mode > 7) return false;
  if(id_mode > 7) return false;

  return true;
}

ChmodCommand::ChmodCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ChmodCommand::execute() {
  if(argc != 3) {
    cerr << "smash error: chmod: invalid arguments\n";
    return;
  }

  int file_mode_decimal;
  try {
    file_mode_decimal = stoi(argv[1]);
  } catch (const exception& e) {
    cerr << "smash error: chmod: invalid arguments\n";
    return;  
  }

  if(!_isFileModeValid(file_mode_decimal)) {
    cerr << "smash error: chmod: invalid arguments\n";
    return;  
  }

  // reaching here means arguments are valid
  int file_mode_octal = strtol(argv[1], 0, 8);


  if(chmod(argv[2], file_mode_octal) < 0){
    perror("smash error: chmod failed");
    return;
  }
}

// ========================= JobList and JobEntry =================== //
JobsList::JobEntry::JobEntry(int job_id, std::string cmd, pid_t process_id, time_t entry_time, bool is_stopped) :
  job_id(job_id),
  cmd(cmd),
  process_id(process_id),
  entry_time(entry_time),
  is_stopped(is_stopped) {}

void JobsList::JobEntry::printEntry(time_t curr_time) const {
  time_t seconds_elapsed = difftime(curr_time, entry_time);
  cout << "[" << job_id << "] " << cmd << " : " << process_id << " " <<seconds_elapsed << " secs";
  if(is_stopped) cout << " (stopped)";
  cout << "\n";
}

void JobsList::JobEntry::resetTimerAndStop() {
  time_t current_time = time(nullptr);
  if(current_time < 0){
    perror("smash error: time failed");
    return;
  }

  this->entry_time = current_time;
  this->is_stopped = true;
}

JobsList::JobsList() : max_job_id(0) {}

void JobsList::addJob(string cmd, pid_t pid, bool isStopped) {
  time_t current_time = time(nullptr);
  if(current_time < 0){
    perror("smash error: time failed");
    return;
  }
  JobsList::JobEntry job(++max_job_id, cmd, pid, current_time, isStopped);
  job_vector.push_back(job);
}

void JobsList::printJobsList(){
  time_t current_time = time(nullptr);
  if(current_time < 0){
    perror("smash error: time failed");
    return;
  }

  for(auto job : job_vector){
    job.printEntry(current_time);
  }
}

void JobsList::killAllJobs() {
  for(auto it = job_vector.begin(); it != job_vector.end(); ++it){
    if(kill(it->process_id, SIGKILL) < 0) {
      perror("smash error: kill failed");
    }
  }
}

void JobsList::removeFinishedJobs() {
  SmallShell& smash = SmallShell::getInstance();
  if(smash.forked_from_smash) return;
  
  auto it = job_vector.begin();
  while(it != job_vector.end()){
    pid_t process_id = it->process_id;
    pid_t res_pid = waitpid(process_id, nullptr, WNOHANG);
    if(res_pid > 0){ // collected a finished process, remove job from list
      it = job_vector.erase(it);
    } else { // not finished
      ++it;
    }
  }
  
  // update max job id
  if(job_vector.empty()){ // if empty, max is 0
    max_job_id = 0;
  } else { // else, max jobs id is the LAST item in vector
    max_job_id = job_vector.back().job_id;
  }
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
  // loop over jobs vector, break when found the correct id
  for(auto it = job_vector.begin(); it != job_vector.end(); ++it){
    if(it->job_id == jobId){
      // found it
      return &(*it); // return the address (&) of the job (*it)
    }
  }

  // reaching here means job not found.
  return nullptr;
}

// NOTE: this method doesn't kill the process
// it only removes from list.
// have to kill it separately before or after removal
void JobsList::removeJobById(int jobId) {
  for(auto it = job_vector.begin(); it != job_vector.end(); ++it){
    if(it->job_id == jobId){
      job_vector.erase(it);
      break;
    }
  }
}

JobsList::JobEntry* JobsList::getLastJob(int* lastJobId){
  if(job_vector.empty()) return nullptr;

  *lastJobId = job_vector.back().job_id;
  return &(job_vector.back());
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int* jobId){
  // iterate in REVERSE (from end to start)
  // return the first stopped job (from the end)
  for(auto it = job_vector.rbegin(); it != job_vector.rend(); ++it){
    // if found, return the address (&) of the job (*it)
    if(it->is_stopped){
      *jobId = it->job_id;
      return &(*it);
    }     
  }

  // reaching here means no stopped jobs have been found
  return nullptr;
}

// ====================== End of JobList and JobEntry ==================== //
