#include "kernel/types.h"
#include "user/user.h"

int main() {
    int p1[2], p2[2];
    int ppid_buf;
    char buf[4]; 
    pipe(p1); // PtoC
    pipe(p2); // CtoP

    int child_pid = fork(); 

    if (child_pid == 0) {
        // Child process
        close(p1[1]); // Close write end of p1
        close(p2[0]); // Close read end of p2

        read(p1[0], &ppid_buf, sizeof(ppid_buf));
        read(p1[0], buf, 4); // Read "ping" from parent
        printf("%d: received ping from pid %d\n", getpid(), ppid_buf);
        write(p2[1], "pong", 4); // Write to parent

        close(p1[0]);
        close(p2[1]);
    } else {
        // Parent process
        close(p1[0]); // Close read end of p1
        close(p2[1]); // Close write end of p2

        int parent_pid = getpid();
        write(p1[1], &parent_pid, sizeof(parent_pid));
        write(p1[1], "ping", 4); // Write to child
        read(p2[0], buf, 4); // Read from child
        printf("%d: received pong from pid %d\n", getpid(), child_pid);

        close(p1[1]);
        close(p2[0]);
    }

    exit(0);
}
