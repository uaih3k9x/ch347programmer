/**
 * CH341 Compatibility Layer for CH347 - Implementation
 *
 * This library provides CH341 API compatibility by wrapping CH347 DLL calls.
 * Allows existing CH341-based applications to work with CH347 hardware.
 *
 * Copyright (C) 2024
 */

#include "ch341_compat.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// CH347 DLL Function Pointers (loaded dynamically)
// ============================================================================

// Device management
typedef HANDLE  (WINAPI *pCH347OpenDevice)(ULONG DevI);
typedef BOOL    (WINAPI *pCH347CloseDevice)(ULONG iIndex);
typedef BOOL    (WINAPI *pCH347GetVersion)(ULONG iIndex, PUCHAR iDriverVer, PUCHAR iDLLVer, PUCHAR ibcdDevice, PUCHAR iChipType);
typedef UCHAR   (WINAPI *pCH347GetChipType)(ULONG iIndex);
typedef BOOL    (WINAPI *pCH347SetTimeout)(ULONG iIndex, ULONG iWriteTimeout, ULONG iReadTimeout);
typedef BOOL    (WINAPI *pCH347GetDeviceInfor)(ULONG iIndex, void *DevInformation);
typedef BOOL    (WINAPI *pCH347SetDeviceNotify)(ULONG iIndex, PCHAR iDeviceID, void *iNotifyRoutine);

// Data transfer
typedef BOOL    (WINAPI *pCH347ReadData)(ULONG iIndex, PVOID oBuffer, PULONG ioLength);
typedef BOOL    (WINAPI *pCH347WriteData)(ULONG iIndex, PVOID iBuffer, PULONG ioLength);

// I2C
typedef BOOL    (WINAPI *pCH347I2C_Set)(ULONG iIndex, ULONG iMode);
typedef BOOL    (WINAPI *pCH347I2C_SetDelaymS)(ULONG iIndex, ULONG iDelay);
typedef BOOL    (WINAPI *pCH347StreamI2C)(ULONG iIndex, ULONG iWriteLength, PVOID iWriteBuffer, ULONG iReadLength, PVOID oReadBuffer);
typedef BOOL    (WINAPI *pCH347ReadEEPROM)(ULONG iIndex, EEPROM_TYPE iEepromID, ULONG iAddr, ULONG iLength, PUCHAR oBuffer);
typedef BOOL    (WINAPI *pCH347WriteEEPROM)(ULONG iIndex, EEPROM_TYPE iEepromID, ULONG iAddr, ULONG iLength, PUCHAR iBuffer);

// SPI
typedef BOOL    (WINAPI *pCH347SPI_Init)(ULONG iIndex, void *SpiCfg);
typedef BOOL    (WINAPI *pCH347SPI_SetFrequency)(ULONG iIndex, ULONG iSpiSpeedHz);
typedef BOOL    (WINAPI *pCH347SPI_GetCfg)(ULONG iIndex, void *SpiCfg);
typedef BOOL    (WINAPI *pCH347SPI_WriteRead)(ULONG iIndex, ULONG iChipSelect, ULONG iLength, PVOID ioBuffer);
typedef BOOL    (WINAPI *pCH347StreamSPI4)(ULONG iIndex, ULONG iChipSelect, ULONG iLength, PVOID ioBuffer);

// GPIO
typedef BOOL    (WINAPI *pCH347GPIO_Get)(ULONG iIndex, UCHAR *iDir, UCHAR *iData);
typedef BOOL    (WINAPI *pCH347GPIO_Set)(ULONG iIndex, UCHAR iEnable, UCHAR iSetDirOut, UCHAR iSetDataOut);

// Interrupt
typedef BOOL    (WINAPI *pCH347SetIntRoutine)(ULONG iIndex, UCHAR Int0PinN, UCHAR Int0TripMode, UCHAR Int1PinN, UCHAR Int1TripMode, void *iIntRoutine);
typedef BOOL    (WINAPI *pCH347ReadInter)(ULONG iIndex, PUCHAR iStatus);
typedef BOOL    (WINAPI *pCH347AbortInter)(ULONG iIndex);

// ============================================================================
// Global State
// ============================================================================

static HMODULE g_hCH347DLL = NULL;
static BOOL g_bInitialized = FALSE;
static HANDLE g_DeviceHandles[mCH341_MAX_NUMBER] = {0};
static char g_DeviceNames[mCH341_MAX_NUMBER][MAX_DEVICE_PATH_SIZE] = {0};

// Function pointers
static pCH347OpenDevice         fp_CH347OpenDevice = NULL;
static pCH347CloseDevice        fp_CH347CloseDevice = NULL;
static pCH347GetVersion         fp_CH347GetVersion = NULL;
static pCH347GetChipType        fp_CH347GetChipType = NULL;
static pCH347SetTimeout         fp_CH347SetTimeout = NULL;
static pCH347GetDeviceInfor     fp_CH347GetDeviceInfor = NULL;
static pCH347SetDeviceNotify    fp_CH347SetDeviceNotify = NULL;
static pCH347ReadData           fp_CH347ReadData = NULL;
static pCH347WriteData          fp_CH347WriteData = NULL;
static pCH347I2C_Set            fp_CH347I2C_Set = NULL;
static pCH347I2C_SetDelaymS     fp_CH347I2C_SetDelaymS = NULL;
static pCH347StreamI2C          fp_CH347StreamI2C = NULL;
static pCH347ReadEEPROM         fp_CH347ReadEEPROM = NULL;
static pCH347WriteEEPROM        fp_CH347WriteEEPROM = NULL;
static pCH347SPI_Init           fp_CH347SPI_Init = NULL;
static pCH347SPI_SetFrequency   fp_CH347SPI_SetFrequency = NULL;
static pCH347SPI_GetCfg         fp_CH347SPI_GetCfg = NULL;
static pCH347SPI_WriteRead      fp_CH347SPI_WriteRead = NULL;
static pCH347StreamSPI4         fp_CH347StreamSPI4 = NULL;
static pCH347GPIO_Get           fp_CH347GPIO_Get = NULL;
static pCH347GPIO_Set           fp_CH347GPIO_Set = NULL;
static pCH347SetIntRoutine      fp_CH347SetIntRoutine = NULL;
static pCH347ReadInter          fp_CH347ReadInter = NULL;
static pCH347AbortInter         fp_CH347AbortInter = NULL;

// Stream mode settings
static ULONG g_StreamMode[mCH341_MAX_NUMBER] = {0};  // Store I2C/SPI mode per device
static BOOL g_SPIInitialized[mCH341_MAX_NUMBER] = {FALSE};

// ============================================================================
// Internal Functions
// ============================================================================

static BOOL LoadCH347DLL(void)
{
    if (g_bInitialized) return TRUE;

    // Try to load CH347DLL.DLL
    g_hCH347DLL = LoadLibraryA("CH347DLL.DLL");
    if (!g_hCH347DLL) {
        // Try alternate locations
        g_hCH347DLL = LoadLibraryA("CH347DLLA64.DLL");
    }
    if (!g_hCH347DLL) {
        return FALSE;
    }

    // Load function pointers
    fp_CH347OpenDevice      = (pCH347OpenDevice)GetProcAddress(g_hCH347DLL, "CH347OpenDevice");
    fp_CH347CloseDevice     = (pCH347CloseDevice)GetProcAddress(g_hCH347DLL, "CH347CloseDevice");
    fp_CH347GetVersion      = (pCH347GetVersion)GetProcAddress(g_hCH347DLL, "CH347GetVersion");
    fp_CH347GetChipType     = (pCH347GetChipType)GetProcAddress(g_hCH347DLL, "CH347GetChipType");
    fp_CH347SetTimeout      = (pCH347SetTimeout)GetProcAddress(g_hCH347DLL, "CH347SetTimeout");
    fp_CH347GetDeviceInfor  = (pCH347GetDeviceInfor)GetProcAddress(g_hCH347DLL, "CH347GetDeviceInfor");
    fp_CH347SetDeviceNotify = (pCH347SetDeviceNotify)GetProcAddress(g_hCH347DLL, "CH347SetDeviceNotify");
    fp_CH347ReadData        = (pCH347ReadData)GetProcAddress(g_hCH347DLL, "CH347ReadData");
    fp_CH347WriteData       = (pCH347WriteData)GetProcAddress(g_hCH347DLL, "CH347WriteData");
    fp_CH347I2C_Set         = (pCH347I2C_Set)GetProcAddress(g_hCH347DLL, "CH347I2C_Set");
    fp_CH347I2C_SetDelaymS  = (pCH347I2C_SetDelaymS)GetProcAddress(g_hCH347DLL, "CH347I2C_SetDelaymS");
    fp_CH347StreamI2C       = (pCH347StreamI2C)GetProcAddress(g_hCH347DLL, "CH347StreamI2C");
    fp_CH347ReadEEPROM      = (pCH347ReadEEPROM)GetProcAddress(g_hCH347DLL, "CH347ReadEEPROM");
    fp_CH347WriteEEPROM     = (pCH347WriteEEPROM)GetProcAddress(g_hCH347DLL, "CH347WriteEEPROM");
    fp_CH347SPI_Init        = (pCH347SPI_Init)GetProcAddress(g_hCH347DLL, "CH347SPI_Init");
    fp_CH347SPI_SetFrequency= (pCH347SPI_SetFrequency)GetProcAddress(g_hCH347DLL, "CH347SPI_SetFrequency");
    fp_CH347SPI_GetCfg      = (pCH347SPI_GetCfg)GetProcAddress(g_hCH347DLL, "CH347SPI_GetCfg");
    fp_CH347SPI_WriteRead   = (pCH347SPI_WriteRead)GetProcAddress(g_hCH347DLL, "CH347SPI_WriteRead");
    fp_CH347StreamSPI4      = (pCH347StreamSPI4)GetProcAddress(g_hCH347DLL, "CH347StreamSPI4");
    fp_CH347GPIO_Get        = (pCH347GPIO_Get)GetProcAddress(g_hCH347DLL, "CH347GPIO_Get");
    fp_CH347GPIO_Set        = (pCH347GPIO_Set)GetProcAddress(g_hCH347DLL, "CH347GPIO_Set");
    fp_CH347SetIntRoutine   = (pCH347SetIntRoutine)GetProcAddress(g_hCH347DLL, "CH347SetIntRoutine");
    fp_CH347ReadInter       = (pCH347ReadInter)GetProcAddress(g_hCH347DLL, "CH347ReadInter");
    fp_CH347AbortInter      = (pCH347AbortInter)GetProcAddress(g_hCH347DLL, "CH347AbortInter");

    // Check essential functions
    if (!fp_CH347OpenDevice || !fp_CH347CloseDevice) {
        FreeLibrary(g_hCH347DLL);
        g_hCH347DLL = NULL;
        return FALSE;
    }

    g_bInitialized = TRUE;
    return TRUE;
}

// Initialize SPI with default CH341-compatible settings
static BOOL InitSPIForCH341(ULONG iIndex, ULONG iChipSelect)
{
    if (!fp_CH347SPI_Init) return FALSE;

    // SPI config structure matching CH347
    #pragma pack(1)
    typedef struct {
        UCHAR  iMode;                  // SPI Mode 0-3
        UCHAR  iClock;                 // Clock speed
        UCHAR  iByteOrder;             // 0=LSB, 1=MSB
        USHORT iSpiWriteReadInterval;  // R/W interval in us
        UCHAR  iSpiOutDefaultData;     // Default output data
        ULONG  iChipSelect;            // Chip select
        UCHAR  CS1Polarity;            // CS1 polarity
        UCHAR  CS2Polarity;            // CS2 polarity
        USHORT iIsAutoDeativeCS;       // Auto deactivate CS
        USHORT iActiveDelay;           // Active delay in us
        ULONG  iDelayDeactive;         // Deactive delay in us
    } SpiCfgS;
    #pragma pack()

    SpiCfgS cfg = {0};
    cfg.iMode = 0;                      // Mode 0
    cfg.iClock = 1;                     // 30MHz (close to CH341's speed)
    cfg.iByteOrder = 1;                 // MSB first (like CH341)
    cfg.iSpiWriteReadInterval = 0;
    cfg.iSpiOutDefaultData = 0xFF;
    cfg.iChipSelect = iChipSelect;
    cfg.CS1Polarity = 0;                // Active low
    cfg.CS2Polarity = 0;
    cfg.iIsAutoDeativeCS = 1;           // Auto deactivate
    cfg.iActiveDelay = 0;
    cfg.iDelayDeactive = 0;

    return fp_CH347SPI_Init(iIndex, &cfg);
}

// ============================================================================
// Device Management Functions
// ============================================================================

HANDLE WINAPI CH341OpenDevice(ULONG iIndex)
{
    if (!LoadCH347DLL()) return INVALID_HANDLE_VALUE;
    if (iIndex >= mCH341_MAX_NUMBER) return INVALID_HANDLE_VALUE;
    if (!fp_CH347OpenDevice) return INVALID_HANDLE_VALUE;

    HANDLE h = fp_CH347OpenDevice(iIndex);
    if (h != INVALID_HANDLE_VALUE && h != NULL) {
        g_DeviceHandles[iIndex] = h;
        snprintf(g_DeviceNames[iIndex], MAX_DEVICE_PATH_SIZE, "CH347_%lu", iIndex);
    }
    return h;
}

VOID WINAPI CH341CloseDevice(ULONG iIndex)
{
    if (iIndex >= mCH341_MAX_NUMBER) return;
    if (!fp_CH347CloseDevice) return;

    fp_CH347CloseDevice(iIndex);
    g_DeviceHandles[iIndex] = NULL;
    g_SPIInitialized[iIndex] = FALSE;
}

ULONG WINAPI CH341GetVersion(void)
{
    // Return compatibility layer version
    // Format: major.minor.patch as BCD: 0x0210 = v2.1.0
    return 0x0210;
}

ULONG WINAPI CH341GetDrvVersion(void)
{
    if (!LoadCH347DLL()) return 0;
    if (!fp_CH347GetVersion) return 0;

    UCHAR driverVer = 0, dllVer = 0, bcdDevice = 0, chipType = 0;
    // Try with device 0, if fails return a default
    if (fp_CH347GetVersion(0, &driverVer, &dllVer, &bcdDevice, &chipType)) {
        return (ULONG)driverVer;
    }
    return 0x0350;  // Default driver version
}

BOOL WINAPI CH341ResetDevice(ULONG iIndex)
{
    // CH347 doesn't have a direct reset function
    // Close and reopen as workaround
    if (iIndex >= mCH341_MAX_NUMBER) return FALSE;

    CH341CloseDevice(iIndex);
    HANDLE h = CH341OpenDevice(iIndex);
    return (h != INVALID_HANDLE_VALUE);
}

BOOL WINAPI CH341GetDeviceDescr(ULONG iIndex, PVOID oBuffer, PULONG ioLength)
{
    // CH347 doesn't expose this directly, return stub data
    if (!oBuffer || !ioLength) return FALSE;
    if (*ioLength < 18) return FALSE;

    // Standard USB device descriptor (18 bytes)
    UCHAR desc[18] = {
        18,         // bLength
        0x01,       // bDescriptorType (Device)
        0x00, 0x02, // bcdUSB (2.0)
        0xFF,       // bDeviceClass (Vendor specific)
        0x00,       // bDeviceSubClass
        0x00,       // bDeviceProtocol
        0x40,       // bMaxPacketSize0
        0x86, 0x1A, // idVendor (WCH)
        0x55, 0x55, // idProduct (CH347)
        0x00, 0x03, // bcdDevice
        0x01,       // iManufacturer
        0x02,       // iProduct
        0x00,       // iSerialNumber
        0x01        // bNumConfigurations
    };
    memcpy(oBuffer, desc, 18);
    *ioLength = 18;
    return TRUE;
}

BOOL WINAPI CH341GetConfigDescr(ULONG iIndex, PVOID oBuffer, PULONG ioLength)
{
    // CH347 doesn't expose this directly, return stub data
    if (!oBuffer || !ioLength) return FALSE;
    if (*ioLength < 9) return FALSE;

    UCHAR desc[9] = {
        9,          // bLength
        0x02,       // bDescriptorType (Configuration)
        32, 0,      // wTotalLength
        0x01,       // bNumInterfaces
        0x01,       // bConfigurationValue
        0x00,       // iConfiguration
        0x80,       // bmAttributes
        250         // bMaxPower (500mA)
    };
    memcpy(oBuffer, desc, 9);
    *ioLength = 9;
    return TRUE;
}

PVOID WINAPI CH341GetDeviceName(ULONG iIndex)
{
    if (iIndex >= mCH341_MAX_NUMBER) return NULL;
    if (g_DeviceHandles[iIndex] == NULL) return NULL;
    return g_DeviceNames[iIndex];
}

ULONG WINAPI CH341GetVerIC(ULONG iIndex)
{
    if (!LoadCH347DLL()) return 0;
    if (!fp_CH347GetChipType) return IC_VER_CH341A;  // Default

    UCHAR chipType = fp_CH347GetChipType(iIndex);
    // Map CH347 chip types to CH341 style return values
    // CH347 returns: 0=CH341, 1=CH347T, 2=CH347F, 3=CH339W
    // CH341 expects: 0x20=CH341A, 0x30=CH341A3
    switch (chipType) {
        case 0:  return IC_VER_CH341A;       // CH341
        case 1:  return IC_VER_CH341A3;      // CH347T -> report as CH341A3
        case 2:  return IC_VER_CH341A3;      // CH347F
        case 3:  return IC_VER_CH341A3;      // CH339W
        default: return IC_VER_CH341A;
    }
}

BOOL WINAPI CH341SetExclusive(ULONG iIndex, ULONG iExclusive)
{
    // CH347 doesn't have exclusive mode, always succeed
    (void)iIndex;
    (void)iExclusive;
    return TRUE;
}

BOOL WINAPI CH341SetTimeout(ULONG iIndex, ULONG iWriteTimeout, ULONG iReadTimeout)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347SetTimeout) return FALSE;
    return fp_CH347SetTimeout(iIndex, iWriteTimeout, iReadTimeout);
}

BOOL WINAPI CH341FlushBuffer(ULONG iIndex)
{
    // CH347 doesn't have explicit buffer flush
    // Read any pending data as workaround
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347ReadData) return TRUE;  // No read function, just succeed

    UCHAR buf[512];
    ULONG len = sizeof(buf);
    fp_CH347ReadData(iIndex, buf, &len);
    return TRUE;
}

ULONG WINAPI CH341DriverCommand(ULONG iIndex, mPWIN32_COMMAND ioCommand)
{
    // This is a low-level function that's hard to emulate
    // Return 0 to indicate not supported
    (void)iIndex;
    (void)ioCommand;
    return 0;
}

// ============================================================================
// I2C Functions
// ============================================================================

BOOL WINAPI CH341SetStream(ULONG iIndex, ULONG iMode)
{
    if (!LoadCH347DLL()) return FALSE;
    if (iIndex >= mCH341_MAX_NUMBER) return FALSE;

    g_StreamMode[iIndex] = iMode;

    // Extract I2C speed from mode bits 1-0
    // CH341: 00=20KHz, 01=100KHz, 10=400KHz, 11=750KHz
    // CH347: 000=20KHz, 001=100KHz, 010=400KHz, 011=750KHz, 100=50KHz, 101=200KHz, 110=1MHz
    ULONG i2cSpeed = iMode & 0x03;

    if (fp_CH347I2C_Set) {
        return fp_CH347I2C_Set(iIndex, i2cSpeed);
    }
    return TRUE;
}

BOOL WINAPI CH341SetDelaymS(ULONG iIndex, ULONG iDelay)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347I2C_SetDelaymS) return FALSE;
    return fp_CH347I2C_SetDelaymS(iIndex, iDelay);
}

BOOL WINAPI CH341StreamI2C(ULONG iIndex, ULONG iWriteLength, PVOID iWriteBuffer,
                           ULONG iReadLength, PVOID oReadBuffer)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347StreamI2C) return FALSE;
    return fp_CH347StreamI2C(iIndex, iWriteLength, iWriteBuffer, iReadLength, oReadBuffer);
}

BOOL WINAPI CH341ReadI2C(ULONG iIndex, UCHAR iDevice, UCHAR iAddr, PUCHAR oByte)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347StreamI2C) return FALSE;
    if (!oByte) return FALSE;

    // Build I2C write buffer: device address (write) + register address
    UCHAR writeBuf[2];
    writeBuf[0] = (iDevice << 1);  // Device address with write bit (bit 0 = 0)
    writeBuf[1] = iAddr;           // Register address

    // Use StreamI2C: write 2 bytes (device+addr), read 1 byte
    return fp_CH347StreamI2C(iIndex, 2, writeBuf, 1, oByte);
}

BOOL WINAPI CH341WriteI2C(ULONG iIndex, UCHAR iDevice, UCHAR iAddr, UCHAR iByte)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347StreamI2C) return FALSE;

    // Build I2C write buffer: device address (write) + register address + data
    UCHAR writeBuf[3];
    writeBuf[0] = (iDevice << 1);  // Device address with write bit (bit 0 = 0)
    writeBuf[1] = iAddr;           // Register address
    writeBuf[2] = iByte;           // Data to write

    // Use StreamI2C: write 3 bytes, read 0 bytes
    return fp_CH347StreamI2C(iIndex, 3, writeBuf, 0, NULL);
}

// ============================================================================
// EEPROM Functions
// ============================================================================

BOOL WINAPI CH341ReadEEPROM(ULONG iIndex, EEPROM_TYPE iEepromID,
                            ULONG iAddr, ULONG iLength, PUCHAR oBuffer)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347ReadEEPROM) return FALSE;
    return fp_CH347ReadEEPROM(iIndex, iEepromID, iAddr, iLength, oBuffer);
}

BOOL WINAPI CH341WriteEEPROM(ULONG iIndex, EEPROM_TYPE iEepromID,
                             ULONG iAddr, ULONG iLength, PUCHAR iBuffer)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347WriteEEPROM) return FALSE;
    return fp_CH347WriteEEPROM(iIndex, iEepromID, iAddr, iLength, iBuffer);
}

// ============================================================================
// SPI Functions
// ============================================================================

BOOL WINAPI CH341StreamSPI4(ULONG iIndex, ULONG iChipSelect,
                            ULONG iLength, PVOID ioBuffer)
{
    if (!LoadCH347DLL()) return FALSE;
    if (iIndex >= mCH341_MAX_NUMBER) return FALSE;

    // Initialize SPI if not done
    if (!g_SPIInitialized[iIndex]) {
        if (!InitSPIForCH341(iIndex, iChipSelect)) {
            return FALSE;
        }
        g_SPIInitialized[iIndex] = TRUE;
    }

    // Prefer CH347StreamSPI4 if available, otherwise use CH347SPI_WriteRead
    if (fp_CH347StreamSPI4) {
        return fp_CH347StreamSPI4(iIndex, iChipSelect, iLength, ioBuffer);
    } else if (fp_CH347SPI_WriteRead) {
        return fp_CH347SPI_WriteRead(iIndex, iChipSelect, iLength, ioBuffer);
    }
    return FALSE;
}

BOOL WINAPI CH341StreamSPI5(ULONG iIndex, ULONG iChipSelect,
                            ULONG iLength, PVOID ioBuffer, PVOID ioBuffer2)
{
    // CH347 doesn't support dual-channel SPI in the same way
    // Fall back to single channel operation
    (void)ioBuffer2;  // Ignore second buffer
    return CH341StreamSPI4(iIndex, iChipSelect, iLength, ioBuffer);
}

BOOL WINAPI CH341BitStreamSPI(ULONG iIndex, ULONG iLength, PVOID ioBuffer)
{
    // CH347 doesn't support bit-level SPI control
    // This is a significant limitation
    (void)iIndex;
    (void)iLength;
    (void)ioBuffer;
    return FALSE;
}

BOOL WINAPI CH341StreamSPI3(ULONG iIndex, ULONG iChipSelect,
                            ULONG iLength, PVOID ioBuffer)
{
    // Deprecated, redirect to SPI4
    return CH341StreamSPI4(iIndex, iChipSelect, iLength, ioBuffer);
}

// ============================================================================
// GPIO Functions
// ============================================================================

BOOL WINAPI CH341GetInput(ULONG iIndex, PULONG iStatus)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347GPIO_Get) return FALSE;
    if (!iStatus) return FALSE;

    UCHAR dir = 0, data = 0;
    if (!fp_CH347GPIO_Get(iIndex, &dir, &data)) {
        return FALSE;
    }

    // Map CH347 GPIO to CH341 status format
    // CH341 status bits: D7-D0 in bits 7-0, status pins in bits 8-11, etc.
    *iStatus = (ULONG)data;  // GPIO0-7 in D7-D0

    return TRUE;
}

BOOL WINAPI CH341GetStatus(ULONG iIndex, PULONG iStatus)
{
    return CH341GetInput(iIndex, iStatus);
}

BOOL WINAPI CH341SetOutput(ULONG iIndex, ULONG iEnable,
                           ULONG iSetDirOut, ULONG iSetDataOut)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347GPIO_Set) return FALSE;

    // Map CH341 output settings to CH347
    // CH341 uses complex bit mapping, CH347 uses simple GPIO0-7
    UCHAR enable = 0xFF;  // Enable all GPIOs
    UCHAR dirOut = 0;
    UCHAR dataOut = 0;

    // Extract D7-D0 direction and data from CH341 format
    if (iEnable & 0x04) {  // Bits 7-0 data valid
        dataOut = (UCHAR)(iSetDataOut & 0xFF);
    }
    if (iEnable & 0x08) {  // Bits 7-0 direction valid
        dirOut = (UCHAR)(iSetDirOut & 0xFF);
    }

    return fp_CH347GPIO_Set(iIndex, enable, dirOut, dataOut);
}

BOOL WINAPI CH341Set_D5_D0(ULONG iIndex, ULONG iSetDirOut, ULONG iSetDataOut)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347GPIO_Set) return FALSE;

    // Set D5-D0 pins only
    UCHAR enable = 0x3F;  // GPIO0-5 enabled
    UCHAR dirOut = (UCHAR)(iSetDirOut & 0x3F);
    UCHAR dataOut = (UCHAR)(iSetDataOut & 0x3F);

    return fp_CH347GPIO_Set(iIndex, enable, dirOut, dataOut);
}

// ============================================================================
// Data Transfer Functions
// ============================================================================

BOOL WINAPI CH341ReadData(ULONG iIndex, PVOID oBuffer, PULONG ioLength)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347ReadData) return FALSE;
    return fp_CH347ReadData(iIndex, oBuffer, ioLength);
}

BOOL WINAPI CH341WriteData(ULONG iIndex, PVOID iBuffer, PULONG ioLength)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347WriteData) return FALSE;
    return fp_CH347WriteData(iIndex, iBuffer, ioLength);
}

BOOL WINAPI CH341ReadData0(ULONG iIndex, PVOID oBuffer, PULONG ioLength)
{
    // CH347 doesn't distinguish ports, use regular read
    return CH341ReadData(iIndex, oBuffer, ioLength);
}

BOOL WINAPI CH341ReadData1(ULONG iIndex, PVOID oBuffer, PULONG ioLength)
{
    return CH341ReadData(iIndex, oBuffer, ioLength);
}

BOOL WINAPI CH341WriteData0(ULONG iIndex, PVOID iBuffer, PULONG ioLength)
{
    return CH341WriteData(iIndex, iBuffer, ioLength);
}

BOOL WINAPI CH341WriteData1(ULONG iIndex, PVOID iBuffer, PULONG ioLength)
{
    return CH341WriteData(iIndex, iBuffer, ioLength);
}

BOOL WINAPI CH341WriteRead(ULONG iIndex, ULONG iWriteLength, PVOID iWriteBuffer,
                           ULONG iReadStep, ULONG iReadTimes,
                           PULONG oReadLength, PVOID oReadBuffer)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347WriteData || !fp_CH347ReadData) return FALSE;

    // Write first
    ULONG writeLen = iWriteLength;
    if (iWriteLength > 0) {
        if (!fp_CH347WriteData(iIndex, iWriteBuffer, &writeLen)) {
            return FALSE;
        }
    }

    // Then read
    if (oReadLength && oReadBuffer && iReadStep > 0 && iReadTimes > 0) {
        ULONG totalRead = 0;
        PUCHAR pBuf = (PUCHAR)oReadBuffer;

        for (ULONG i = 0; i < iReadTimes; i++) {
            ULONG readLen = iReadStep;
            if (!fp_CH347ReadData(iIndex, pBuf, &readLen)) {
                break;
            }
            pBuf += readLen;
            totalRead += readLen;
        }
        *oReadLength = totalRead;
    }

    return TRUE;
}

// ============================================================================
// Parallel Port Functions (Stubs - CH347 doesn't support parallel port mode)
// ============================================================================

BOOL WINAPI CH341SetParaMode(ULONG iIndex, ULONG iMode)
{
    (void)iIndex;
    (void)iMode;
    return FALSE;  // Not supported
}

BOOL WINAPI CH341InitParallel(ULONG iIndex, ULONG iMode)
{
    (void)iIndex;
    (void)iMode;
    return FALSE;  // Not supported
}

BOOL WINAPI CH341EppReadData(ULONG iIndex, PVOID oBuffer, PULONG ioLength)
{
    (void)iIndex;
    (void)oBuffer;
    (void)ioLength;
    return FALSE;
}

BOOL WINAPI CH341EppReadAddr(ULONG iIndex, PVOID oBuffer, PULONG ioLength)
{
    (void)iIndex;
    (void)oBuffer;
    (void)ioLength;
    return FALSE;
}

BOOL WINAPI CH341EppWriteData(ULONG iIndex, PVOID iBuffer, PULONG ioLength)
{
    (void)iIndex;
    (void)iBuffer;
    (void)ioLength;
    return FALSE;
}

BOOL WINAPI CH341EppWriteAddr(ULONG iIndex, PVOID iBuffer, PULONG ioLength)
{
    (void)iIndex;
    (void)iBuffer;
    (void)ioLength;
    return FALSE;
}

BOOL WINAPI CH341EppSetAddr(ULONG iIndex, UCHAR iAddr)
{
    (void)iIndex;
    (void)iAddr;
    return FALSE;
}

BOOL WINAPI CH341MemReadAddr0(ULONG iIndex, PVOID oBuffer, PULONG ioLength)
{
    (void)iIndex;
    (void)oBuffer;
    (void)ioLength;
    return FALSE;
}

BOOL WINAPI CH341MemReadAddr1(ULONG iIndex, PVOID oBuffer, PULONG ioLength)
{
    (void)iIndex;
    (void)oBuffer;
    (void)ioLength;
    return FALSE;
}

BOOL WINAPI CH341MemWriteAddr0(ULONG iIndex, PVOID iBuffer, PULONG ioLength)
{
    (void)iIndex;
    (void)iBuffer;
    (void)ioLength;
    return FALSE;
}

BOOL WINAPI CH341MemWriteAddr1(ULONG iIndex, PVOID iBuffer, PULONG ioLength)
{
    (void)iIndex;
    (void)iBuffer;
    (void)ioLength;
    return FALSE;
}

// ============================================================================
// Interrupt Functions
// ============================================================================

// Store user's interrupt callback
static mPCH341_INT_ROUTINE g_IntRoutines[mCH341_MAX_NUMBER] = {NULL};

// Wrapper callback for CH347 interrupt format
static VOID CALLBACK CH347IntWrapper(PUCHAR iStatus)
{
    // CH347 provides 8 bytes of GPIO status, convert to CH341 format
    for (ULONG i = 0; i < mCH341_MAX_NUMBER; i++) {
        if (g_IntRoutines[i]) {
            // Extract status and convert to CH341 format
            ULONG status = 0;
            if (iStatus) {
                status = (ULONG)iStatus[0];  // Basic GPIO status
            }
            g_IntRoutines[i](status);
        }
    }
}

BOOL WINAPI CH341SetIntRoutine(ULONG iIndex, mPCH341_INT_ROUTINE iIntRoutine)
{
    if (iIndex >= mCH341_MAX_NUMBER) return FALSE;
    if (!LoadCH347DLL()) return FALSE;

    g_IntRoutines[iIndex] = iIntRoutine;

    if (!fp_CH347SetIntRoutine) return FALSE;

    if (iIntRoutine) {
        // Set up CH347 interrupt with GPIO0 falling edge trigger
        return fp_CH347SetIntRoutine(iIndex, 0, 0, 255, 0, CH347IntWrapper);
    } else {
        // Disable interrupt
        return fp_CH347SetIntRoutine(iIndex, 255, 0, 255, 0, NULL);
    }
}

BOOL WINAPI CH341ReadInter(ULONG iIndex, PULONG iStatus)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347ReadInter) return FALSE;
    if (!iStatus) return FALSE;

    UCHAR status[8] = {0};
    if (!fp_CH347ReadInter(iIndex, status)) {
        return FALSE;
    }

    // Convert CH347 8-byte GPIO status to CH341 format
    *iStatus = (ULONG)status[0];
    return TRUE;
}

BOOL WINAPI CH341AbortInter(ULONG iIndex)
{
    if (!LoadCH347DLL()) return FALSE;
    if (!fp_CH347AbortInter) return FALSE;
    return fp_CH347AbortInter(iIndex);
}

BOOL WINAPI CH341ResetInter(ULONG iIndex)
{
    // Reset by aborting and re-enabling if callback was set
    if (iIndex >= mCH341_MAX_NUMBER) return FALSE;

    CH341AbortInter(iIndex);
    if (g_IntRoutines[iIndex]) {
        return CH341SetIntRoutine(iIndex, g_IntRoutines[iIndex]);
    }
    return TRUE;
}

// ============================================================================
// Abort/Reset Functions
// ============================================================================

BOOL WINAPI CH341AbortRead(ULONG iIndex)
{
    (void)iIndex;
    // No direct equivalent, just return success
    return TRUE;
}

BOOL WINAPI CH341AbortWrite(ULONG iIndex)
{
    (void)iIndex;
    return TRUE;
}

BOOL WINAPI CH341ResetRead(ULONG iIndex)
{
    return CH341AbortRead(iIndex);
}

BOOL WINAPI CH341ResetWrite(ULONG iIndex)
{
    return CH341AbortWrite(iIndex);
}

// ============================================================================
// Buffer Upload/Download Functions (Stubs)
// ============================================================================

static BOOL g_BufUploadEnabled[mCH341_MAX_NUMBER] = {FALSE};
static BOOL g_BufDownloadEnabled[mCH341_MAX_NUMBER] = {FALSE};

BOOL WINAPI CH341SetBufUpload(ULONG iIndex, ULONG iEnableOrClear)
{
    if (iIndex >= mCH341_MAX_NUMBER) return FALSE;
    g_BufUploadEnabled[iIndex] = (iEnableOrClear != 0);
    return TRUE;
}

LONG WINAPI CH341QueryBufUpload(ULONG iIndex)
{
    if (iIndex >= mCH341_MAX_NUMBER) return -1;
    if (!g_BufUploadEnabled[iIndex]) return -1;
    return 0;  // No data in buffer
}

BOOL WINAPI CH341SetBufDownload(ULONG iIndex, ULONG iEnableOrClear)
{
    if (iIndex >= mCH341_MAX_NUMBER) return FALSE;
    g_BufDownloadEnabled[iIndex] = (iEnableOrClear != 0);
    return TRUE;
}

LONG WINAPI CH341QueryBufDownload(ULONG iIndex)
{
    if (iIndex >= mCH341_MAX_NUMBER) return -1;
    if (!g_BufDownloadEnabled[iIndex]) return -1;
    return 0;
}

// ============================================================================
// Device Notification
// ============================================================================

static mPCH341_NOTIFY_ROUTINE g_NotifyRoutines[mCH341_MAX_NUMBER] = {NULL};

// Wrapper for CH347 notify callback
static VOID CALLBACK CH347NotifyWrapper(ULONG iEventStatus)
{
    // CH347 and CH341 use same event status values (0, 1, 3)
    for (ULONG i = 0; i < mCH341_MAX_NUMBER; i++) {
        if (g_NotifyRoutines[i]) {
            g_NotifyRoutines[i](iEventStatus);
        }
    }
}

BOOL WINAPI CH341SetDeviceNotify(ULONG iIndex, PCHAR iDeviceID,
                                  mPCH341_NOTIFY_ROUTINE iNotifyRoutine)
{
    if (iIndex >= mCH341_MAX_NUMBER) return FALSE;
    if (!LoadCH347DLL()) return FALSE;

    g_NotifyRoutines[iIndex] = iNotifyRoutine;

    if (!fp_CH347SetDeviceNotify) return FALSE;

    return fp_CH347SetDeviceNotify(iIndex, iDeviceID,
                                    iNotifyRoutine ? (void*)CH347NotifyWrapper : NULL);
}

// ============================================================================
// Extended Functions
// ============================================================================

HANDLE WINAPI CH341OpenDeviceEx(ULONG iIndex)
{
    return CH341OpenDevice(iIndex);
}

VOID WINAPI CH341CloseDeviceEx(ULONG iIndex)
{
    CH341CloseDevice(iIndex);
}

PCHAR WINAPI CH341GetDeviceNameEx(ULONG iIndex)
{
    return (PCHAR)CH341GetDeviceName(iIndex);
}

BOOL WINAPI CH341SetDeviceNotifyEx(ULONG iIndex, PCHAR iDeviceID,
                                    mPCH341_NOTIFY_ROUTINE iNotifyRoutine)
{
    return CH341SetDeviceNotify(iIndex, iDeviceID, iNotifyRoutine);
}

// ============================================================================
// Serial Port Functions
// ============================================================================

BOOL WINAPI CH341SetupSerial(ULONG iIndex, ULONG iParityMode, ULONG iBaudRate)
{
    // CH347 uses different UART API (CH347Uart_Init)
    // This would require loading different function pointers
    // For now, return failure as serial mode is a different use case
    (void)iIndex;
    (void)iParityMode;
    (void)iBaudRate;
    return FALSE;
}

// ============================================================================
// DLL Entry Point
// ============================================================================

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL;
    (void)lpvReserved;

    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            // Initialize on first load
            break;

        case DLL_PROCESS_DETACH:
            // Cleanup
            if (g_hCH347DLL) {
                FreeLibrary(g_hCH347DLL);
                g_hCH347DLL = NULL;
            }
            g_bInitialized = FALSE;
            break;
    }
    return TRUE;
}
#endif
