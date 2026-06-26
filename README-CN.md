# Mini PCB 设计与布局

一套**从零构建、零外部依赖的 C 语言实现**，涵盖 PCB 设计理论、布局算法与可制造性分析。每个模块将行业标准设计规则（IPC-2221/2222/2223/6012/6013/7351）、传输线理论、热物理及原理图捕获概念转化为可运行的 C 代码，连接 EDA 工具与第一性原理理解之间的鸿沟。

## 子模块

| 子模块 | 主题 | 对应课程 |
|--------|------|----------|
| [mini-4layer-high-speed-layout](mini-4layer-high-speed-layout/) | 串扰（NEXT/FEXT）、特性/差分阻抗、PDN 设计、四层叠层结构、S 参数、过孔建模 | MIT 6.776, Stanford EE273, Bogatin SI |
| [mini-component-placement-strategy](mini-component-placement-strategy/) | 多目标布局优化、模拟退火、力导向布局、热感知布局、DRC 约束 | CMU 15-462, MIT 6.002 |
| [mini-flex-rigid-flex-design](mini-flex-rigid-flex-design/) | 弯曲力学、IPC-2223/6013 设计规则、柔性材料、刚柔过渡区、柔性信号完整性、柔性叠层、柔性热分析 | IPC-2223, IPC-6013 |
| [mini-gerber-generation-dfm-review](mini-gerber-generation-dfm-review/) | Gerber RS-274X/X2 生成、Excellon 钻孔格式、DFM 规则检查、PCB 几何图元 | Berkeley EE117, TU Munich 高频工程 |
| [mini-pcb-design-for-manufacturing](mini-pcb-design-for-manufacturing/) | DFM 设计规则、成本建模、拼板/二维装箱、热力 DFM、良率模型（Poisson/Murphy/Seeds） | IPC-2221, IPC-2222, IPC-6012, IPC-7351 |
| [mini-pcb-stackup-impedance-routing](mini-pcb-stackup-impedance-routing/) | PCB 叠层设计、Wheeler/Wadell 阻抗公式、传输线理论、信号完整性指标、布线规则、过孔设计 | MIT 6.002, Bogatin SI, Wheeler 1965 |
| [mini-pcb-thermal-management-layout](mini-pcb-thermal-management-layout/) | 稳态/瞬态热分析、二维有限差分法仿真、热过孔优化、材料数据库、铜皮散热面积计算 | MIT 6.630, Berkeley EE105, Stanford EE359 |
| [mini-schematic-design-kicad](mini-schematic-design-kicad/) | 原理图核心数据结构、S 表达式解析器、ERC 电气规则检查、网表导出（SPICE/IPC-D-356/EDIF）、连通性图分析、BOM 生成 | MIT 6.002, Berkeley EE16A/B, Stanford EE272 |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅依赖 `libc` 和 `libm`
- **模块自包含** — 每个目录独立拥有 `include/`、`src/`、`examples/`、`demos/`、`tests/`
- **标准驱动** — 每个模块均映射到 IPC 标准（IPC-2221/2222/2223/6012/6013/7351）与教材理论
- **实践演示** — 阻抗计算器、DFM 检查器、布局优化器、网表导出器等

## 构建

每个模块相互独立。进入模块目录并运行：

```bash
cd mini-4layer-high-speed-layout
make all    # 构建全部目标
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-pcb-design-layout/
├── mini-4layer-high-speed-layout/       # 高速布局：串扰、阻抗、PDN、叠层、S参数、过孔
├── mini-component-placement-strategy/   # 多目标元器件布局优化
├── mini-flex-rigid-flex-design/         # 柔性与刚柔结合板：弯曲、材料、过渡区
├── mini-gerber-generation-dfm-review/   # Gerber RS-274X/X2、Excellon、DFM 检查
├── mini-pcb-design-for-manufacturing/   # DFM 规则、成本、拼板、良率模型
├── mini-pcb-stackup-impedance-routing/  # 叠层结构、阻抗控制、传输线、布线规则
├── mini-pcb-thermal-management-layout/  # 稳态/瞬态热分析、有限差分法仿真
└── mini-schematic-design-kicad/         # 原理图捕获、ERC、网表、BOM 生成
```

## 许可证

MIT
