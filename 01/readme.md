### 01、Hello World
a、int 0x10  
&emsp;&emsp;使用BIOS中断（int 0x10）显示“hello，world”  
b、操作显存  
&emsp;&emsp;直接往显存写入“hello，world”字符。显存地址分布如下：  
|起始|结束|大小|用途|
|-:-|-:-|-:-|-:-|
|C0000|C7FFF|32KB|显示适配器BIOS|
|B8000|BFFFF|32KB|用于文本模式显示适配器|
|B0000|B7FFF|32KB|用于黑白显示适配器|
|A0000|AFFFF|64KB|用于彩色显示适配器|
&emsp;&emsp;我们使用文本模式就好了，类似Linux终端的字符界面。代码：https://github.com/lmkang/xcore/01