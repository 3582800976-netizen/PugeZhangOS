# 课程环境与开发板教程

本目录也保存[Lab 0 总体课件](拔尖班-lab-0.pptx)，来源是[课程仓库 Lab 0 PPT](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/lab-ppt/拔尖班-lab-0.pptx)，于 2026-11-02 下载，大小 188001 字节，SHA-256 为 `713aa6379226b1f8f2c697144af2f4c3ace85eefb3c31a26b7854f4072aec2e3`，PPTX ZIP 完整性检查通过。

## 环境与开发板资料

2026-10-13（Asia/Shanghai）从[课程仓库 docs 目录](https://gitee.com/christinaaa/ecnu-oslab-2026-task/tree/master/docs)下载以下原件。内容未修改，均已检查 DOCX 压缩包完整性并核对课程文件的 Git blob SHA-1。

| 本地文件 | 用途 | 课程原件 |
| --- | --- | --- |
| [vf2.docx](vf2.docx) | VisionFive 2 的供电、写卡、串口接线、WSL USB 转发与系统启动 | [来源](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/docs/vf2.docx) |
| [ssh到虚拟机攻略.docx](ssh到虚拟机攻略.docx) | SSH 连接开发用虚拟机 | [来源](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/docs/ssh到虚拟机攻略.docx) |
| [操作系统实验环境配置26版.docx](操作系统实验环境配置26版.docx) | 课程开发环境配置 | [来源](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/docs/操作系统实验环境配置26版.docx) |

开发板准备先看 `vf2.docx`；它适用于 VisionFive 2，需核对实际板型后使用其接线与镜像说明。Windows PowerShell、WSL Ubuntu、开发板上的 Ubuntu 是三个不同的命令环境，教程中的命令要在对应环境执行。

本次下载和电脑端检查由 Agent 完成，尚未取得开发板开机日志，也没有写入 SD 卡或完成上板验收。开发机为 WSL2、Ubuntu 24.04.4；随后检查已确认 `minicom` 与 `lsusb` 可用，USB 串口尚未出现在 WSL。Windows 端已安装 usbipd-win 5.3.0，服务正常；尚未识别到 CH340。开发板配置按本人要求暂缓，实际连接与上板结果以设备识别和开机日志为准。

| 文件 | 字节数 | Git blob SHA-1 |
| --- | --- | --- |
| `vf2.docx` | 1513712 | `7a5adcb9a6f0a5fdf3691dbaa9e37d88cd091695` |
| `ssh到虚拟机攻略.docx` | 1212600 | `a2619c8e30da26467ca87276898dbc9f42187207` |
| `操作系统实验环境配置26版.docx` | 2946855 | `3cf68bc932eb96302cdc93dbca6c9d7ad73f6611` |

SHA-256：

```text
vf2.docx
3966f3137c20a3568cc37680a0489be881a30cf3bc67afb46e4f12e933ee0a91

ssh到虚拟机攻略.docx
c054e8ddcff417be9aaa97237350fa98fde1df2bcb5978a4bd02f0dd81adea62

操作系统实验环境配置26版.docx
fae937ff7161d20f26e52ada2be1298caa9200f2a703bb41304c1aa57e5ec01a
```
