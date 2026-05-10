# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

简易示波器，目标硬件为**德飞莱超翼M35 (STM32F103ZET6)**，2.8" ILI9325 LCD（FSMC 接口）+ ADC1_CH0(PA0) + DMA + TIM3 触发定时采样。
工程在 **Keil MDK (μVision 5)** 中构建，编译器为 **ARM Compiler 6 (AC6)**，工程入口 `Project/Oscilloscope.uvprojx`。

## 构建 / 烧录

工作目录是 Windows，shell 是 bash —— 需要执行 Keil 命令时使用 Windows 路径但用 bash 语法（`cmd.exe //c` 或 `MSYS_NO_PATHCONV=1`）。

```bash
# 命令行构建（输出到 Project/Objects/Oscilloscope.hex）
"/c/Keil_v5/UV4/UV4.exe" -b Project/Oscilloscope.uvprojx -j0 -o Project/build.log
cat Project/build.log

# 烧录（ST-Link / SWD）
STM32_Programmer_CLI.exe -c port=SWD -w Project/Objects/Oscilloscope.hex -v -rst

# 查看 RO/RW/ZI 占用
grep -E "Total RO|Total RW|Total ZI" Project/Objects/Oscilloscope.map
```

构建产物：`Project/Objects/`（.hex/.axf/.map），列表文件：`Project/Listings/`。

## 代码架构

**目录布局**（与 README 中描述的"User/inc + User/src"结构 *不一致*，以下是实际结构）：

- `Project/` — Keil 工程文件、构建产物、RTE
- `User/` — 入口（`main.c`、`main.h`、`stm32f1xx_it.c`）
- `Drivers/Inc/` + `Drivers/Src/` — 板级驱动（**所有 BSP 与示波器业务代码均在此**）
  - 示波器核心：`adc_sample.c`、`oscilloscope.c`、`lcd.c`、`lcd_ex.c`、`button.c`
  - 其他外设：`uart.c`、`spi.c`、`spisd.c`、`iic.c`、`24cxx.c`、`touch.c`、`ft5206.c`、`gt9xxx.c`、`timer.c`、`delay.c`、`led.c`、`systemclock.c`、`mymalloc.c`
- `STM32F1xx_HAL_Driver/` — 标准 STM32 HAL 库（仅子集编入工程，见 `.uvprojx`）
- `CMSIS/` — CMSIS 头文件 + `startup_stm32f103xe.s` + `system_stm32f1xx.c`
- `Project/RTE/Device/` — Keil RTE 设备配置头（`stm32f1xx_hal_conf.h` 等）

**包含路径与宏**（从 `.uvprojx` 提取）：
- `Define`：`USE_HAL_DRIVER, STM32F103xE`
- `IncludePath`：`CMSIS/Include; CMSIS/Device/Include; STM32F1xx_HAL_Driver/Inc; User; Drivers/Inc`，外加预留的 `Middlewares/{FreeRTOS,LVGL,ff15}/...`（**目前 Middlewares 目录尚未建立**，include 路径已先行铺好）

### ⚠️ 已知架构不一致（动手前务必读）

1. **HAL 与标准外设库混用**。工程已切到 HAL（`USE_HAL_DRIVER`），但下列文件仍 `#include "stm32f10x.h"`（标准外设库头），并使用 `RCC_APB2Periph_*` / `ADC_InitTypeDef` 等 SPL API：
   - `User/main.c`
   - `Drivers/Src/adc_sample.c`、`Drivers/Inc/adc_sample.h`
   - `Drivers/Inc/oscilloscope.h`
   
   而 `User/main.h`、`Drivers/Inc/button.h` 等用的是 `stm32f1xx.h` (HAL)。
   **后果：当前工程基本无法直接编译通过。** 任何工作之前需先确认是要将示波器代码迁移到 HAL，还是要把工程回退到 SPL；不要默认假设当前代码"应该能跑"。

2. **README 与现状脱节**。`README.md` 描述的文件（`User/inc/key.h`、`User/src/key.c`、`font.c`）并不存在；按键模块实际是 `Drivers/{Inc,Src}/button.{h,c}`，且 API 完全不同：
   - README 假设的：`Key_Init()` / `Key_Scan()` / 枚举 `KEY0..KEY4 / KEY_NONE`
   - 实际现有的：`button_init()` / `button_scan(uint8_t mode)`，宏 `KEY0..KEY2 / WK_UP` 是 `HAL_GPIO_ReadPin` 表达式（不是按键码），返回值是 `KEY0_PRES..WKUP_PRES`
   - `main.c` 调用的 `Key_Scan` / `KEY0..KEY4` / `KEY_NONE` 在仓库中均无定义。

3. **`adc_sample.c` 配置 STM32F103 ADC 上限超规格**。注释里写到"500us/div 档位 ~2MSPS"，但 F103 ADC 在 12-bit 下理论上限约 1MSPS（参考 RM0008 §11.6）；时基表的 `s_tim_arr[0]=1` 在 72MHz/PSC=71 下意味着 1MHz 触发率，已逼近极限。修改时基档位时同步核对此约束。

### 模块边界

- `adc_sample.{c,h}`：ADC1_CH0(PA0) + DMA1_Channel1 + TIM3 触发；导出 `g_adc_buf[SAMPLE_BUF_SIZE=240]`、`g_sample_done`、`ADC_Sample_Start/Init/SetTimebase/ToVoltage`、`g_timebase_str[9]`
- `oscilloscope.{c,h}`：UI 与测量。状态结构 `g_osc`（`OscState`），坐标常量 `OSC_WAVE_X/Y/W/H=0/16/240/240`、网格 10×8（每格 24×30 px），底部信息栏 64 px。函数：`Osc_Init/DrawFrame/DrawGrid/DrawWave/DrawTrigLine/UpdateStatusBar/UpdateInfoBar/Measure/FindTrigger`
- `lcd.{c,h}` + `lcd_ex.c`：FSMC 驱动 ILI9325，`RS=PF0(FSMC_A0)`、`CS=PG12(FSMC_NE4)`、`RST=PC5`
- `main.c` 主循环：扫按键 → 启动一次采样 → 等 DMA 完成（带超时 + 按键复扫）→ `Osc_FindTrigger` 找触发点 → memcpy 出显示窗口 → `Osc_Measure` + `Osc_DrawWave`。`TRIG_AUTO/NORMAL/SINGLE` 三种触发模式状态机在主循环里用 `g_osc.run` / `g_osc.single_done` 管控。
- 中断：`stm32f1xx_it.c` 是 HAL 风格；DMA 完成回调是采样模块设置 `g_sample_done` 的关键路径，迁移驱动时必须同步迁移此回调。

## 硬件接线（关键点）

- **信号输入 PA0：仅 0~3.3V，必须外加分压/钳位保护电路**（README 给出 10kΩ + 钳位二极管参考方案）。
- 按键引脚（按 README，但需对照 button.c 实际占用）：KEY0=PE4、KEY1=PE3、KEY2=PE2、KEY3=PE5、KEY4=PC13；WK_UP(PA0) 因被 ADC 占用**禁用**为按键。
- LCD 通过板载 TFT 排母直接接 FSMC，无需手工接线。

## Claude 行为约束

- 修改 `oscilloscope.c` / `adc_sample.c` 时，**先确认当前 SPL/HAL 状态**——不要在 SPL 文件里塞 HAL 调用，反之亦然，否则会引入第二种链接错误。
- 触碰 ADC / DMA / TIM3 / 中断优先级配置时，主动提示采样链路上的竞态（`g_sample_done` 是 `volatile`，必须在 ISR 写、主循环读）和优先级反转风险。
- 涉及寄存器位操作时附带 RM0008 章节号注释（如 `/* RM0008 §11.12.3 ADC_CR2 */`）。
- 不要在 ISR（含 DMA 完成回调）里调用 `HAL_Delay` / 任何阻塞函数；`main.c` 中现有 `Delay_ms` 是忙等空转，不要扩散这种用法到驱动里。
- 不引入 `malloc` 到裸机路径；如确需动态分配，使用 `Drivers/Src/mymalloc.c` 提供的内存池。
- 命令行脚本使用 Windows 路径（反斜杠 + 引号包裹空格）；bash 内调用时按本文档"构建/烧录"段的写法处理路径。
