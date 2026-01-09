//! SPI Flash Operations
//!
//! Support for common SPI NOR flash chips used in BIOS

use crate::ch347::{Ch347Device, Ch347Error, Result, SpiClock};
use serde::{Deserialize, Serialize};

// Common SPI Flash Commands
pub const CMD_READ_JEDEC_ID: u8 = 0x9F;
pub const CMD_READ_STATUS: u8 = 0x05;
pub const CMD_READ_STATUS2: u8 = 0x35;
pub const CMD_WRITE_ENABLE: u8 = 0x06;
pub const CMD_WRITE_DISABLE: u8 = 0x04;
pub const CMD_PAGE_PROGRAM: u8 = 0x02;
pub const CMD_READ_DATA: u8 = 0x03;
pub const CMD_FAST_READ: u8 = 0x0B;
pub const CMD_SECTOR_ERASE: u8 = 0x20;   // 4KB
pub const CMD_BLOCK_ERASE_32K: u8 = 0x52;
pub const CMD_BLOCK_ERASE_64K: u8 = 0xD8;
pub const CMD_CHIP_ERASE: u8 = 0xC7;     // or 0x60
pub const CMD_POWER_DOWN: u8 = 0xB9;
pub const CMD_RELEASE_PD: u8 = 0xAB;

// Status register bits
pub const STATUS_WIP: u8 = 0x01;  // Write In Progress
pub const STATUS_WEL: u8 = 0x02;  // Write Enable Latch

/// Flash chip information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FlashChip {
    pub name: String,
    pub manufacturer: String,
    pub jedec_id: [u8; 3],
    pub size: usize,           // Total size in bytes
    pub page_size: usize,      // Page size (usually 256)
    pub sector_size: usize,    // Sector size (usually 4096)
    pub block_size: usize,     // Block size (usually 65536)
}

impl FlashChip {
    pub fn size_str(&self) -> String {
        if self.size >= 1024 * 1024 {
            format!("{}MB", self.size / (1024 * 1024))
        } else if self.size >= 1024 {
            format!("{}KB", self.size / 1024)
        } else {
            format!("{}B", self.size)
        }
    }
}

/// Flash chip database
pub fn get_flash_database() -> Vec<FlashChip> {
    vec![
        // Winbond
        FlashChip {
            name: "W25Q16".into(),
            manufacturer: "Winbond".into(),
            jedec_id: [0xEF, 0x40, 0x15],
            size: 2 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        FlashChip {
            name: "W25Q32".into(),
            manufacturer: "Winbond".into(),
            jedec_id: [0xEF, 0x40, 0x16],
            size: 4 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        FlashChip {
            name: "W25Q64".into(),
            manufacturer: "Winbond".into(),
            jedec_id: [0xEF, 0x40, 0x17],
            size: 8 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        FlashChip {
            name: "W25Q128".into(),
            manufacturer: "Winbond".into(),
            jedec_id: [0xEF, 0x40, 0x18],
            size: 16 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        FlashChip {
            name: "W25Q256".into(),
            manufacturer: "Winbond".into(),
            jedec_id: [0xEF, 0x40, 0x19],
            size: 32 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        // GigaDevice
        FlashChip {
            name: "GD25Q16".into(),
            manufacturer: "GigaDevice".into(),
            jedec_id: [0xC8, 0x40, 0x15],
            size: 2 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        FlashChip {
            name: "GD25Q32".into(),
            manufacturer: "GigaDevice".into(),
            jedec_id: [0xC8, 0x40, 0x16],
            size: 4 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        FlashChip {
            name: "GD25Q64".into(),
            manufacturer: "GigaDevice".into(),
            jedec_id: [0xC8, 0x40, 0x17],
            size: 8 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        FlashChip {
            name: "GD25Q128".into(),
            manufacturer: "GigaDevice".into(),
            jedec_id: [0xC8, 0x40, 0x18],
            size: 16 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        // Macronix
        FlashChip {
            name: "MX25L6405".into(),
            manufacturer: "Macronix".into(),
            jedec_id: [0xC2, 0x20, 0x17],
            size: 8 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        FlashChip {
            name: "MX25L12835F".into(),
            manufacturer: "Macronix".into(),
            jedec_id: [0xC2, 0x20, 0x18],
            size: 16 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        FlashChip {
            name: "MX25L25635F".into(),
            manufacturer: "Macronix".into(),
            jedec_id: [0xC2, 0x20, 0x19],
            size: 32 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        // Spansion/Cypress
        FlashChip {
            name: "S25FL128S".into(),
            manufacturer: "Spansion".into(),
            jedec_id: [0x01, 0x20, 0x18],
            size: 16 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        // ISSI
        FlashChip {
            name: "IS25LP128".into(),
            manufacturer: "ISSI".into(),
            jedec_id: [0x9D, 0x60, 0x18],
            size: 16 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        // XMC
        FlashChip {
            name: "XM25QH128A".into(),
            manufacturer: "XMC".into(),
            jedec_id: [0x20, 0x70, 0x18],
            size: 16 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
        // ESMT
        FlashChip {
            name: "F25L16PA".into(),
            manufacturer: "ESMT".into(),
            jedec_id: [0x8C, 0x21, 0x15],
            size: 2 * 1024 * 1024,
            page_size: 256,
            sector_size: 4096,
            block_size: 65536,
        },
    ]
}

/// Identify chip by JEDEC ID
pub fn identify_chip(jedec_id: &[u8; 3]) -> Option<FlashChip> {
    get_flash_database()
        .into_iter()
        .find(|chip| &chip.jedec_id == jedec_id)
}

/// Create unknown chip info
pub fn unknown_chip(jedec_id: [u8; 3]) -> FlashChip {
    // Try to guess size from third byte
    let size = match jedec_id[2] {
        0x14 => 1 * 1024 * 1024,    // 1MB / 8Mbit
        0x15 => 2 * 1024 * 1024,    // 2MB / 16Mbit
        0x16 => 4 * 1024 * 1024,    // 4MB / 32Mbit
        0x17 => 8 * 1024 * 1024,    // 8MB / 64Mbit
        0x18 => 16 * 1024 * 1024,   // 16MB / 128Mbit
        0x19 => 32 * 1024 * 1024,   // 32MB / 256Mbit
        0x1A => 64 * 1024 * 1024,   // 64MB / 512Mbit
        0x20 => 64 * 1024 * 1024,   // 64MB
        0x21 => 128 * 1024 * 1024,  // 128MB
        _ => 16 * 1024 * 1024,      // Default 16MB
    };

    FlashChip {
        name: format!("Unknown ({:02X}{:02X}{:02X})", jedec_id[0], jedec_id[1], jedec_id[2]),
        manufacturer: "Unknown".into(),
        jedec_id,
        size,
        page_size: 256,
        sector_size: 4096,
        block_size: 65536,
    }
}

/// SPI Flash Programmer
pub struct FlashProgrammer {
    device: Ch347Device,
    chip: Option<FlashChip>,
}

impl FlashProgrammer {
    /// Create new programmer
    pub fn new() -> Result<Self> {
        let mut device = Ch347Device::open()?;

        // Initialize SPI with 15MHz clock (default, safe for most chips)
        device.spi_init(SpiClock::Clk15MHz)?;

        Ok(Self {
            device,
            chip: None,
        })
    }

    /// Detect and identify flash chip
    pub fn detect(&mut self) -> Result<FlashChip> {
        let jedec_id = self.read_jedec_id()?;

        let chip = identify_chip(&jedec_id)
            .unwrap_or_else(|| unknown_chip(jedec_id));

        self.chip = Some(chip.clone());
        Ok(chip)
    }

    /// Read JEDEC ID
    pub fn read_jedec_id(&mut self) -> Result<[u8; 3]> {
        self.device.spi_cs(true)?;

        let cmd = [CMD_READ_JEDEC_ID];
        let mut resp = [0u8; 3];

        self.device.spi_write(&cmd)?;
        self.device.spi_read(&mut resp)?;

        self.device.spi_cs(false)?;

        // Validate - shouldn't be all 0xFF or 0x00
        if (resp[0] == 0xFF && resp[1] == 0xFF && resp[2] == 0xFF) ||
           (resp[0] == 0x00 && resp[1] == 0x00 && resp[2] == 0x00) {
            return Err(Ch347Error::DeviceNotFound);
        }

        Ok(resp)
    }

    /// Read status register
    pub fn read_status(&mut self) -> Result<u8> {
        self.device.spi_cs(true)?;

        let cmd = [CMD_READ_STATUS];
        let mut status = [0u8; 1];

        self.device.spi_write(&cmd)?;
        self.device.spi_read(&mut status)?;

        self.device.spi_cs(false)?;

        Ok(status[0])
    }

    /// Wait for write to complete
    pub fn wait_ready(&mut self, timeout_ms: u32) -> Result<()> {
        let start = std::time::Instant::now();
        let timeout = std::time::Duration::from_millis(timeout_ms as u64);

        loop {
            let status = self.read_status()?;
            if (status & STATUS_WIP) == 0 {
                return Ok(());
            }

            if start.elapsed() > timeout {
                return Err(Ch347Error::TransferFailed("Timeout waiting for ready".into()));
            }

            std::thread::sleep(std::time::Duration::from_millis(1));
        }
    }

    /// Enable write
    pub fn write_enable(&mut self) -> Result<()> {
        self.device.spi_cs(true)?;
        self.device.spi_write(&[CMD_WRITE_ENABLE])?;
        self.device.spi_cs(false)?;

        // Verify WEL bit is set
        let status = self.read_status()?;
        if (status & STATUS_WEL) == 0 {
            return Err(Ch347Error::TransferFailed("Write enable failed".into()));
        }

        Ok(())
    }

    /// Read data from flash
    pub fn read(&mut self, address: u32, data: &mut [u8]) -> Result<()> {
        self.device.spi_cs(true)?;

        // Send read command with 24-bit address
        let cmd = [
            CMD_READ_DATA,
            ((address >> 16) & 0xFF) as u8,
            ((address >> 8) & 0xFF) as u8,
            (address & 0xFF) as u8,
        ];
        self.device.spi_write(&cmd)?;

        // Read data in chunks
        const CHUNK_SIZE: usize = 256;
        for chunk in data.chunks_mut(CHUNK_SIZE) {
            self.device.spi_read(chunk)?;
        }

        self.device.spi_cs(false)?;

        Ok(())
    }

    /// Erase sector (4KB)
    pub fn erase_sector(&mut self, address: u32) -> Result<()> {
        self.write_enable()?;

        self.device.spi_cs(true)?;

        let cmd = [
            CMD_SECTOR_ERASE,
            ((address >> 16) & 0xFF) as u8,
            ((address >> 8) & 0xFF) as u8,
            (address & 0xFF) as u8,
        ];
        self.device.spi_write(&cmd)?;

        self.device.spi_cs(false)?;

        // Sector erase typically takes 50-400ms
        self.wait_ready(500)?;

        Ok(())
    }

    /// Erase block (64KB)
    pub fn erase_block(&mut self, address: u32) -> Result<()> {
        self.write_enable()?;

        self.device.spi_cs(true)?;

        let cmd = [
            CMD_BLOCK_ERASE_64K,
            ((address >> 16) & 0xFF) as u8,
            ((address >> 8) & 0xFF) as u8,
            (address & 0xFF) as u8,
        ];
        self.device.spi_write(&cmd)?;

        self.device.spi_cs(false)?;

        // Block erase typically takes 150-2000ms
        self.wait_ready(3000)?;

        Ok(())
    }

    /// Erase entire chip
    pub fn erase_chip(&mut self) -> Result<()> {
        self.write_enable()?;

        self.device.spi_cs(true)?;
        self.device.spi_write(&[CMD_CHIP_ERASE])?;
        self.device.spi_cs(false)?;

        // Chip erase can take very long (up to 200 seconds for large chips)
        self.wait_ready(200000)?;

        Ok(())
    }

    /// Program page (up to 256 bytes)
    pub fn program_page(&mut self, address: u32, data: &[u8]) -> Result<()> {
        if data.is_empty() || data.len() > 256 {
            return Err(Ch347Error::TransferFailed("Invalid page size".into()));
        }

        self.write_enable()?;

        self.device.spi_cs(true)?;

        // Send program command with address
        let cmd = [
            CMD_PAGE_PROGRAM,
            ((address >> 16) & 0xFF) as u8,
            ((address >> 8) & 0xFF) as u8,
            (address & 0xFF) as u8,
        ];
        self.device.spi_write(&cmd)?;

        // Write data
        self.device.spi_write(data)?;

        self.device.spi_cs(false)?;

        // Page program typically takes 0.7-3ms
        self.wait_ready(10)?;

        Ok(())
    }

    /// Write data with automatic page handling
    pub fn write(&mut self, address: u32, data: &[u8], progress: Option<&dyn Fn(usize, usize)>) -> Result<()> {
        let page_size = self.chip.as_ref().map(|c| c.page_size).unwrap_or(256);
        let total = data.len();
        let mut offset = 0;
        let mut addr = address;

        while offset < total {
            // Calculate bytes to write in this page
            let page_offset = (addr as usize) % page_size;
            let chunk_size = std::cmp::min(page_size - page_offset, total - offset);

            self.program_page(addr, &data[offset..offset + chunk_size])?;

            offset += chunk_size;
            addr += chunk_size as u32;

            if let Some(cb) = progress {
                cb(offset, total);
            }
        }

        Ok(())
    }

    /// Verify data
    pub fn verify(&mut self, address: u32, data: &[u8], progress: Option<&dyn Fn(usize, usize)>) -> Result<bool> {
        const CHUNK_SIZE: usize = 4096;
        let total = data.len();
        let mut offset = 0;
        let mut addr = address;
        let mut read_buf = vec![0u8; CHUNK_SIZE];

        while offset < total {
            let chunk_size = std::cmp::min(CHUNK_SIZE, total - offset);

            self.read(addr, &mut read_buf[..chunk_size])?;

            if read_buf[..chunk_size] != data[offset..offset + chunk_size] {
                return Ok(false);
            }

            offset += chunk_size;
            addr += chunk_size as u32;

            if let Some(cb) = progress {
                cb(offset, total);
            }
        }

        Ok(true)
    }

    /// Get detected chip info
    pub fn get_chip(&self) -> Option<&FlashChip> {
        self.chip.as_ref()
    }
}
