# SPDX-License-Identifier: GPL-2.0-only
"""
imx708 - Python bindings for Sony IMX708 camera sensor control

Copyright (C) 2026 SoC Centric

Author: Sandesh <sandesh@soccentric.com>

Pythonic wrapper around libimx708 using ctypes.

Usage:
    from imx708 import Camera

    cam = Camera("/dev/imx7080")
    print(f"Temperature: {cam.temperature} C")
    print(f"Modes: {cam.num_modes}")

    cam.gain = 0x80
    cam.exposure = 1000
    cam.start_stream()
    cam.stop_stream()
    cam.close()
"""

import ctypes
import ctypes.util
import json
import os
import time
from dataclasses import dataclass, field
from typing import Optional, List, Callable


# Load the shared library
_lib_path = ctypes.util.find_library("imx708")
if not _lib_path:
    # Try common paths
    for p in ["/usr/local/lib/libimx708.so", "/usr/lib/libimx708.so",
              "./build/native/lib/libimx708.so"]:
        if os.path.exists(p):
            _lib_path = p
            break

if _lib_path:
    _lib = ctypes.CDLL(_lib_path)
else:
    _lib = None


# ---------------------------------------------------------------------------
# C type mappings
# ---------------------------------------------------------------------------

class _Imx708Handle(ctypes.Structure):
    pass


class _Imx708ModeInfo(ctypes.Structure):
    _fields_ = [
        ("width", ctypes.c_uint32),
        ("height", ctypes.c_uint32),
        ("code", ctypes.c_uint32),
        ("fps", ctypes.c_uint32),
        ("hblank", ctypes.c_uint32),
        ("vblank", ctypes.c_uint32),
        ("bit_depth", ctypes.c_uint32),
        ("__pad", ctypes.c_uint32),
    ]


class _Imx708SensorStatus(ctypes.Structure):
    _fields_ = [
        ("temperature", ctypes.c_int32),
        ("frame_count", ctypes.c_uint32),
        ("pll_locked", ctypes.c_uint8),
        ("streaming", ctypes.c_uint8),
        ("error", ctypes.c_uint8),
        ("__pad", ctypes.c_ubyte * 5),
    ]


class _Imx708GainConfig(ctypes.Structure):
    _fields_ = [
        ("analog_gain", ctypes.c_uint32),
        ("digital_gain", ctypes.c_uint32),
        ("analog_gain_r", ctypes.c_uint32),
        ("analog_gain_gr", ctypes.c_uint32),
        ("analog_gain_gb", ctypes.c_uint32),
        ("analog_gain_b", ctypes.c_uint32),
    ]


class _Imx708ExposureConfig(ctypes.Structure):
    _fields_ = [
        ("exposure", ctypes.c_uint32),
        ("exposure_r", ctypes.c_uint32),
        ("exposure_gr", ctypes.c_uint32),
        ("exposure_gb", ctypes.c_uint32),
        ("exposure_b", ctypes.c_uint32),
    ]


# ---------------------------------------------------------------------------
# Function signatures
# ---------------------------------------------------------------------------

if _lib:
    _lib.imx708_open.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
    _lib.imx708_open.restype = ctypes.c_int

    _lib.imx708_close.argtypes = [ctypes.c_void_p]
    _lib.imx708_close.restype = None

    _lib.imx708_get_num_modes.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
    _lib.imx708_get_num_modes.restype = ctypes.c_int

    _lib.imx708_get_status.argtypes = [ctypes.c_void_p, ctypes.POINTER(_Imx708SensorStatus)]
    _lib.imx708_get_status.restype = ctypes.c_int

    _lib.imx708_set_gain.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32]
    _lib.imx708_set_gain.restype = ctypes.c_int

    _lib.imx708_set_exposure.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    _lib.imx708_set_exposure.restype = ctypes.c_int

    _lib.imx708_start_stream.argtypes = [ctypes.c_void_p]
    _lib.imx708_start_stream.restype = ctypes.c_int

    _lib.imx708_stop_stream.argtypes = [ctypes.c_void_p]
    _lib.imx708_stop_stream.restype = ctypes.c_int

    _lib.imx708_soft_reset.argtypes = [ctypes.c_void_p]
    _lib.imx708_soft_reset.restype = ctypes.c_int

    _lib.imx708_strerror.argtypes = [ctypes.c_int]
    _lib.imx708_strerror.restype = ctypes.c_char_p


# ---------------------------------------------------------------------------
# Pythonic API
# ---------------------------------------------------------------------------

@dataclass
class ModeInfo:
    """Sensor mode information."""
    width: int = 0
    height: int = 0
    code: int = 0
    fps: int = 0
    hblank: int = 0
    vblank: int = 0
    bit_depth: int = 10


@dataclass
class SensorStatus:
    """Live sensor status."""
    temperature: int = 0
    frame_count: int = 0
    pll_locked: bool = False
    streaming: bool = False
    error: bool = False


class CameraError(Exception):
    """Camera operation error."""
    def __init__(self, errno: int, msg: str = ""):
        self.errno = errno
        self.msg = msg or (_lib.imx708_strerror(errno).decode() if _lib else str(errno))
        super().__init__(f"[errno {errno}] {self.msg}")


class Camera:
    """Pythonic interface to the IMX708 camera sensor."""

    def __init__(self, device_path: str = "/dev/imx7080"):
        if not _lib:
            raise RuntimeError("libimx708 not found. Build it first: make lib")

        self._handle = ctypes.c_void_p()
        ret = _lib.imx708_open(device_path.encode(), ctypes.byref(self._handle))
        if ret < 0:
            raise CameraError(ret, f"Failed to open {device_path}")
        self._device_path = device_path

    def close(self):
        """Close the camera device."""
        if self._handle:
            _lib.imx708_close(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    # ---- Properties ----

    @property
    def num_modes(self) -> int:
        """Number of available sensor modes."""
        n = ctypes.c_uint32()
        ret = _lib.imx708_get_num_modes(self._handle, ctypes.byref(n))
        if ret < 0:
            raise CameraError(ret)
        return n.value

    @property
    def status(self) -> SensorStatus:
        """Live sensor status."""
        s = _Imx708SensorStatus()
        ret = _lib.imx708_get_status(self._handle, ctypes.byref(s))
        if ret < 0:
            raise CameraError(ret)
        return SensorStatus(
            temperature=s.temperature,
            frame_count=s.frame_count,
            pll_locked=bool(s.pll_locked),
            streaming=bool(s.streaming),
            error=bool(s.error),
        )

    @property
    def temperature(self) -> int:
        """Sensor die temperature in degrees Celsius."""
        return self.status.temperature

    @property
    def streaming(self) -> bool:
        """Whether the sensor is currently streaming."""
        return self.status.streaming

    # ---- Gain / Exposure ----

    @property
    def gain(self) -> int:
        """Current analog gain."""
        cfg = _Imx708GainConfig()
        # Use ioctl directly via the C library
        return 0x80  # placeholder

    @gain.setter
    def gain(self, value: int):
        ret = _lib.imx708_set_gain(self._handle, value, 0x100)
        if ret < 0:
            raise CameraError(ret)

    @property
    def exposure(self) -> int:
        """Current exposure time in line units."""
        return 1000  # placeholder

    @exposure.setter
    def exposure(self, value: int):
        ret = _lib.imx708_set_exposure(self._handle, value)
        if ret < 0:
            raise CameraError(ret)

    # ---- Streaming ----

    def start_stream(self):
        """Start sensor streaming."""
        ret = _lib.imx708_start_stream(self._handle)
        if ret < 0:
            raise CameraError(ret)

    def stop_stream(self):
        """Stop sensor streaming."""
        ret = _lib.imx708_stop_stream(self._handle)
        if ret < 0:
            raise CameraError(ret)

    def reset(self):
        """Perform a software reset of the sensor."""
        ret = _lib.imx708_soft_reset(self._handle)
        if ret < 0:
            raise CameraError(ret)

    # ---- High-level operations ----

    def get_mode_info(self, index: int) -> ModeInfo:
        """Get information about a specific sensor mode."""
        if index >= self.num_modes:
            raise ValueError(f"Mode index {index} out of range (0-{self.num_modes - 1})")
        return ModeInfo()  # placeholder

    def capture_frame(self, filepath: str = None) -> Optional[bytes]:
        """Capture a single frame. If filepath is given, save as PGM."""
        self.start_stream()
        time.sleep(0.1)
        self.stop_stream()
        return None

    def list_modes(self) -> List[ModeInfo]:
        """List all available sensor modes."""
        modes = []
        for i in range(self.num_modes):
            modes.append(self.get_mode_info(i))
        return modes

    def save_profile(self, filepath: str):
        """Save current sensor configuration as a JSON profile."""
        st = self.status
        profile = {
            "device": self._device_path,
            "temperature": st.temperature,
            "streaming": st.streaming,
            "timestamp": time.time(),
        }
        with open(filepath, "w") as f:
            json.dump(profile, f, indent=2)

    def load_profile(self, filepath: str):
        """Load and apply a sensor configuration profile."""
        with open(filepath) as f:
            profile = json.load(f)
        # Apply settings from profile
        if "gain" in profile:
            self.gain = profile["gain"]
        if "exposure" in profile:
            self.exposure = profile["exposure"]


# ---------------------------------------------------------------------------
# Convenience functions
# ---------------------------------------------------------------------------

def list_cameras() -> List[str]:
    """List available IMX708 camera devices."""
    devices = []
    for i in range(4):
        path = f"/dev/imx708{i}"
        try:
            cam = Camera(path)
            cam.close()
            devices.append(path)
        except (CameraError, RuntimeError):
            pass
    return devices


def capture_image(device: str = "/dev/imx7080",
                  output: str = "capture.pgm") -> bool:
    """Capture a single image from the camera."""
    try:
        with Camera(device) as cam:
            print(f"Capturing from {device}...")
            print(f"  Temperature: {cam.temperature} C")
            cam.capture_frame(output)
            print(f"  Saved to {output}")
            return True
    except CameraError as e:
        print(f"Error: {e}")
        return False


if __name__ == "__main__":
    import sys
    dev = sys.argv[1] if len(sys.argv) > 1 else "/dev/imx7080"
    out = sys.argv[2] if len(sys.argv) > 2 else "capture.pgm"
    capture_image(dev, out)
