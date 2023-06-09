# pns-con-mapper
一种基图像处理的手柄操作辅助程序的思路方案

用于辅助游戏 战双
手柄上每个具体的按键将能对应某个具体颜色的技能球

## 功能介绍

X-- 红 Y --黄 A-- 蓝色 B -- 白色

通过组合键的方式来决定消除哪一组

使用多个组合键来完成选择

比如 A 是最右边一组 LT + A 是右起第二组， LS + A 是第三组

如：
[红 ] [ 黄] [红] [ 黄] [蓝] [蓝 蓝 蓝]

对应按键为：

[LT + X ] [LT + Y ]  X  Y   [LT+A] A


同一个颜色能出现第四组的情况应该很少....

可能是一个更容易学习的操作方式 ^_^



## 基本实现原理

程序基于C语言实现， 通过使用arm-linux工具链编译，直接运行在模拟器中

程序基本原理是
fork 子进程 使用 Android原生的 screenrecord 脚本 获取屏幕的rgb24数据流，

通过对指定的区域进行图像分析来辨别每个技能的颜色

统计图标区域中的像素， 寻找纯[白色]边缘的像素，并计算它们与设定的 红 蓝 黄 标准色的余弦相似度并打分
最终判断每个技能球的具体结果

将判断结果组成规定的字符串， 通过管道发送给用于可视化的Service, Service 生成一个覆盖全屏幕的悬浮窗
根据识别结果与按键状态绘制可视化辅助

最后通过读写 /dev/input/event 的方式， 读取手柄的输入，并且映射到屏幕指定区域发送触摸事件


## 使用说明

开发环境 Mumu 模拟器 + XBox One 手柄

确保你的模拟器能够使用使用adb 链接

参考文档：[MuMu模拟器6如何连接adb_MuMu模拟器_安卓模拟器 (163.com)](https://mumu.163.com/help/20210531/35047_951108.html)

执行 ：

将adb连接到模拟器
adb connect 127.0.0.1:7555

创建temp文件夹
adb shell "mkdir -p  /data/local/temp"

将执行程序推入设备
adb push con-mapper  /data/local/temp

为执行程序赋予可执行权限
adb shell \"chmod a+x /data/local/temp/con-mapper"

可选：
安装可视化Service 提供视觉支持
adb install assister/pns-conmapper-assister.apk


运行
adb shell "/data/local/temp/con-mapper"


首次启动时需要标定技能栏位置





