# CH341 Compatibility Layer for CH347

这是一个兼容层，让使用 CH341 API 的程序可以在 CH347 硬件上运行。

## 文件说明

- `ch341_compat.h` - CH341 API 头文件
- `ch341_compat.c` - 兼容层实现，调用 CH347DLL
- `ch341_compat.def` - DLL 导出定义
- `CMakeLists.txt` - CMake 构建配置
- `Makefile.mingw` - MinGW 编译用 Makefile

## 编译方法

### 使用 CMake + MSVC

```batch
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 使用 MinGW

```batch
mingw32-make -f Makefile.mingw
```

### 使用 MSVC 命令行

```batch
cl /LD /O2 ch341_compat.c /Fe:CH341DLL.dll /link /DEF:ch341_compat.def kernel32.lib
```

## 使用方法

1. 编译生成 `CH341DLL.dll`
2. 确保系统中有 `CH347DLL.DLL`（WCH 官方驱动自带）
3. 将 `CH341DLL.dll` 替换原来的 CH341 DLL，或者放到程序目录

## API 映射

| CH341 函数 | CH347 对应 | 状态 |
|-----------|----------|------|
| CH341OpenDevice | CH347OpenDevice | ✅ 完全支持 |
| CH341CloseDevice | CH347CloseDevice | ✅ 完全支持 |
| CH341SetTimeout | CH347SetTimeout | ✅ 完全支持 |
| CH341StreamI2C | CH347StreamI2C | ✅ 完全支持 |
| CH341ReadI2C | CH347StreamI2C封装 | ✅ 完全支持 |
| CH341WriteI2C | CH347StreamI2C封装 | ✅ 完全支持 |
| CH341SetStream | CH347I2C_Set | ✅ 完全支持 |
| CH341ReadEEPROM | CH347ReadEEPROM | ✅ 完全支持 |
| CH341WriteEEPROM | CH347WriteEEPROM | ✅ 完全支持 |
| CH341StreamSPI4 | CH347StreamSPI4 | ✅ 完全支持 |
| CH341GetVerIC | CH347GetChipType | ✅ 映射支持 |
| CH341GPIO_Get/Set | CH347GPIO_Get/Set | ✅ 完全支持 |
| CH341SetIntRoutine | CH347SetIntRoutine | ⚠️ 部分支持 |
| CH341Epp*/Mem* | - | ❌ 不支持(并口模式) |
| CH341BitStreamSPI | - | ❌ 不支持(位级SPI) |
| CH341SetupSerial | CH347Uart_Init | ❌ 未实现 |

## 注意事项

1. **并口模式不支持**: CH347 不支持 EPP/MEM 并口模式，相关函数返回 FALSE
2. **位级 SPI 不支持**: `CH341BitStreamSPI` 无法模拟，返回 FALSE
3. **串口功能**: 需要单独实现，当前返回 FALSE
4. **GPIO 映射**: CH347 的 GPIO 和 CH341 有差异，已做基本映射
5. **速度差异**: CH347 是高速 USB，实际传输速度会比 CH341 快

## 测试建议

建议在正式使用前先用简单程序测试：

```c
#include "ch341_compat.h"

int main() {
    HANDLE h = CH341OpenDevice(0);
    if (h == INVALID_HANDLE_VALUE) {
        printf("Open failed\n");
        return 1;
    }

    printf("IC Version: 0x%02X\n", CH341GetVerIC(0));

    // 测试 I2C
    UCHAR data;
    if (CH341ReadI2C(0, 0x50, 0x00, &data)) {
        printf("I2C Read: 0x%02X\n", data);
    }

    CH341CloseDevice(0);
    return 0;
}
```

## License

MIT License
