#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *target);

void find(char *path, char *target) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 如果传入的 path 是目录，我们遍历其中的所有文件和子目录
    if (st.type == T_DIR) {
        // 准备好路径缓冲区
        strcpy(buf, path);
        p = buf + strlen(buf);
        if (p[-1] != '/') {
            *p++ = '/';
        }

        // 遍历当前目录中的每个目录项
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;

            // 忽略当前目录 "." 和父目录 ".."
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            // 构造子目录/文件的完整路径
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            // 获取文件/目录的状态
            if (stat(buf, &st) < 0) {
                printf("find: cannot stat %s\n", buf);
                continue;
            }

            // 如果名字匹配目标 target，输出相对路径
            if (strcmp(de.name, target) == 0) {
                printf("%s\n", buf);
            }

            // 如果是目录，递归调用 find 继续查找
            if (st.type == T_DIR) {
                find(buf, target);
            }
        }
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage: find <path> <name>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}
