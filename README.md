# QuickCompress

QuickCompress is a Windows application that allows you to compress videos to a target size (e.g., 5MB, 10MB, 20MB) via a context menu in File Explorer.

## Features
- Compress videos to a specified size using FFmpeg.
- Adds context menu entries ("QuickCompress (5MB)", "QuickCompress (10MB)", "QuickCompress (20MB)") to File Explorer.
- Includes FFmpeg and FFprobe in the installer for offline use.

## Requirements
- Windows operating system.
- No internet connection required (FFmpeg and FFprobe are bundled in the installer).

## Installation
1. Download the latest release (`QuickCompressSetup.exe`) from the [Releases](https://github.com/muratcx/quickcompress/releases) page.
2. Run the installer as an administrator to install QuickCompress.
3. Right-click a video file in File Explorer and select "QuickCompress (X MB)" to compress it.
4. The compressed video (e.g., `video_compressed.mp4`) will appear in the same directory.

## Licensing
QuickCompress is licensed under the GNU General Public License v3.0 (GPLv3). The source code for QuickCompress is available in this repository (see `compress_video.c`).

### FFmpeg Licensing
QuickCompress uses FFmpeg, which is licensed under the GNU General Public License v3.0 (GPLv3). The FFmpeg binaries are bundled in the installer. The source code for FFmpeg is available at [https://github.com/FFmpeg/FFmpeg](https://github.com/FFmpeg/FFmpeg).

## Support
For issues or questions, please open an issue on this repository.