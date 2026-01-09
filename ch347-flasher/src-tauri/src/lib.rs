//! CH347 Flash Programmer - Tauri Backend
//!
//! Provides Tauri commands for the frontend GUI

mod ch347;
mod flash;

use flash::{FlashChip, FlashProgrammer, get_flash_database};
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use tauri::{State, Emitter, AppHandle};

/// Application state
pub struct AppState {
    programmer: Mutex<Option<FlashProgrammer>>,
    current_chip: Mutex<Option<FlashChip>>,
}

impl Default for AppState {
    fn default() -> Self {
        Self {
            programmer: Mutex::new(None),
            current_chip: Mutex::new(None),
        }
    }
}

/// Result type for Tauri commands
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CmdResult<T> {
    pub success: bool,
    pub data: Option<T>,
    pub error: Option<String>,
}

impl<T> CmdResult<T> {
    pub fn ok(data: T) -> Self {
        Self {
            success: true,
            data: Some(data),
            error: None,
        }
    }

    pub fn err(msg: impl Into<String>) -> Self {
        Self {
            success: false,
            data: None,
            error: Some(msg.into()),
        }
    }
}

/// Device info for frontend
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceInfo {
    pub connected: bool,
    pub vid: Option<u16>,
    pub pid: Option<u16>,
    pub name: Option<String>,
}

/// Chip info for frontend
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChipInfo {
    pub detected: bool,
    pub name: String,
    pub manufacturer: String,
    pub jedec_id: String,
    pub size: usize,
    pub size_str: String,
}

/// Progress info
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProgressInfo {
    pub current: usize,
    pub total: usize,
    pub percent: f32,
    pub operation: String,
}

// ============================================================================
// Tauri Commands
// ============================================================================

/// Connect to CH347 device
#[tauri::command]
fn connect(state: State<'_, Arc<AppState>>) -> CmdResult<DeviceInfo> {
    let mut programmer_guard = state.programmer.lock();

    match FlashProgrammer::new() {
        Ok(prog) => {
            *programmer_guard = Some(prog);
            CmdResult::ok(DeviceInfo {
                connected: true,
                vid: Some(ch347::CH347_VID),
                pid: Some(ch347::CH347T_PID),
                name: Some("CH347".into()),
            })
        }
        Err(e) => CmdResult::err(format!("Failed to connect: {}", e)),
    }
}

/// Disconnect from device
#[tauri::command]
fn disconnect(state: State<'_, Arc<AppState>>) -> CmdResult<()> {
    let mut programmer_guard = state.programmer.lock();
    let mut chip_guard = state.current_chip.lock();

    *programmer_guard = None;
    *chip_guard = None;

    CmdResult::ok(())
}

/// Check connection status
#[tauri::command]
fn is_connected(state: State<'_, Arc<AppState>>) -> bool {
    state.programmer.lock().is_some()
}

/// Detect flash chip
#[tauri::command]
fn detect_chip(state: State<'_, Arc<AppState>>) -> CmdResult<ChipInfo> {
    let mut programmer_guard = state.programmer.lock();
    let mut chip_guard = state.current_chip.lock();

    let programmer = match programmer_guard.as_mut() {
        Some(p) => p,
        None => return CmdResult::err("Not connected"),
    };

    match programmer.detect() {
        Ok(chip) => {
            let info = ChipInfo {
                detected: true,
                name: chip.name.clone(),
                manufacturer: chip.manufacturer.clone(),
                jedec_id: format!("{:02X} {:02X} {:02X}",
                    chip.jedec_id[0], chip.jedec_id[1], chip.jedec_id[2]),
                size: chip.size,
                size_str: chip.size_str(),
            };
            *chip_guard = Some(chip);
            CmdResult::ok(info)
        }
        Err(e) => CmdResult::err(format!("Detection failed: {}", e)),
    }
}

/// Read flash to file
#[tauri::command]
fn read_flash(
    state: State<'_, Arc<AppState>>,
    app: AppHandle,
    path: String,
) -> CmdResult<()> {
    let mut programmer_guard = state.programmer.lock();
    let chip_guard = state.current_chip.lock();

    let programmer = match programmer_guard.as_mut() {
        Some(p) => p,
        None => return CmdResult::err("Not connected"),
    };

    let chip = match chip_guard.as_ref() {
        Some(c) => c,
        None => return CmdResult::err("No chip detected"),
    };

    let size = chip.size;
    let mut data = vec![0u8; size];

    // Read in 64KB chunks for progress
    const CHUNK_SIZE: usize = 65536;
    let mut offset = 0;

    while offset < size {
        let chunk_len = std::cmp::min(CHUNK_SIZE, size - offset);

        if let Err(e) = programmer.read(offset as u32, &mut data[offset..offset + chunk_len]) {
            return CmdResult::err(format!("Read error at 0x{:06X}: {}", offset, e));
        }

        offset += chunk_len;

        // Send progress
        let _ = app.emit("progress", ProgressInfo {
            current: offset,
            total: size,
            percent: (offset as f32 / size as f32) * 100.0,
            operation: "Reading".into(),
        });
    }

    // Write to file
    if let Err(e) = std::fs::write(&path, &data) {
        return CmdResult::err(format!("Failed to save file: {}", e));
    }

    CmdResult::ok(())
}

/// Write flash from file
#[tauri::command]
fn write_flash(
    state: State<'_, Arc<AppState>>,
    app: AppHandle,
    path: String,
    verify: bool,
) -> CmdResult<()> {
    let mut programmer_guard = state.programmer.lock();
    let chip_guard = state.current_chip.lock();

    let programmer = match programmer_guard.as_mut() {
        Some(p) => p,
        None => return CmdResult::err("Not connected"),
    };

    let chip = match chip_guard.as_ref() {
        Some(c) => c.clone(),
        None => return CmdResult::err("No chip detected"),
    };

    // Read file
    let data = match std::fs::read(&path) {
        Ok(d) => d,
        Err(e) => return CmdResult::err(format!("Failed to read file: {}", e)),
    };

    if data.len() > chip.size {
        return CmdResult::err(format!(
            "File size ({}) exceeds chip size ({})",
            data.len(),
            chip.size
        ));
    }

    let size = data.len();

    // Erase required sectors
    let sectors = (size + chip.sector_size - 1) / chip.sector_size;
    let _ = app.emit("progress", ProgressInfo {
        current: 0,
        total: sectors,
        percent: 0.0,
        operation: "Erasing".into(),
    });

    for i in 0..sectors {
        let addr = (i * chip.sector_size) as u32;
        if let Err(e) = programmer.erase_sector(addr) {
            return CmdResult::err(format!("Erase error at 0x{:06X}: {}", addr, e));
        }

        let _ = app.emit("progress", ProgressInfo {
            current: i + 1,
            total: sectors,
            percent: ((i + 1) as f32 / sectors as f32) * 100.0,
            operation: "Erasing".into(),
        });
    }

    // Write data
    const PAGE_SIZE: usize = 256;
    let pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for i in 0..pages {
        let offset = i * PAGE_SIZE;
        let addr = offset as u32;
        let chunk_len = std::cmp::min(PAGE_SIZE, size - offset);

        if let Err(e) = programmer.program_page(addr, &data[offset..offset + chunk_len]) {
            return CmdResult::err(format!("Write error at 0x{:06X}: {}", addr, e));
        }

        let _ = app.emit("progress", ProgressInfo {
            current: i + 1,
            total: pages,
            percent: ((i + 1) as f32 / pages as f32) * 100.0,
            operation: "Writing".into(),
        });
    }

    // Verify if requested
    if verify {
        let _ = app.emit("progress", ProgressInfo {
            current: 0,
            total: size,
            percent: 0.0,
            operation: "Verifying".into(),
        });

        const CHUNK_SIZE: usize = 4096;
        let mut read_buf = vec![0u8; CHUNK_SIZE];
        let mut offset = 0;

        while offset < size {
            let chunk_len = std::cmp::min(CHUNK_SIZE, size - offset);

            if let Err(e) = programmer.read(offset as u32, &mut read_buf[..chunk_len]) {
                return CmdResult::err(format!("Verify read error at 0x{:06X}: {}", offset, e));
            }

            if read_buf[..chunk_len] != data[offset..offset + chunk_len] {
                return CmdResult::err(format!("Verification failed at 0x{:06X}", offset));
            }

            offset += chunk_len;

            let _ = app.emit("progress", ProgressInfo {
                current: offset,
                total: size,
                percent: (offset as f32 / size as f32) * 100.0,
                operation: "Verifying".into(),
            });
        }
    }

    CmdResult::ok(())
}

/// Erase entire chip
#[tauri::command]
fn erase_chip(
    state: State<'_, Arc<AppState>>,
    app: AppHandle,
) -> CmdResult<()> {
    let mut programmer_guard = state.programmer.lock();

    let programmer = match programmer_guard.as_mut() {
        Some(p) => p,
        None => return CmdResult::err("Not connected"),
    };

    let _ = app.emit("progress", ProgressInfo {
        current: 0,
        total: 1,
        percent: 0.0,
        operation: "Erasing chip...".into(),
    });

    if let Err(e) = programmer.erase_chip() {
        return CmdResult::err(format!("Erase failed: {}", e));
    }

    let _ = app.emit("progress", ProgressInfo {
        current: 1,
        total: 1,
        percent: 100.0,
        operation: "Erase complete".into(),
    });

    CmdResult::ok(())
}

/// Verify flash against file
#[tauri::command]
fn verify_flash(
    state: State<'_, Arc<AppState>>,
    app: AppHandle,
    path: String,
) -> CmdResult<bool> {
    let mut programmer_guard = state.programmer.lock();

    let programmer = match programmer_guard.as_mut() {
        Some(p) => p,
        None => return CmdResult::err("Not connected"),
    };

    // Read file
    let data = match std::fs::read(&path) {
        Ok(d) => d,
        Err(e) => return CmdResult::err(format!("Failed to read file: {}", e)),
    };

    let size = data.len();
    const CHUNK_SIZE: usize = 4096;
    let mut read_buf = vec![0u8; CHUNK_SIZE];
    let mut offset = 0;

    while offset < size {
        let chunk_len = std::cmp::min(CHUNK_SIZE, size - offset);

        if let Err(e) = programmer.read(offset as u32, &mut read_buf[..chunk_len]) {
            return CmdResult::err(format!("Read error at 0x{:06X}: {}", offset, e));
        }

        if read_buf[..chunk_len] != data[offset..offset + chunk_len] {
            return CmdResult::ok(false);
        }

        offset += chunk_len;

        let _ = app.emit("progress", ProgressInfo {
            current: offset,
            total: size,
            percent: (offset as f32 / size as f32) * 100.0,
            operation: "Verifying".into(),
        });
    }

    CmdResult::ok(true)
}

/// Get flash chip database
#[tauri::command]
fn get_chip_database() -> Vec<FlashChip> {
    get_flash_database()
}

/// List connected devices
#[tauri::command]
fn list_devices() -> CmdResult<Vec<DeviceInfo>> {
    match ch347::list_devices() {
        Ok(devices) => {
            let infos: Vec<DeviceInfo> = devices
                .into_iter()
                .map(|d| DeviceInfo {
                    connected: false,
                    vid: Some(d.vid),
                    pid: Some(d.pid),
                    name: Some(d.product),
                })
                .collect();
            CmdResult::ok(infos)
        }
        Err(e) => CmdResult::err(format!("Failed to list devices: {}", e)),
    }
}

// ============================================================================
// Tauri App Setup
// ============================================================================

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .manage(Arc::new(AppState::default()))
        .invoke_handler(tauri::generate_handler![
            connect,
            disconnect,
            is_connected,
            detect_chip,
            read_flash,
            write_flash,
            erase_chip,
            verify_flash,
            get_chip_database,
            list_devices,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
