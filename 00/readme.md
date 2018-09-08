### 00、搭建开发环境
1、开发工具
```
CentOS-6.9-x86_64-minimal.iso
VirtualBox-5.1.22-115126-Win.exe(windows)
Bochs-2.6.8(windows)
Bz1621.lzh(Binary Editor)
gcc
nasm
dd
Notepad++(windows)
```
2、VirtualBox设置linux和windows共享文件夹  
（1）设置-->共享文件夹-->名称(linuxshare)，自动挂载，固定分配  
（2）设置-->存储-->控制器：IDE-->VBoxGuestAdditions.iso  
（3）执行命令：
```
yum install -y gcc gcc-c++
yum install -y autoconf
yum install -y automake
yum install -y kernel-devel
mkdir /home/share
mount /dev/cdrom /home/share
cd /home/share
./VBoxLinuxAdditions.run
mount -t vboxsf linuxshare /usr/local/winshare
```
3、开发过程  
（1）在windows用Notepad++写代码，在linux中用nasm、gcc编译和用dd打包，然后在windows中用Bochs调试运行  
（2）Notepad++(windows)-->make(linux)-->run.bat(windows)  
4、注意事项  
&emsp;&emsp;编译/链接32位的C代码：
```
gcc -m32 -c -o main.o main.c
ld main.o -melf_i386 -Ttext 0xc0001500 -e main -o kernel.bin
```