# xcore
A simple x86 os core  
源码使用：  
&emsp;&emsp;先在Linux下用make编译，然后在Windows中运行run.bat即可（注意Bochs的安装目录）。至于为什么不全在Linux/Windows开发，原因很简单：因为全在Linux上开发，可能有些人不太习惯；全在Windows也不行，因为我们使用的是ELF文件格式，在Windows上交叉编译比较复杂。因此，用一种折衷的方案：用virtualbox运行Centos-minimal（最小安装包），然后用共享文件夹的方式，在Windows用notepad++写好代码，在Centos上用make编译一下，然后在Windows运行run.bat即可看到效果，调试也非常方便。  
声明：  
&emsp;&emsp;xcore是本人在阅读《操作系统真象还原》（郑钢著）及xv6源码（MIT）等参考资料后，在理解的基础上写的一个小型操作系统内核，权当练手巩固知识。如有侵权，请联系删除。
