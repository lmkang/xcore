### 01、Hello World
a、int 0x10  
&emsp;&emsp;使用BIOS中断（int 0x10）显示“hello，world”  
b、操作显存  
&emsp;&emsp;直接往显存写入“hello，world”字符。显存地址分布如下：  
<table>
	<tr>
		<td>起始</td>
		<td>结束</td>
		<td>大小</td>
		<td>用途</td>
	</tr>
	<tr>
		<td>C0000</td>
		<td>C7FFF</td>
		<td>32KB</td>
		<td>显示适配器BIOS</td>
	</tr>
	<tr>
		<td>B8000</td>
		<td>BFFFF</td>
		<td>32KB</td>
		<td>用于文本模式显示适配器</td>
	</tr>
	<tr>
		<td>B0000</td>
		<td>B7FFF</td>
		<td>32KB</td>
		<td>用于黑白显示适配器</td>
	</tr>
	<tr>
		<td>A0000</td>
		<td>AFFFF</td>
		<td>32KB</td>
		<td>用于彩色显示适配器</td>
	</tr>
</table>
&emsp;&emsp;我们使用文本模式显示适配器就行了，类似于Linux终端的字符界面。