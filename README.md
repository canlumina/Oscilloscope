# STM32 双通道简易示波器

基于 **德飞莱超翼M35（STM32F103ZET6）+ 2.8" ILI9325 LCD（FSMC）** 的双通道示波器。专为观察 STM32 GPIO 信号（0~3.3V）设计，可用于查看 PWM 互补输出、死区时间等数字波形。

![screenshot placeholder]: 上半屏 CH1（绿色）+ 下半屏 CH2（青色）分屏显示

---

## 主要功能

- **双通道同步采样**：ADC1+ADC2 DUAL REGSIMULT 模式，CH1/CH2 每个采样点严格时间对齐（适合观察 PWM 互补输出 + 死区）
- **分屏显示**：CH1 占上半屏、CH2 占下半屏，互不重叠，每路独立 0~vscale 单极性映射
- **12 档时基**：50 µs/div ~ 200 ms/div，最快档逼近 F103 ADC 物理极限（~1.17 µs/转换）
- **7 档电压量程**：62.5 mV/div ~ 2 V/div，默认 **3.3 V**（STM32 GPIO 全幅）
- **实时测量**（CH1 / CH2 各一套）：Vpp、Max、Min、Avg、频率、高电平脉宽 Thi、低电平脉宽 Tlo
- **触发**：AUTO / NORM / SING 三模式（默认 AUTO，以 CH1 为触发源，固定 1.65 V 上升沿）
- **三按键操作**：KEY0 切时基、KEY1 切量程、KEY2 Run/Stop
- **增量绘制**：缓存每列上一帧 Y 增量擦+画，避免整屏闪烁

---

## 硬件接线

| 接口 | 引脚 | 说明 |
|------|------|------|
| **CH1** | PA0（ADC1_IN0） | 信号输入，必须 0 ~ 3.3 V |
| **CH2** | PA1（ADC2_IN1） | 信号输入，必须 0 ~ 3.3 V |
| LCD | 板载 TFT 排母 → FSMC NE4 + A10 | RS=PG0, CS=PG12, RST=PC5 |
| KEY0 | PE4 | 时基切换 |
| KEY1 | PE3 | 量程切换 |
| KEY2 | PE2 | Run/Stop |
| 调试串口 | USART1（PA9/PA10） | 115200 8N1，上电打印 `LCD ID:9325` |

> ⚠️ **PA0 / PA1 严禁超过 3.3 V 或负压**，否则烧 MCU。测大信号或交流信号请先加分压/钳位电路。

---

## 性能与物理极限

| 项目 | 值 |
|------|---|
| ADC | STM32F103 内置 12-bit × 2，DUAL REGSIMULT 模式 |
| 每路采样率 | 最高约 **857 kSPS**（ADCCLK = 12 MHz，14 cycles/转换） |
| 缓冲深度 | 240 点 / 屏（每点含 CH1 + CH2 共 32 bit） |
| 显示分辨率 | 320 × 240（横屏） |
| 波形区 | 240 × 192（CH1 上半 + CH2 下半，每路 240 × 96） |
| 时间分辨率 | 最快档 50 µs/div = **2 µs/像素**（每对样本间隔 2 µs） |
| 电压分辨率 | 默认 3.3 V 档下 ≈ 34 mV/像素（半屏 96 像素覆盖 3.3 V） |

**死区观察能力**：F103 双 ADC 最快 ~2 µs/采样对，意味着死区时长 ≥ ~4 µs 才能在屏幕上看到 ≥2 像素宽的过渡。**2.4 µs 级死区只能看到 1 像素宽的过渡线，要看清需要换 F4（每路 2.5 MSPS）**。

---

## 构建与烧录

工程入口：`Project/Oscilloscope.uvprojx`，编译器 **ARM Compiler 6 (AC6)**。

### Keil GUI

打开 `Project/Oscilloscope.uvprojx`，按 F7 编译，F8 烧录（需配置 ST-Link / SWD）。

### 命令行（bash 环境）

```bash
# 构建（输出到 Project/Objects/Oscilloscope.hex）
"/c/Users/yangcan/AppData/Local/Keil_v5/UV4/UV4.exe" -b Project/Oscilloscope.uvprojx -j0 -o Project/build.log
cat Project/build.log

# 烧录（ST-Link SWD）
STM32_Programmer_CLI.exe -c port=SWD -w Project/Objects/Oscilloscope.hex -v -rst

# 查看 Flash/RAM 占用
grep -E "Total RO|Total RW|Total ZI" Project/Objects/Oscilloscope.map
```

当前 Flash ≈ 37 KB / SRAM ≈ 8.6 KB，远低于 STM32F103ZE 的 512 KB / 64 KB。

---

## 使用方法

上电后状态栏显示默认值 `500us/d  3.3V  AUTO  /\  RUN  CH1+CH2`，波形区上下两区分别显示 CH1（绿）和 CH2（青）。

- **KEY0** 按一次往更慢档跳；最慢档（200 ms/div）后循环回最快档（50 µs/div）
- **KEY1** 切电压量程（双通道共用），共 7 档
- **KEY2** 切 Run / Stop（状态栏显示 RUN / STP）

切档时屏幕自动清波形区+重画网格，旧波形不残留。

详细使用说明：见 **[Doc/使用说明.md](Doc/使用说明.md)**。

---

## 屏幕布局

```
┌────────────────────────┬──────┐  y=0
│ 时基 量程 模式 边沿 RUN │CH1+CH2│  状态栏 16px
├────────────────────────┼──────┤  y=16
│ ▶ CH1（绿）              │ Vpp..│
│   0V→中线，3.3V→屏顶     │ Max..│
│                          │ Min..│
├──────── 中央分隔线 ──────┤ Avg..│  y=112
│ ▶ CH2（青）              │ Frq..│
│   0V→屏底，3.3V→中线     │ Thi..│
│                          │ Tlo..│
│                          │ Trg..│
├────────────────────────┴──────┤  y=208
│  K0:Tbase  K1:Vscale  K2:R/S  │  底部按键栏 32px
└────────────────────────────────┘  y=239
```

- 上半屏 4×10 格 + 下半屏 4×10 格，每格 24×24 像素
- 触发线（黄色虚线）只在 CH1 半屏显示
- 右栏 80 px：标签灰 / CH1 数值绿 / CH2 数值青三列对齐

---

## 目录结构

```
Oscilloscope/
├── Project/                    # Keil μVision 工程
│   ├── Oscilloscope.uvprojx
│   ├── Objects/                # 构建产物 (.hex/.axf/.map)
│   ├── Listings/
│   └── RTE/Device/             # Keil RTE 设备配置头
├── User/                       # 应用入口
│   ├── main.c                  # 主循环：扫键→采样→测量→绘制
│   ├── main.h                  # 汇总 BSP 头
│   ├── stm32f1xx_it.c          # 中断向量（HAL 风格）
│   └── stm32f1xx_it.h
├── Drivers/
│   ├── Inc/                    # 板级驱动头文件
│   └── Src/                    # 板级驱动 + 示波器业务代码
│       ├── adc_sample.{c,h}    # ★ ADC1+ADC2 同步采样 + DMA + TIM3
│       ├── oscilloscope.{c,h}  # ★ 波形/测量/UI
│       ├── lcd.{c,h} + lcd_ex.c + lcdfont.h   # ILI9325 FSMC 驱动
│       ├── button.{c,h}        # 三按键扫描
│       ├── uart.{c,h}          # USART1 调试输出
│       ├── delay.{c,h}         # SysTick 微秒延时
│       ├── systemclock.{c,h}   # HSE×9 → 72 MHz
│       ├── timer.{c,h} + led.{c,h} + 24cxx.{c,h} + iic.{c,h}
│       ├── spi.{c,h} + spi_sd.h + spisd.c + touch.{c,h}
├── STM32F1xx_HAL_Driver/       # ST 官方 HAL 库（仅工程引用的 c 编入）
├── CMSIS/                      # CMSIS 头 + startup_stm32f103xe.s
├── Doc/
│   ├── 使用说明.md             # 面向使用者的操作手册
│   └── 调试记录.md             # 9 个踩坑过程的根因+修复记录
├── README.md                   # 本文件
└── CLAUDE.md                   # Claude Code 工程上下文
```

★ 标记的两个模块是示波器核心，其余是板级驱动复用。

---

## 已知限制

- **2 µs 死区以下不可分辨**：F103 ADC 物理极限 ~1.17 µs/转换，受此约束
- **AOTO 模式触发线不稳**：当前固定 1.65 V 上升沿，AUTO 模式下不强制对齐，画面会"漂"；NORM 模式更稳但需要代码层切换（修改 `g_osc.trig_mode` 初值）
- **采样间隔超过 PWM 周期一半时混叠**：当前选择时基档需匹配信号频率，否则波形失真
- **PA0 / PA1 直连 ADC 无保护**：超 3.3 V 直接损坏 MCU，外部需自行加分压/钳位

---

## 文档

- **[Doc/使用说明.md](Doc/使用说明.md)** — 按键操作、量程档位表、典型场景（测互补 PWM）
- **[Doc/调试记录.md](Doc/调试记录.md)** — 工程从空白到双通道运行过程中遇到的 9 个坑（SPL/HAL 冲突、LCD API 不匹配、HAL_GetTick 冻结、F1 HAL dual ADC 两个 bug 等），含 STM32F1 HAL 双 ADC dual REGSIMULT **完整启用要点清单**
- **[CLAUDE.md](CLAUDE.md)** — Claude Code AI 助手的工程上下文与行为约束

---

## License

教学/学习用途，无商业授权限制。
