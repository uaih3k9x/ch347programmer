/**
 * CH341 Compatibility Layer for CH347
 *
 * This library provides CH341 API compatibility by wrapping CH347 DLL calls.
 * Allows existing CH341-based applications to work with CH347 hardware.
 *
 * Copyright (C) 2024
 */

#ifndef _CH341_COMPAT_H
#define _CH341_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>

// ============================================================================
// Constants
// ============================================================================

#define mCH341_PACKET_LENGTH    32      // CH341 packet length
#define mCH341_PKT_LEN_SHORT    8       // CH341 short packet length
#define mCH341_MAX_NUMBER       16      // Max devices

#define mMAX_BUFFER_LENGTH      0x1000  // 4096 bytes
#define mDEFAULT_BUFFER_LEN     0x0400  // 1024 bytes

// CH341 endpoints
#define mCH341_ENDP_INTER_UP    0x81
#define mCH341_ENDP_INTER_DOWN  0x01
#define mCH341_ENDP_DATA_UP     0x82
#define mCH341_ENDP_DATA_DOWN   0x02

// Pipe types
#define mPipeDeviceCtrl         0x00000004
#define mPipeInterUp            0x00000005
#define mPipeDataUp             0x00000006
#define mPipeDataDown           0x00000007

// Parallel port modes
#define mCH341_PARA_MODE_EPP    0x00
#define mCH341_PARA_MODE_EPP17  0x00
#define mCH341_PARA_MODE_EPP19  0x01
#define mCH341_PARA_MODE_MEM    0x02
#define mCH341_PARA_MODE_ECP    0x03

// IC Version
#define IC_VER_CH341A           0x20
#define IC_VER_CH341A3          0x30

// Device events
#define CH341_DEVICE_ARRIVAL        3
#define CH341_DEVICE_REMOVE_PEND    1
#define CH341_DEVICE_REMOVE         0

// I/O status bits
#define mStateBitERR            0x00000100
#define mStateBitPEMP           0x00000200
#define mStateBitINT            0x00000400
#define mStateBitSLCT           0x00000800
#define mStateBitWAIT           0x00002000
#define mStateBitDATAS          0x00004000
#define mStateBitADDRS          0x00008000
#define mStateBitRESET          0x00010000
#define mStateBitWRITE          0x00020000
#define mStateBitSCL            0x00400000
#define mStateBitSDA            0x00800000

#define MAX_DEVICE_PATH_SIZE    128
#define MAX_DEVICE_ID_SIZE      64

// ============================================================================
// Types
// ============================================================================

#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif

// USB Setup Packet
typedef struct _USB_SETUP_PKT {
    UCHAR mUspReqType;
    UCHAR mUspRequest;
    union {
        struct {
            UCHAR mUspValueLow;
            UCHAR mUspValueHigh;
        };
        USHORT mUspValue;
    };
    union {
        struct {
            UCHAR mUspIndexLow;
            UCHAR mUspIndexHigh;
        };
        USHORT mUspIndex;
    };
    USHORT mLength;
} mUSB_SETUP_PKT, *mPUSB_SETUP_PKT;

// WIN32 Command structure
typedef struct _WIN32_COMMAND {
    union {
        ULONG mFunction;
        NTSTATUS mStatus;
    };
    ULONG mLength;
    union {
        mUSB_SETUP_PKT mSetupPkt;
        UCHAR mBuffer[mCH341_PACKET_LENGTH];
    };
} mWIN32_COMMAND, *mPWIN32_COMMAND;

// EEPROM types
typedef enum _EEPROM_TYPE {
    ID_24C01,
    ID_24C02,
    ID_24C04,
    ID_24C08,
    ID_24C16,
    ID_24C32,
    ID_24C64,
    ID_24C128,
    ID_24C256,
    ID_24C512,
    ID_24C1024,
    ID_24C2048,
    ID_24C4096
} EEPROM_TYPE;

// Callback types
typedef VOID (CALLBACK *mPCH341_INT_ROUTINE)(ULONG iStatus);
typedef VOID (CALLBACK *mPCH341_NOTIFY_ROUTINE)(ULONG iEventStatus);

// ============================================================================
// Device Management Functions
// ============================================================================

// Open CH341 device, returns handle or INVALID_HANDLE_VALUE on failure
HANDLE WINAPI CH341OpenDevice(ULONG iIndex);

// Close CH341 device
VOID WINAPI CH341CloseDevice(ULONG iIndex);

// Get DLL version
ULONG WINAPI CH341GetVersion(void);

// Get driver version
ULONG WINAPI CH341GetDrvVersion(void);

// Reset USB device
BOOL WINAPI CH341ResetDevice(ULONG iIndex);

// Get device descriptor
BOOL WINAPI CH341GetDeviceDescr(ULONG iIndex, PVOID oBuffer, PULONG ioLength);

// Get config descriptor
BOOL WINAPI CH341GetConfigDescr(ULONG iIndex, PVOID oBuffer, PULONG ioLength);

// Get device name
PVOID WINAPI CH341GetDeviceName(ULONG iIndex);

// Get IC version: 0x20=CH341A, 0x30=CH341A3, mapped from CH347
ULONG WINAPI CH341GetVerIC(ULONG iIndex);

// Set exclusive use
BOOL WINAPI CH341SetExclusive(ULONG iIndex, ULONG iExclusive);

// Set USB timeout
BOOL WINAPI CH341SetTimeout(ULONG iIndex, ULONG iWriteTimeout, ULONG iReadTimeout);

// Flush buffer
BOOL WINAPI CH341FlushBuffer(ULONG iIndex);

// Driver command
ULONG WINAPI CH341DriverCommand(ULONG iIndex, mPWIN32_COMMAND ioCommand);

// ============================================================================
// I2C Functions
// ============================================================================

// Set stream mode (I2C speed, SPI mode)
// Bits 1-0: I2C speed: 00=20KHz, 01=100KHz, 10=400KHz, 11=750KHz
// Bit 2: SPI I/O mode: 0=single, 1=dual
// Bit 7: SPI bit order: 0=MSB first, 1=LSB first
BOOL WINAPI CH341SetStream(ULONG iIndex, ULONG iMode);

// Set hardware delay in milliseconds
BOOL WINAPI CH341SetDelaymS(ULONG iIndex, ULONG iDelay);

// Stream I2C transfer
BOOL WINAPI CH341StreamI2C(ULONG iIndex, ULONG iWriteLength, PVOID iWriteBuffer,
                           ULONG iReadLength, PVOID oReadBuffer);

// Read single byte from I2C device
BOOL WINAPI CH341ReadI2C(ULONG iIndex, UCHAR iDevice, UCHAR iAddr, PUCHAR oByte);

// Write single byte to I2C device
BOOL WINAPI CH341WriteI2C(ULONG iIndex, UCHAR iDevice, UCHAR iAddr, UCHAR iByte);

// ============================================================================
// EEPROM Functions
// ============================================================================

// Read data block from EEPROM
BOOL WINAPI CH341ReadEEPROM(ULONG iIndex, EEPROM_TYPE iEepromID,
                            ULONG iAddr, ULONG iLength, PUCHAR oBuffer);

// Write data block to EEPROM
BOOL WINAPI CH341WriteEEPROM(ULONG iIndex, EEPROM_TYPE iEepromID,
                             ULONG iAddr, ULONG iLength, PUCHAR iBuffer);

// ============================================================================
// SPI Functions
// ============================================================================

// SPI 4-wire stream transfer
BOOL WINAPI CH341StreamSPI4(ULONG iIndex, ULONG iChipSelect,
                            ULONG iLength, PVOID ioBuffer);

// SPI 5-wire stream transfer (dual channel)
BOOL WINAPI CH341StreamSPI5(ULONG iIndex, ULONG iChipSelect,
                            ULONG iLength, PVOID ioBuffer, PVOID ioBuffer2);

// SPI bit stream transfer
BOOL WINAPI CH341BitStreamSPI(ULONG iIndex, ULONG iLength, PVOID ioBuffer);

// Legacy SPI function (deprecated)
BOOL WINAPI CH341StreamSPI3(ULONG iIndex, ULONG iChipSelect,
                            ULONG iLength, PVOID ioBuffer);

// ============================================================================
// GPIO Functions
// ============================================================================

// Get input status
BOOL WINAPI CH341GetInput(ULONG iIndex, PULONG iStatus);

// Get status (same as GetInput)
BOOL WINAPI CH341GetStatus(ULONG iIndex, PULONG iStatus);

// Set output
// WARNING: Be careful not to damage the chip by setting wrong directions
BOOL WINAPI CH341SetOutput(ULONG iIndex, ULONG iEnable,
                           ULONG iSetDirOut, ULONG iSetDataOut);

// Set D5-D0 pins
BOOL WINAPI CH341Set_D5_D0(ULONG iIndex, ULONG iSetDirOut, ULONG iSetDataOut);

// ============================================================================
// Data Transfer Functions
// ============================================================================

// Read data block
BOOL WINAPI CH341ReadData(ULONG iIndex, PVOID oBuffer, PULONG ioLength);

// Write data block
BOOL WINAPI CH341WriteData(ULONG iIndex, PVOID iBuffer, PULONG ioLength);

// Read data from port 0
BOOL WINAPI CH341ReadData0(ULONG iIndex, PVOID oBuffer, PULONG ioLength);

// Read data from port 1
BOOL WINAPI CH341ReadData1(ULONG iIndex, PVOID oBuffer, PULONG ioLength);

// Write data to port 0
BOOL WINAPI CH341WriteData0(ULONG iIndex, PVOID iBuffer, PULONG ioLength);

// Write data to port 1
BOOL WINAPI CH341WriteData1(ULONG iIndex, PVOID iBuffer, PULONG ioLength);

// Write then read
BOOL WINAPI CH341WriteRead(ULONG iIndex, ULONG iWriteLength, PVOID iWriteBuffer,
                           ULONG iReadStep, ULONG iReadTimes,
                           PULONG oReadLength, PVOID oReadBuffer);

// ============================================================================
// Parallel Port Functions (EPP/MEM modes)
// ============================================================================

// Set parallel mode
BOOL WINAPI CH341SetParaMode(ULONG iIndex, ULONG iMode);

// Initialize parallel port
BOOL WINAPI CH341InitParallel(ULONG iIndex, ULONG iMode);

// EPP read data
BOOL WINAPI CH341EppReadData(ULONG iIndex, PVOID oBuffer, PULONG ioLength);

// EPP read address
BOOL WINAPI CH341EppReadAddr(ULONG iIndex, PVOID oBuffer, PULONG ioLength);

// EPP write data
BOOL WINAPI CH341EppWriteData(ULONG iIndex, PVOID iBuffer, PULONG ioLength);

// EPP write address
BOOL WINAPI CH341EppWriteAddr(ULONG iIndex, PVOID iBuffer, PULONG ioLength);

// EPP set address
BOOL WINAPI CH341EppSetAddr(ULONG iIndex, UCHAR iAddr);

// MEM read address 0
BOOL WINAPI CH341MemReadAddr0(ULONG iIndex, PVOID oBuffer, PULONG ioLength);

// MEM read address 1
BOOL WINAPI CH341MemReadAddr1(ULONG iIndex, PVOID oBuffer, PULONG ioLength);

// MEM write address 0
BOOL WINAPI CH341MemWriteAddr0(ULONG iIndex, PVOID iBuffer, PULONG ioLength);

// MEM write address 1
BOOL WINAPI CH341MemWriteAddr1(ULONG iIndex, PVOID iBuffer, PULONG ioLength);

// ============================================================================
// Interrupt Functions
// ============================================================================

// Set interrupt routine
BOOL WINAPI CH341SetIntRoutine(ULONG iIndex, mPCH341_INT_ROUTINE iIntRoutine);

// Read interrupt data
BOOL WINAPI CH341ReadInter(ULONG iIndex, PULONG iStatus);

// Abort interrupt read
BOOL WINAPI CH341AbortInter(ULONG iIndex);

// Reset interrupt
BOOL WINAPI CH341ResetInter(ULONG iIndex);

// ============================================================================
// Abort/Reset Functions
// ============================================================================

// Abort read operation
BOOL WINAPI CH341AbortRead(ULONG iIndex);

// Abort write operation
BOOL WINAPI CH341AbortWrite(ULONG iIndex);

// Reset read
BOOL WINAPI CH341ResetRead(ULONG iIndex);

// Reset write
BOOL WINAPI CH341ResetWrite(ULONG iIndex);

// ============================================================================
// Buffer Upload/Download Functions
// ============================================================================

// Set buffer upload mode
BOOL WINAPI CH341SetBufUpload(ULONG iIndex, ULONG iEnableOrClear);

// Query buffer upload count
LONG WINAPI CH341QueryBufUpload(ULONG iIndex);

// Set buffer download mode
BOOL WINAPI CH341SetBufDownload(ULONG iIndex, ULONG iEnableOrClear);

// Query buffer download count
LONG WINAPI CH341QueryBufDownload(ULONG iIndex);

// ============================================================================
// Device Notification
// ============================================================================

// Set device notification callback
BOOL WINAPI CH341SetDeviceNotify(ULONG iIndex, PCHAR iDeviceID,
                                  mPCH341_NOTIFY_ROUTINE iNotifyRoutine);

// ============================================================================
// Extended Functions
// ============================================================================

// Open device (extended)
HANDLE WINAPI CH341OpenDeviceEx(ULONG iIndex);

// Close device (extended)
VOID WINAPI CH341CloseDeviceEx(ULONG iIndex);

// Get device name (extended)
PCHAR WINAPI CH341GetDeviceNameEx(ULONG iIndex);

// Set device notification (extended)
BOOL WINAPI CH341SetDeviceNotifyEx(ULONG iIndex, PCHAR iDeviceID,
                                    mPCH341_NOTIFY_ROUTINE iNotifyRoutine);

// ============================================================================
// Serial Port Functions (for CH341 in serial mode)
// ============================================================================

// Setup serial port
BOOL WINAPI CH341SetupSerial(ULONG iIndex, ULONG iParityMode, ULONG iBaudRate);

#ifdef __cplusplus
}
#endif

#endif // _CH341_COMPAT_H
