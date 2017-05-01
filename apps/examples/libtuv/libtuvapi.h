
enum watch_io {
	WATCH_IO_IN = (1 << 0),	/**< data input available */
	WATCH_IO_OUT = (1 << 1), /**< data can be written */
	WATCH_IO_PRI = (1 << 2), /**< high priority input available */
	WATCH_IO_ERR = (1 << 3), /**< i/o error */
	WATCH_IO_HUP = (1 << 4), /**< Hung up. device disconnected */
	WATCH_IO_NVAL = (1 << 5)
					/**< invalid request. the file descriptor is not open */
};

typedef void (*timeout_callback)(void *user_data);
typedef int (*watch_callback)(int fd, enum watch_io io, void *user_data);
typedef int (*idle_callback)(void *user_data);

void libtuv_loop_run(void);
void libtuv_loop_quit(void);
int libtuv_add_timeout_callback(unsigned int msec, timeout_callback func, void *user_data);
int libtuv_add_fd_watch(int fd, enum watch_io io, watch_callback func, void *user_data, int *watch_id);
int libtuv_add_idle_callback(idle_callback func, void *user_data);
