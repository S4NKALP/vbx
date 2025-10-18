#ifndef VBX_PROCESS_H
#define VBX_PROCESS_H

#include <signal.h>
#include <sys/types.h>

extern pid_t keyboard_pid;
extern pid_t sound_pid;
extern char pidfile_path[];
extern int is_daemon;
extern volatile sig_atomic_t reload_requested;

void cleanup_processes(int sig);
int require_running_pid(pid_t *out_pid);
void daemonize_self(void);
void stop_children(void);
int start_children(const char *sound_dir, const char *config_path, int volume,
                   int verbose, int mute, const char *mouse_sound_dir,
                   const char *mouse_config_path, int mouse_volume,
                   int keyboard_mute, int mouse_mute, int keyboard_enabled, int mouse_enabled);
void handle_sighup(int sig);

#endif // VBX_PROCESS_H
