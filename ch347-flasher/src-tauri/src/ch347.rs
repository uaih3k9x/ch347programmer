//! CH347 USB Communication Layer
//!
//! Implements low-level USB communication with CH347 chip using libusb/rusb
//! Based on flashrom's ch347_spi.c implementation

use rusb::{Context, Device, DeviceHandle, UsbContext};
use std::time::Duration;
use thiserror::Error;

// CH347 USB IDs
pub const CH347_VID: u16 = 0x1A86;   // WCH Vendor ID
pub const CH347T_PID: u16 = 0x55DB;  // CH347T
pub const CH347F_PID: u16 = 0x55DE;  // CH347F

// CH347 Endpoints
pub const EP_OUT: u8 = 0x06;  // Bulk OUT endpoint
pub const EP_IN: u8 = 0x86;   // Bulk IN endpoint

// Interface numbers
pub const CH347T_IFACE: u8 = 2;  // CH347T SPI interface
pub const CH347F_IFACE: u8 = 4;  // CH347F SPI interface

// Packet size (from flashrom: max 510, leaving 507 for data)
pub const PACKET_SIZE: usize = 510;
pub const MAX_DATA_LEN: usize = PACKET_SIZE - 3;

// Timeouts
pub const USB_TIMEOUT: Duration = Duration::from_millis(1000);

// SPI Commands (from flashrom ch347_spi.c)
pub const CMD_SPI_SET_CFG: u8 = 0xC0;   // Configure SPI
pub const CMD_SPI_CS_CTRL: u8 = 0xC1;   // CS control
pub const CMD_SPI_OUT_IN: u8 = 0xC2;    // Write and read simultaneously
pub const CMD_SPI_IN: u8 = 0xC3;        // Read only
pub const CMD_SPI_OUT: u8 = 0xC4;       // Write only
pub const CMD_SPI_GET_CFG: u8 = 0xCA;   // Get SPI config

// CS Control flags (from flashrom)
pub const CS_ASSERT: u8 = 0x00;    // Assert CS (active low)
pub const CS_DEASSERT: u8 = 0x40;  // Deassert CS
pub const CS_CHANGE: u8 = 0x80;    // Change CS state
pub const CS_IGNORE: u8 = 0x00;    // Ignore this CS

// SPI Clock speeds (divisor values)
#[derive(Debug, Clone, Copy, PartialEq)]
#[repr(u8)]
pub enum SpiClock {
    Clk60MHz = 0,
    Clk30MHz = 1,
    Clk15MHz = 2,
    Clk7_5MHz = 3,
    Clk3_75MHz = 4,
    Clk1_875MHz = 5,
    Clk937_5KHz = 6,
    Clk468_75KHz = 7,
}

impl Default for SpiClock {
    fn default() -> Self {
        SpiClock::Clk15MHz  // Default to 15MHz like flashrom
    }
}

#[derive(Error, Debug)]
pub enum Ch347Error {
    #[error("USB error: {0}")]
    Usb(#[from] rusb::Error),

    #[error("Device not found")]
    DeviceNotFound,

    #[error("Device busy or permission denied")]
    DeviceBusy,

    #[error("Invalid response from device")]
    InvalidResponse,

    #[error("Transfer failed: {0}")]
    TransferFailed(String),

    #[error("SPI not initialized")]
    SpiNotInitialized,
}

pub type Result<T> = std::result::Result<T, Ch347Error>;

/// Device information
#[derive(Debug, Clone)]
pub struct DeviceInfo {
    pub vid: u16,
    pub pid: u16,
    pub manufacturer: String,
    pub product: String,
    pub is_ch347t: bool,
}

/// CH347 Device Handle
pub struct Ch347Device {
    handle: DeviceHandle<Context>,
    interface: u8,
    spi_initialized: bool,
}

impl Ch347Device {
    /// Find and open CH347 device
    pub fn open() -> Result<Self> {
        let context = Context::new()?;

        // Try CH347T first, then CH347F
        let devices_to_try = [
            (CH347T_PID, CH347T_IFACE),
            (CH347F_PID, CH347F_IFACE),
        ];

        for device in context.devices()?.iter() {
            let desc = match device.device_descriptor() {
                Ok(d) => d,
                Err(_) => continue,
            };

            if desc.vendor_id() != CH347_VID {
                continue;
            }

            let pid = desc.product_id();
            for (target_pid, iface) in devices_to_try.iter() {
                if pid == *target_pid {
                    match Self::open_device(&device, *iface) {
                        Ok(dev) => return Ok(dev),
                        Err(_) => break, // Try next device
                    }
                }
            }
        }

        Err(Ch347Error::DeviceNotFound)
    }

    /// Open specific device with given interface
    fn open_device(device: &Device<Context>, interface: u8) -> Result<Self> {
        let handle = device.open()?;

        // Detach kernel driver if needed (Linux/macOS)
        #[cfg(any(target_os = "linux", target_os = "macos"))]
        {
            if handle.kernel_driver_active(interface).unwrap_or(false) {
                let _ = handle.detach_kernel_driver(interface);
            }
        }

        // Claim interface
        handle.claim_interface(interface)?;

        Ok(Self {
            handle,
            interface,
            spi_initialized: false,
        })
    }

    /// Get device info
    pub fn get_info(&self) -> Result<DeviceInfo> {
        let device = self.handle.device();
        let desc = device.device_descriptor()?;

        let manufacturer = self.handle
            .read_manufacturer_string_ascii(&desc)
            .unwrap_or_default();
        let product = self.handle
            .read_product_string_ascii(&desc)
            .unwrap_or_default();

        Ok(DeviceInfo {
            vid: desc.vendor_id(),
            pid: desc.product_id(),
            manufacturer,
            product,
            is_ch347t: desc.product_id() == CH347T_PID,
        })
    }

    /// Configure SPI interface (based on flashrom ch347_spi_config)
    pub fn spi_init(&mut self, clock: SpiClock) -> Result<()> {
        // 29-byte config packet (from flashrom)
        let mut cmd = [0u8; 29];
        cmd[0] = CMD_SPI_SET_CFG;
        cmd[1] = 26;  // Payload length low byte
        cmd[2] = 0;   // Payload length high byte

        // Mystery bytes that vendor driver sets
        cmd[5] = 4;
        cmd[6] = 1;

        // Clock polarity (CPOL): bit 1 = 0 for mode 0
        cmd[9] = 0;

        // Clock phase (CPHA): bit 0 = 0 for mode 0
        cmd[11] = 0;

        // Another mystery byte
        cmd[14] = 2;

        // Clock divisor: bits 5:3
        cmd[15] = (clock as u8) << 3;

        // Bit order: bit 7, 0=MSB first
        cmd[17] = 0;

        // Yet another mystery byte
        cmd[19] = 7;

        // CS polarity: bit 7 CS2, bit 6 CS1. 0 = active low
        cmd[24] = 0;

        // Send config
        self.write_bulk(&cmd)?;

        // Read response
        let mut resp = [0u8; 29];
        self.read_bulk(&mut resp)?;

        self.spi_initialized = true;
        Ok(())
    }

    /// Control CS (chip select) - based on flashrom ch347_cs_control
    pub fn spi_cs(&mut self, assert: bool) -> Result<()> {
        let mut cmd = [0u8; 13];
        cmd[0] = CMD_SPI_CS_CTRL;
        cmd[1] = 10;  // Payload length
        cmd[2] = 0;

        // CS1 control at offset 3
        if assert {
            cmd[3] = CS_ASSERT | CS_CHANGE;
        } else {
            cmd[3] = CS_DEASSERT | CS_CHANGE;
        }

        // CS2 control at offset 8 - ignore
        cmd[8] = CS_IGNORE;

        self.write_bulk(&cmd)?;
        Ok(())
    }

    /// SPI write only - based on flashrom ch347_write
    pub fn spi_write(&mut self, data: &[u8]) -> Result<()> {
        if !self.spi_initialized {
            return Err(Ch347Error::SpiNotInitialized);
        }

        let mut bytes_written = 0;
        let mut buffer = [0u8; PACKET_SIZE];

        while bytes_written < data.len() {
            let chunk_len = std::cmp::min(MAX_DATA_LEN, data.len() - bytes_written);

            buffer[0] = CMD_SPI_OUT;
            buffer[1] = (chunk_len & 0xFF) as u8;
            buffer[2] = ((chunk_len >> 8) & 0xFF) as u8;
            buffer[3..3+chunk_len].copy_from_slice(&data[bytes_written..bytes_written+chunk_len]);

            let packet_len = chunk_len + 3;
            self.write_bulk(&buffer[..packet_len])?;

            // Read response (4 bytes)
            let mut resp = [0u8; 4];
            self.read_bulk(&mut resp)?;

            bytes_written += chunk_len;
        }

        Ok(())
    }

    /// SPI read only - based on flashrom ch347_read
    pub fn spi_read(&mut self, data: &mut [u8]) -> Result<()> {
        if !self.spi_initialized {
            return Err(Ch347Error::SpiNotInitialized);
        }

        let readcnt = data.len();

        // Send read command with 32-bit length
        let cmd = [
            CMD_SPI_IN,
            4,  // Payload length (4 bytes for the count)
            0,
            (readcnt & 0xFF) as u8,
            ((readcnt >> 8) & 0xFF) as u8,
            ((readcnt >> 16) & 0xFF) as u8,
            ((readcnt >> 24) & 0xFF) as u8,
        ];

        self.write_bulk(&cmd)?;

        // Read data in packets
        let mut bytes_read = 0;
        let mut buffer = [0u8; PACKET_SIZE];

        while bytes_read < readcnt {
            let transferred = self.read_bulk(&mut buffer)?;

            if transferred < 3 {
                return Err(Ch347Error::InvalidResponse);
            }

            // Response format: u8 command, u16 data length, then data
            let data_len = (buffer[1] as usize) | ((buffer[2] as usize) << 8);

            if transferred < 3 + data_len {
                return Err(Ch347Error::InvalidResponse);
            }

            let copy_len = std::cmp::min(data_len, readcnt - bytes_read);
            data[bytes_read..bytes_read+copy_len].copy_from_slice(&buffer[3..3+copy_len]);

            bytes_read += data_len;
        }

        Ok(())
    }

    /// SPI write then read (with CS control) - main interface for flash operations
    pub fn spi_transfer(&mut self, write_data: &[u8], read_data: &mut [u8]) -> Result<()> {
        self.spi_cs(true)?;

        if !write_data.is_empty() {
            self.spi_write(write_data)?;
        }

        if !read_data.is_empty() {
            self.spi_read(read_data)?;
        }

        self.spi_cs(false)?;

        Ok(())
    }

    /// Write to bulk endpoint
    fn write_bulk(&self, data: &[u8]) -> Result<usize> {
        let written = self.handle.write_bulk(EP_OUT, data, USB_TIMEOUT)?;
        Ok(written)
    }

    /// Read from bulk endpoint
    fn read_bulk(&self, data: &mut [u8]) -> Result<usize> {
        let read = self.handle.read_bulk(EP_IN, data, USB_TIMEOUT)?;
        Ok(read)
    }
}

impl Drop for Ch347Device {
    fn drop(&mut self) {
        let _ = self.handle.release_interface(self.interface);
    }
}

/// List all CH347 devices
pub fn list_devices() -> Result<Vec<DeviceInfo>> {
    let context = Context::new()?;
    let pids = [CH347T_PID, CH347F_PID];
    let mut devices = Vec::new();

    for device in context.devices()?.iter() {
        let desc = match device.device_descriptor() {
            Ok(d) => d,
            Err(_) => continue,
        };

        if desc.vendor_id() == CH347_VID && pids.contains(&desc.product_id()) {
            let handle = match device.open() {
                Ok(h) => h,
                Err(_) => continue,
            };

            let manufacturer = handle
                .read_manufacturer_string_ascii(&desc)
                .unwrap_or_default();
            let product = handle
                .read_product_string_ascii(&desc)
                .unwrap_or_default();

            devices.push(DeviceInfo {
                vid: desc.vendor_id(),
                pid: desc.product_id(),
                manufacturer,
                product,
                is_ch347t: desc.product_id() == CH347T_PID,
            });
        }
    }

    Ok(devices)
}
