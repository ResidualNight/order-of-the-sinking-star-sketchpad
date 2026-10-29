# ootss-cmd 镜头定位参考评估

日期：2026-10-29（Asia/Shanghai）。用户要求查看 https://github.com/qiekn/ootss-cmd 的镜头定位算法，判断 StarSketch 是否可以参考。

后续用户已授权本地试用接入，当前实现与验证边界见 [场景相机试用接入](场景相机试用接入.md)。本文记录前一轮静态评估。

同日再次核对远程 HEAD 仍为 `cf432e95962b946c80a9e6fdea6dba307dc757c9`。重点重读 `game_camera_snapshot` / `game_camera_read` / `game_camera_movement`：上游直接读取并验证活动场景 pose，不要求栈里的 Viewpoint_Params；freecam 冻结和写入是另一路功能。StarSketch 最新本地版本已移除自行添加的 type=8 手动预览门槛，保留只读、完整透视和明确采用；额外检查俯视/滚转/旋转姿态的前向、屏幕右和屏幕上投影约定。上游不存在可直接拿来使用的 StarSketch 屏幕网格算法，单应映射和反向深度矩阵仍由我们验证实现。

本次下载并静态阅读了该仓库提交 `cf432e95962b946c80a9e6fdea6dba307dc757c9`（提交时间 2026-10-20）。副本位于忽略目录 `references/ootss-cmd/`。没有运行对方程序或测试，没有启动、读取或修改真实游戏进程，没有修改 StarSketch 运行时或安装包。下文把源码行为、上游记录和本次推断分别说明。

## 结论

值得优先参考的是**相机数据来源和来源校验**。上游从活动场景管理器读取运行时相机位置、四元数、垂直 FOV、near/far，而 StarSketch 当前从线程栈寻找完整 `Viewpoint_Params`。这提供了一条避开短命栈副本的具体研究路线；它尚不等于已经获得与最终屏幕一致、可直接替换的网格投影。

特别需要区分：上游读取的不是关卡文件里 `Camera_Control` 的初始姿态，也不是相机列表第一项的位置。相机列表用于身份校验；实际姿态来自活动 manager 的运行时字段。这与旧评估中“静态 Camera_Control 初始姿态不能代替当前相机”的结论并不冲突。

## 上游实际算法

1. `memory.jai::game_entity_manager_slot` 扫描已加载 PE 的 `.text`，匹配当前场景引用特征。按有符号 RIP 相对位移计算全局指针槽，再读取活动 manager。拒绝多个匹配、越界及截断；保存完整命中字节供后续复核。源码也含一个带原始指令校验的旧 trainer 回退，不能把这个固定 RVA 回退直接搬进新路径。
2. `camera_layout.jai::game_camera_layout` 使用 `retail-r13-2026-10-17b` 配置，查找相机更新模式分支、渲染参数读取和活动场景引用。解码 position/orientation/projection/cameras 偏移，并核对跟随分支、列表读取、冻结分支目标和渲染参数是否互相一致。保存六段代码证据，每次快照可重新检查。
3. `game_camera_snapshot` 读取 manager 的运行时位置、四元数及三个投影标量。验证场景名、最多 64 个 `Camera_Control` 的类型、owner、ID、列表指针及类型结构信息；读取结束再次复核场景和相机身份。位置、四元数和投影也有有限值及范围检查，四元数模平方误差不超过 0.002。
4. `camera.jai` 说明该游戏相机局部轴为 +X 前、+Y 左、+Z 上，并提供四元数转朝向/移动方向的公式。`game_camera_read` 是独立只读入口；freecam 的模式 2 冻结、位置/旋转写入和恢复属于另一套会话逻辑，StarSketch 参考读取算法无需移植这些写操作。

上游记录中，与本项目既有研究相同的 EXE SHA256 `0ae10a513dae25286fe93d0db4fa8a2df9a473bc07e1734e12bc1ad30166db1c` 对应以下值：

| 项目 | 上游记录的 r67271 值 | 用途 |
| --- | --- | --- |
| 活动 manager 指针槽 | 主模块 RVA `0xB69600` | 与 StarSketch 当前根一致 |
| 位置 | `manager + 0x3D98`，3 个 float | 运行时相机世界坐标 |
| 朝向 | `manager + 0x3DB0`，4 个 float | 四元数 |
| 投影参数 | `manager + 0x3DC0`，3 个 float | 垂直 FOV（弧度）、near、far |
| 相机列表 | `manager + 0x538` 的 count/data | 身份、类型与场景归属核对 |

这些是上游源码/文档证据，本次没有对本机或受影响机器重新实测，不能当作跨版本固定地址。

## 对 StarSketch 的意义与限制

| 可参考部分 | 判断 |
| --- | --- |
| 活动 manager 内的运行时姿态来源 | 优先研究。若与最终主视图对应，可减少对栈中 960 字节临时副本的依赖 |
| 唯一指令匹配、RIP 解码、多处引用交叉校验 | 可借鉴。也为当前固定 manager RVA 提供版本适配思路，但 Settings 等布局仍须单独验证 |
| 读取前后的场景/相机身份核对 | 可借鉴，补充当前关卡与 manager 保护 |
| 四元数局部轴约定 | 可用于重建 view 及核对方向；仍须与本项目矩阵约定对照 |
| `game_camera_ground_move` 的最近网格轴 | 用于角色移动，不能作为普通斜视网格的屏幕投影；取最近正交轴会丢失真实方向与透视 |
| `capture_make_grid` 的像素/世界单位换算 | 适用于固定俯视、固定高度、参考平面的地图拼接，不能直接推广到普通斜视网格 |
| freecam 冻结与写入 | 本次目标不需要 |

对当前“type=8 冗余投影逆不完整、type=5 反射有效”的问题，新的来源有望绕开故障采集位置，这是推断。上游没有证明这些 manager 字段在受影响机器上一定等于本帧最终主视图，也没有证明它能解释此前损坏副本的具体原因。

移植前还缺以下证据：

- 上游快照只含姿态和 FOV/near/far，不含本项目使用的完整 `transform`、`inverse_transform`、`map_amount` 和渲染帧号。不能伪造帧推进，或把数次相同姿态误判为过期；也不能仅因新算出的矩阵自洽就认定来源正确。
- 需要验证四元数转换、view/projection 组合、宽高比与最终输出映射，以及地图/过渡/特殊相机状态。运行时投影可能受其他参数影响；这些参数是否存在、是否需要补读，必须实测，不能凭 FOV 三个标量作保证。
- 分开读取位置、朝向和投影，即使前后身份相同，也可能跨过游戏更新时刻。上游身份检查不是同帧原子快照证明。新来源需要自己的采样一致性与新鲜度证据；保留现有栈来源的原检查。
- 当前相机配置仅保留一份特定指令形态；manager 正式版签名也含具体字段偏移。因此它不是通用的跨版本扫描器。寄存器、分支、字段或编译结果变化仍需维护，定位失败不能使旧偏移自动生效。

## 建议的后续验证顺序

1. 先做独立只读探针，按上述来源与校验读取运行时姿态，和同一时段已通过校验的主视图 origin/direction/投影对照；覆盖普通斜视、镜头转向、地图进出、切关、暂停与尺寸/渲染档位变化。
2. 在堆上重建 view/projection，对不同屏幕区域和多个高度的世界点比较投影残差，再用实际地砖叠加核对。沿用 StarSketch 完整平面单应映射及整数坐标为格心的约定，不能用等距仿射网格替代。
3. 第一版接入可先作为明确标记的新来源预览，由用户确认采用。来源、采样、地图状态和画面贴合均验证后，再评估自动定位优先级；签名失败保留现有恢复路径。
4. 保留直接预览快照采用、会话 pin/backup、取消恢复与不可用关卡暂停行为。不得重新加入采用时的 012 重校验门，不持久化相机或网格几何。

## 固定提交源码入口

- [活动场景定位 memory.jai L187–284](https://github.com/qiekn/ootss-cmd/blob/cf432e95962b946c80a9e6fdea6dba307dc757c9/src/game/memory.jai#L187-L284)
- [相机布局与多处证据 camera_layout.jai L75–152](https://github.com/qiekn/ootss-cmd/blob/cf432e95962b946c80a9e6fdea6dba307dc757c9/src/game/camera_layout.jai#L75-L152)
- [姿态、身份与快照 camera_layout.jai L154–266](https://github.com/qiekn/ootss-cmd/blob/cf432e95962b946c80a9e6fdea6dba307dc757c9/src/game/camera_layout.jai#L154-L266)
- [轴约定与只读入口 camera.jai L166–201](https://github.com/qiekn/ootss-cmd/blob/cf432e95962b946c80a9e6fdea6dba307dc757c9/src/game/camera.jai#L166-L201)
- [俯视拍摄换算 capture_plan.jai L59–81](https://github.com/qiekn/ootss-cmd/blob/cf432e95962b946c80a9e6fdea6dba307dc757c9/src/game/capture_plan.jai#L59-L81)
- [上游版本研究记录 game-memory.md L388–394](https://github.com/qiekn/ootss-cmd/blob/cf432e95962b946c80a9e6fdea6dba307dc757c9/docs/game-memory.md#L388-L394)
- [布局及证据篡改回归 check_game_layout.jai](https://github.com/qiekn/ootss-cmd/blob/cf432e95962b946c80a9e6fdea6dba307dc757c9/scripts/check_game_layout.jai)

对方仓库自带 LICENSE，并明确写有允许使用、复制、修改与分发的声明。后续若移植实现，应保留清晰来源记录并按实际采用内容整理第三方说明。本次仅分析，没有复制代码进入运行时。
