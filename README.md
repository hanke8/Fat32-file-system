# Fat32-file-system

## 介绍

1. 采用Fat32数据结构，基于共享内存的文件系统。
2. 支持功能如下：
   - 目录相关操作：mkdir, rmdir
   - 文件相关操作：touch, vim, rm, cat
   - 其他操作：mv, ls, cd, clear, exit, dismiss, help
3. 采用信号量实现读写锁，来控制文件的并发访问控制。


![image](https://user-images.githubusercontent.com/64403987/177193904-96422946-f9f2-4876-8cdd-40b1d7847b3c.png)


![image](https://user-images.githubusercontent.com/64403987/177193955-341e9f90-9d75-4dc8-a305-c06e7b55e707.png)

## 环境

- linux C

## 启动

```
./run.sh
```
