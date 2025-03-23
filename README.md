# png2txt

将 PNG 图片转换为 RGBA 彩色字符画与灰度字符画的命令行工具 。

支持输出格式：纯文本 (TXT) | 网页版 (HTML) | Canvas 像素画

---

## 功能特性

- 双模式输出
  - 真彩色 RGBA 字符画
  - 灰度 ASCII 字符画
- 多格式支持
  - 文本文件输出 (TXT)
  - 带样式的 HTML 网页
  - 高精度 Canvas 渲染

---

## 使用说明

### 基本命令
```bash
./png2txt -f <输入文件> [选项]
```

### 参数选项

| 选项                   | 缩写 | 必选 | 说明                      |
| :--------------------- | :--- | :--- | :------------------------ |
| `--help`               | `-h` |      | 显示帮助信息              |
| `--filename <文件>`    | `-f` | 是   | 输入 PNG 文件路径         |
| `--output_path <路径>` | `-o` |      | 输出目录 (默认: 当前目录) |
| `--txt`                |      |      | 生成 TXT 文件             |
| `--html`               |      |      | 生成 HTML 文件            |
| `--canvas`             |      |      | 生成 Canvas 版本          |

------

## 使用示例

### 生成所有格式

```bash
png2txt -f input.png --txt --html --canvas
```

### 指定输出目录

```bash
png2txt -f input.png -o ./output/ --canvas
```

------

## 构建要求

### 依赖项

- C++20 编译器 (GCC 11+/Clang 14+/MSVC 2022+)
- libpng 1.6+
- CMake 3.20+

------

## 贡献指南

欢迎通过 Issues 提交问题报告，或通过 Pull Requests 贡献代码。

------

## 开源协议

GPL-3.0
